import sqlite3
import time
import random
import string
import os

DB_NAME = "massive_legend.db"
ROWS_TO_GENERATE = 63_000  # 5 Milioni di righe
BATCH_SIZE = 1_000          # Commit ogni 100k righe (fondamentale per la velocità)

def get_random_string(length):
    return ''.join(random.choices(string.ascii_letters, k=length))

def forge_database():
    if os.path.exists(DB_NAME):
        os.remove(DB_NAME)
        print(f"[FORGE] Vecchio DB '{DB_NAME}' distrutto.")

    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()

    # --- OTTIMIZZAZIONI LEGGENDARIE ---
    # Queste impostazioni rendono la scrittura 100x più veloce.
    # Rischioso in produzione se salta la corrente, ma perfetto per generare dati di test.
    c.execute("PRAGMA synchronous = OFF;")
    c.execute("PRAGMA journal_mode = MEMORY;") 
    c.execute("PRAGMA cache_size = 100000;")
    
    print("[FORGE] Forgia accesa. Parametri ottimizzati.")

    # 1. Creazione Tabelle
    c.execute("""
        CREATE TABLE metrics (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            temperature REAL,
            status TEXT,
            timestamp INTEGER
        )
    """)
    
    c.execute("""
        CREATE TABLE heavy_files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            filename TEXT,
            content BLOB
        )
    """)
    
    conn.commit()
    print("[FORGE] Tabelle create.")

    # 2. Generazione dell'Esercito (5 Milioni di righe)
    print(f"[FORGE] Inizio generazione di {ROWS_TO_GENERATE} righe nella tabella 'metrics'...")
    start_time = time.time()

    # Usiamo una transazione unica per blocchi giganti
    c.execute("BEGIN TRANSACTION;")
    
    query = "INSERT INTO metrics (device_id, temperature, status, timestamp) VALUES (?, ?, ?, ?)"
    
    for i in range(1, ROWS_TO_GENERATE + 1):
        # Dati sintetici leggeri
        dev = f"DEV-{random.randint(1, 1000)}"
        temp = random.uniform(20.0, 90.0)
        stat = "OK" if temp < 80 else "WARNING"
        ts = int(time.time()) - i
        
        c.execute(query, (dev, temp, stat, ts))

        if i % BATCH_SIZE == 0:
            conn.commit()
            c.execute("BEGIN TRANSACTION;")
            print(f"    -> Forgiate {i} righe...", end='\r')

    conn.commit()
    elapsed = time.time() - start_time
    print(f"\n[FORGE] Fatto! {ROWS_TO_GENERATE} righe in {elapsed:.2f} secondi.")

    # 3. Generazione dei Giganti (BLOB per testare la Cache)
    print("[FORGE] Generazione BLOB giganti per testare l'allineamento della cache...")
    
    # Un file da 10MB
    blob_10mb = os.urandom(10 * 1024 * 1024) 
    c.execute("INSERT INTO heavy_files (filename, content) VALUES (?, ?)", ("big_10mb.bin", blob_10mb))
    
    # Un file testuale enorme (ripetitivo ma lungo) per testare 'cat'
    text_huge = "LEGENDARY " * (2 * 1024 * 1024) # Circa 20MB di testo
    c.execute("INSERT INTO heavy_files (filename, content) VALUES (?, ?)", ("log_huge.txt", text_huge))

    conn.commit()
    print("[FORGE] Giganti inseriti.")

    # Chiusura
    conn.close()
    
    size_mb = os.path.getsize(DB_NAME) / (1024 * 1024)
    print(f"\n[COMPLETE] Il database '{DB_NAME}' è pronto per la battaglia.")
    print(f"[STATS] Dimensione finale su disco: {size_mb:.2f} MB")

if __name__ == "__main__":
    forge_database()