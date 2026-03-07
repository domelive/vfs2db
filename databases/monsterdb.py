import sqlite3
import time
import random
import string
import sys

# CONFIGURAZIONE TEST
DB_NAME = 'stress_test.db'
NUM_USERS = 10000         # Numero di utenti base
NUM_WIDE_ROWS = 100       # Righe nella tabella "larga"
CHAIN_DEPTH = 10          # Profondità delle tabelle a catena (FK)
BLOB_SIZE = 1 * 1024 * 1024 # 10 MB di dati binari per testare i limiti di I/O

def generate_random_string(length=10):
    return ''.join(random.choices(string.ascii_letters, k=length))

def create_complex_db():
    print(f"--- Inizio creazione Database Complesso: {DB_NAME} ---")
    start_time = time.time()
    
    conn = None
    try:
        conn = sqlite3.connect(DB_NAME)
        c = conn.cursor()

        # ---------------------------------------------------------
        # 0. OTTIMIZZAZIONE E CONFIGURAZIONE AVANZATA
        # ---------------------------------------------------------
        # Write-Ahead Logging per velocità in scrittura
        c.execute("PRAGMA journal_mode = WAL;") 
        # Sincronizzazione meno aggressiva (rischio corruzione se salta corrente, ma veloce)
        c.execute("PRAGMA synchronous = NORMAL;") 
        # Aumenta page size per gestire meglio BLOB grandi
        c.execute("PRAGMA page_size = 65536;")
        # Aumenta la cache
        c.execute("PRAGMA cache_size = -64000;") # 64MB cache
        # FONDAMENTALE: Attiva Foreign Keys
        c.execute("PRAGMA foreign_keys = ON;")
        
        print("Configurazione PRAGMA completata.")

        # ---------------------------------------------------------
        # 1. TABELLA UTENTI (Parent)
        # ---------------------------------------------------------
        c.execute('''
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uuid TEXT NOT NULL UNIQUE,
                username TEXT NOT NULL,
                email TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        ''')
        print("Tabella 'users' creata.")

        # ---------------------------------------------------------
        # 2. TABELLA AUTO-REFERENZIATA (Gerarchia/Albero)
        # Test: Recursive Queries & Self-Joins
        # ---------------------------------------------------------
        c.execute('''
            CREATE TABLE IF NOT EXISTS org_chart (
                employee_id INTEGER PRIMARY KEY,
                manager_id INTEGER,
                title TEXT,
                FOREIGN KEY (employee_id) REFERENCES users(id) ON DELETE CASCADE,
                FOREIGN KEY (manager_id) REFERENCES users(id) ON DELETE SET NULL
            );
        ''')
        print("Tabella 'org_chart' (gerarchica) creata.")

        # ---------------------------------------------------------
        # 3. STRESS TEST LIMITI COLONNE (Wide Table)
        # Test: Limite 2000 colonne di default
        # ---------------------------------------------------------
        # Creiamo dinamicamente una tabella con 1900 colonne
        num_cols = 1900
        cols_def = ", ".join([f"col_{i} INTEGER DEFAULT 0" for i in range(num_cols)])
        c.execute(f'''
            CREATE TABLE IF NOT EXISTS wide_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                {cols_def},
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            );
        ''')
        print(f"Tabella 'wide_metrics' creata con {num_cols} colonne extra.")

        # ---------------------------------------------------------
        # 4. TABELLA BLOB (Large Objects)
        # Test: Limiti dimensione file e allocazione pagine
        # ---------------------------------------------------------
        c.execute('''
            CREATE TABLE IF NOT EXISTS large_assets (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER NOT NULL,
                asset_name TEXT,
                payload BLOB, -- Dati binari massivi
                meta_text TEXT, -- Testo molto lungo
                FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
            ) STRICT;
        ''')
        print("Tabella 'large_assets' creata.")

        # ---------------------------------------------------------
        # 5. CATENA DI DIPENDENZE (FK Cascade Depth)
        # Test: Creiamo Tabelle T1 -> T2 -> T3 ... -> T10
        # Se cancello T1, deve cancellare a cascata fino a T10
        # ---------------------------------------------------------
        previous_table = "users"
        previous_col = "id"
        
        for i in range(1, CHAIN_DEPTH + 1):
            table_name = f"chain_level_{i}"
            c.execute(f'''
                CREATE TABLE IF NOT EXISTS {table_name} (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    parent_ref INTEGER NOT NULL,
                    info TEXT,
                    FOREIGN KEY (parent_ref) REFERENCES {previous_table}({previous_col}) ON DELETE CASCADE
                ) STRICT;
            ''')
            previous_table = table_name
            previous_col = "id"
        print(f"Catena di {CHAIN_DEPTH} tabelle vincolate creata.")

        conn.commit()
        print("\n--- Inizio Inserimento Dati (Batch) ---")

        # ---------------------------------------------------------
        # 6. POPOLAMENTO MASSIVO UTENTI
        # ---------------------------------------------------------
        users_data = []
        print(f"Generazione di {NUM_USERS} utenti in memoria...")
        for i in range(NUM_USERS):
            uuid_val = f"{generate_random_string(8)}-{generate_random_string(4)}-{i}"
            users_data.append((uuid_val, f"User_{i}", f"user{i}@test.com"))
        
        c.executemany("INSERT INTO users (uuid, username, email) VALUES (?, ?, ?)", users_data)
        print(f"Inseriti {NUM_USERS} utenti.")

        # ---------------------------------------------------------
        # 7. POPOLAMENTO WIDE TABLE
        # ---------------------------------------------------------
        # Inseriamo dati sparsi nella tabella gigante
        wide_rows = []
        for i in range(1, NUM_WIDE_ROWS + 1):
            # User ID ciclico (1...NUM_USERS)
            u_id = (i % NUM_USERS) + 1
            # Creiamo una lista con l'ID utente + 1900 zeri/uni random
            row_vals = [u_id] + [random.randint(0, 100) for _ in range(num_cols)]
            wide_rows.append(row_vals)
        
        # Costruzione stringa placeholders (?, ?, ?...)
        placeholders = ",".join(["?"] * (num_cols + 1))
        c.executemany(f"INSERT INTO wide_metrics (user_id, {', '.join([f'col_{j}' for j in range(num_cols)])}) VALUES ({placeholders})", wide_rows)
        print(f"Inserite {NUM_WIDE_ROWS} righe nella tabella larga (1900+ colonne).")

        # ---------------------------------------------------------
        # 8. POPOLAMENTO BLOB (Heavy I/O)
        # ---------------------------------------------------------
        print(f"Creazione payload BLOB di {BLOB_SIZE / 1024 / 1024:.2f} MB...")
        heavy_blob =  bytes('F' * BLOB_SIZE, "utf-8") # Genera byte
        long_text = 'A' * (BLOB_SIZE // 2) # Genera testo enorme
        
        c.execute("INSERT INTO large_assets (user_id, asset_name, payload, meta_text) VALUES (?, ?, ?, ?)", 
                  (1, "Massive File", heavy_blob, long_text))
        print("Inserito record con BLOB massivo.")

        # ---------------------------------------------------------
        # 9. POPOLAMENTO CATENA (Cascade Test Setup)
        # ---------------------------------------------------------
        # Inseriamo un record nel livello 1 collegato all'utente 1
        c.execute(f"INSERT INTO chain_level_1 (parent_ref, info) VALUES (?, ?)", (1, "Level 1 Start"))
        last_id = c.lastrowid
        
        # Propaghiamo fino al livello 10
        for i in range(2, CHAIN_DEPTH + 1):
            c.execute(f"INSERT INTO chain_level_{i} (parent_ref, info) VALUES (?, ?)", (last_id, f"Level {i} Info"))
            last_id = c.lastrowid
        print("Catena di dipendenze popolata.")

        conn.commit()

        # ---------------------------------------------------------
        # 10. VERIFICA E STRESS TEST CANCELLAZIONE
        # ---------------------------------------------------------
        print("\n--- Test Integrità ---")
        
        # Verifica dimensione DB
        c.execute("PRAGMA page_count;")
        pages = c.fetchone()[0]
        c.execute("PRAGMA page_size;")
        p_size = c.fetchone()[0]
        db_size = (pages * p_size) / (1024 * 1024)
        print(f"Dimensione attuale Database: {db_size:.2f} MB")

    except sqlite3.Error as e:
        print(f"ERRORE CRITICO SQLITE: {e}")
    except Exception as e:
        print(f"ERRORE GENERICO: {e}")
    finally:
        if conn:
            conn.close()
            end_time = time.time()
            print(f"\nConnessione chiusa. Tempo totale esecuzione: {end_time - start_time:.2f} secondi.")

if __name__ == "__main__":
    create_complex_db()