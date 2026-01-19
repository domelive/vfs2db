#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
VFS2DB Stress Test Database Generator
=====================================

Questo script genera un database SQLite progettato per testare TUTTE le
funzionalità del driver VFS2DB in modo approfondito.

Features testate:
- Navigazione filesystem (molte tabelle, molti record)
- Tutti i tipi SQLite (TEXT, INTEGER, FLOAT, BLOB, NULL)
- Foreign Keys (semplici, composite, catene)
- Dati grandi (> BLOCK_SIZE per testare cache)
- Extended attributes (user.type)
- Read con offset
- Write operations
- Cache stress (LRU eviction)

Author: Claude (Stress Test Generator)
"""

import sqlite3
import random
import string
import os
import sys
from datetime import datetime, timedelta

# =============================================================================
# CONFIGURAZIONE - Modifica questi valori per controllare l'intensità del test
# =============================================================================

CONFIG = {
    # Numero di tabelle "normali" da generare
    "num_simple_tables": 5,
    
    # Record per tabella semplice
    "records_per_simple_table": 1000,
    
    # Tabelle per test FK
    "num_fk_chains": 3,  # Catene di FK (A -> B -> C -> D)
    "fk_chain_depth": 4,  # Profondità di ogni catena
    
    # Test dati grandi (per cache)
    "large_text_sizes": [
        64 * 1024,      # 64 KB
        256 * 1024,     # 256 KB (= BLOCK_SIZE)
        512 * 1024,     # 512 KB (> BLOCK_SIZE, 2 blocchi)
        1024 * 1024,    # 1 MB (4 blocchi)
    ],
    "num_large_records": 50,  # Record con dati grandi
    
    # Test BLOB
    "blob_sizes": [1024, 10240, 102400],  # 1KB, 10KB, 100KB
    "num_blob_records": 30,
    
    # Test valori NULL
    "null_percentage": 0.15,  # 15% dei valori nullable saranno NULL
    
    # Test molti attributi
    "max_columns_table": 50,  # Tabella con molte colonne
    
    # Seed per riproducibilità
    "random_seed": 42,
}

# Costanti dal driver (per riferimento)
BLOCK_SIZE = 256 * 1024  # 256 KB
CACHE_SIZE = 128 * 1024 * 1024  # 128 MB
CACHE_BLOCKS = CACHE_SIZE // BLOCK_SIZE  # 512 blocchi

# =============================================================================
# UTILITY FUNCTIONS
# =============================================================================

def random_string(length, charset=string.ascii_letters + string.digits):
    """Genera una stringa casuale della lunghezza specificata."""
    return ''.join(random.choice(charset) for _ in range(length))

def random_text(min_len=10, max_len=500):
    """Genera testo casuale con parole."""
    words = []
    current_len = 0
    target_len = random.randint(min_len, max_len)
    
    while current_len < target_len:
        word_len = random.randint(3, 12)
        words.append(random_string(word_len, string.ascii_lowercase))
        current_len += word_len + 1
    
    return ' '.join(words)

def random_blob(size):
    """Genera dati binari casuali."""
    return bytes(random.getrandbits(8) for _ in range(size))

def random_date(start_year=2000, end_year=2025):
    """Genera una data casuale."""
    start = datetime(start_year, 1, 1)
    end = datetime(end_year, 12, 31)
    delta = end - start
    random_days = random.randint(0, delta.days)
    return (start + timedelta(days=random_days)).strftime('%Y-%m-%d')

def random_email():
    """Genera un'email casuale."""
    domains = ['gmail.com', 'yahoo.com', 'outlook.com', 'example.org', 'test.net']
    name = random_string(random.randint(5, 12), string.ascii_lowercase)
    return f"{name}@{random.choice(domains)}"

def maybe_null(value, null_chance=CONFIG["null_percentage"]):
    """Ritorna None con una certa probabilità, altrimenti il valore."""
    return None if random.random() < null_chance else value

# =============================================================================
# DATABASE CREATION FUNCTIONS
# =============================================================================

def create_users_table(cursor):
    """
    Tabella users - base per molte FK
    Testa: INTEGER PK, TEXT, DATE, NULL values
    """
    print("  Creating 'users' table...")
    
    cursor.execute('''
        CREATE TABLE users (
            id INTEGER PRIMARY KEY,
            username TEXT NOT NULL UNIQUE,
            email TEXT,
            full_name TEXT,
            bio TEXT,
            created_at TEXT,
            age INTEGER,
            balance REAL,
            is_active INTEGER DEFAULT 1
        )
    ''')
    
    records = []
    for i in range(CONFIG["records_per_simple_table"]):
        records.append((
            f"user_{i:05d}",
            maybe_null(random_email()),
            maybe_null(f"{random_string(8)} {random_string(10)}"),
            maybe_null(random_text(50, 500)),
            random_date(),
            maybe_null(random.randint(18, 80)),
            maybe_null(round(random.uniform(0, 10000), 2)),
            random.choice([0, 1])
        ))
    
    cursor.executemany('''
        INSERT INTO users (username, email, full_name, bio, created_at, age, balance, is_active)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    ''', records)
    
    print(f"    Inserted {len(records)} users")

def create_categories_table(cursor):
    """
    Tabella categories - per FK gerarchiche (self-referencing)
    Testa: Self-referencing FK, gerarchie
    """
    print("  Creating 'categories' table...")
    
    cursor.execute('''
        CREATE TABLE categories (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            parent_id INTEGER,
            FOREIGN KEY (parent_id) REFERENCES categories(id)
        )
    ''')
    
    # Root categories
    root_categories = ['Electronics', 'Clothing', 'Books', 'Home', 'Sports']
    for i, name in enumerate(root_categories, 1):
        cursor.execute('''
            INSERT INTO categories (id, name, description, parent_id)
            VALUES (?, ?, ?, NULL)
        ''', (i, name, f"Main category for {name.lower()}", ))
    
    # Sub-categories
    sub_id = len(root_categories) + 1
    for parent_id in range(1, len(root_categories) + 1):
        for j in range(5):  # 5 subcategories per root
            cursor.execute('''
                INSERT INTO categories (id, name, description, parent_id)
                VALUES (?, ?, ?, ?)
            ''', (sub_id, f"Subcategory_{parent_id}_{j}", random_text(20, 100), parent_id))
            sub_id += 1
    
    print(f"    Inserted {sub_id - 1} categories (hierarchical)")

def create_products_table(cursor):
    """
    Tabella products - FK multiple
    Testa: Multiple FK alla stessa tabella (users), FK a categories
    """
    print("  Creating 'products' table...")
    
    cursor.execute('''
        CREATE TABLE products (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            price REAL NOT NULL,
            stock INTEGER DEFAULT 0,
            category_id INTEGER,
            created_by INTEGER,
            updated_by INTEGER,
            FOREIGN KEY (category_id) REFERENCES categories(id),
            FOREIGN KEY (created_by) REFERENCES users(id),
            FOREIGN KEY (updated_by) REFERENCES users(id)
        )
    ''')
    
    # Ottieni il numero di utenti e categorie
    cursor.execute("SELECT COUNT(*) FROM users")
    num_users = cursor.fetchone()[0]
    cursor.execute("SELECT COUNT(*) FROM categories")
    num_categories = cursor.fetchone()[0]
    
    records = []
    for i in range(CONFIG["records_per_simple_table"]):
        records.append((
            f"Product {i:05d} - {random_string(10)}",
            random_text(100, 1000),
            round(random.uniform(1, 1000), 2),
            random.randint(0, 1000),
            random.randint(1, num_categories),
            random.randint(1, num_users),
            random.randint(1, num_users)
        ))
    
    cursor.executemany('''
        INSERT INTO products (name, description, price, stock, category_id, created_by, updated_by)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    ''', records)
    
    print(f"    Inserted {len(records)} products")

def create_orders_table(cursor):
    """
    Tabella orders - FK a users
    Testa: FK semplici, DATE, stati
    """
    print("  Creating 'orders' table...")
    
    cursor.execute('''
        CREATE TABLE orders (
            id INTEGER PRIMARY KEY,
            user_id INTEGER NOT NULL,
            order_date TEXT NOT NULL,
            status TEXT DEFAULT 'pending',
            total_amount REAL,
            shipping_address TEXT,
            notes TEXT,
            FOREIGN KEY (user_id) REFERENCES users(id)
        )
    ''')
    
    cursor.execute("SELECT COUNT(*) FROM users")
    num_users = cursor.fetchone()[0]
    
    statuses = ['pending', 'confirmed', 'shipped', 'delivered', 'cancelled']
    
    records = []
    for i in range(CONFIG["records_per_simple_table"]):
        records.append((
            random.randint(1, num_users),
            random_date(2020, 2025),
            random.choice(statuses),
            round(random.uniform(10, 5000), 2),
            maybe_null(f"{random.randint(1, 999)} {random_string(10)} Street, {random_string(8)} City"),
            maybe_null(random_text(10, 200))
        ))
    
    cursor.executemany('''
        INSERT INTO orders (user_id, order_date, status, total_amount, shipping_address, notes)
        VALUES (?, ?, ?, ?, ?, ?)
    ''', records)
    
    print(f"    Inserted {len(records)} orders")

def create_order_items_table(cursor):
    """
    Tabella order_items - FK composite (orders + products)
    Testa: Multiple FK, junction table
    """
    print("  Creating 'order_items' table...")
    
    cursor.execute('''
        CREATE TABLE order_items (
            id INTEGER PRIMARY KEY,
            order_id INTEGER NOT NULL,
            product_id INTEGER NOT NULL,
            quantity INTEGER NOT NULL DEFAULT 1,
            unit_price REAL NOT NULL,
            discount REAL DEFAULT 0,
            FOREIGN KEY (order_id) REFERENCES orders(id),
            FOREIGN KEY (product_id) REFERENCES products(id)
        )
    ''')
    
    cursor.execute("SELECT COUNT(*) FROM orders")
    num_orders = cursor.fetchone()[0]
    cursor.execute("SELECT COUNT(*) FROM products")
    num_products = cursor.fetchone()[0]
    
    records = []
    for order_id in range(1, num_orders + 1):
        # Ogni ordine ha 1-5 items
        num_items = random.randint(1, 5)
        for _ in range(num_items):
            records.append((
                order_id,
                random.randint(1, num_products),
                random.randint(1, 10),
                round(random.uniform(5, 500), 2),
                maybe_null(round(random.uniform(0, 0.3), 2))
            ))
    
    cursor.executemany('''
        INSERT INTO order_items (order_id, product_id, quantity, unit_price, discount)
        VALUES (?, ?, ?, ?, ?)
    ''', records)
    
    print(f"    Inserted {len(records)} order items")

def create_large_data_table(cursor):
    """
    Tabella large_data - dati molto grandi per testare il caching
    Testa: Chunked reading, cache eviction, large TEXT
    """
    print("  Creating 'large_data' table (cache stress test)...")
    
    cursor.execute('''
        CREATE TABLE large_data (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            size_category TEXT,
            large_text TEXT,
            created_at TEXT
        )
    ''')
    
    records = []
    for i in range(CONFIG["num_large_records"]):
        # Cicla attraverso le diverse dimensioni
        size = CONFIG["large_text_sizes"][i % len(CONFIG["large_text_sizes"])]
        size_cat = f"{size // 1024}KB"
        
        # Genera testo grande
        large_text = random_string(size)
        
        records.append((
            f"large_record_{i:03d}_{size_cat}",
            size_cat,
            large_text,
            random_date()
        ))
        
        if (i + 1) % 10 == 0:
            print(f"    Generated {i + 1}/{CONFIG['num_large_records']} large records...")
    
    cursor.executemany('''
        INSERT INTO large_data (name, size_category, large_text, created_at)
        VALUES (?, ?, ?, ?)
    ''', records)
    
    print(f"    Inserted {len(records)} large data records")

def create_blob_table(cursor):
    """
    Tabella binary_data - dati BLOB
    Testa: SQLITE_BLOB type, binary data handling
    """
    print("  Creating 'binary_data' table (BLOB test)...")
    
    cursor.execute('''
        CREATE TABLE binary_data (
            id INTEGER PRIMARY KEY,
            filename TEXT NOT NULL,
            mime_type TEXT,
            file_size INTEGER,
            data BLOB,
            checksum TEXT
        )
    ''')
    
    mime_types = ['image/png', 'image/jpeg', 'application/pdf', 'application/octet-stream']
    
    records = []
    for i in range(CONFIG["num_blob_records"]):
        size = CONFIG["blob_sizes"][i % len(CONFIG["blob_sizes"])]
        blob_data = random_blob(size)
        
        records.append((
            f"file_{i:03d}.bin",
            random.choice(mime_types),
            size,
            blob_data,
            random_string(32)  # Fake checksum
        ))
    
    cursor.executemany('''
        INSERT INTO binary_data (filename, mime_type, file_size, data, checksum)
        VALUES (?, ?, ?, ?, ?)
    ''', records)
    
    print(f"    Inserted {len(records)} blob records")

def create_all_types_table(cursor):
    """
    Tabella all_types - tutti i tipi SQLite in una tabella
    Testa: getxattr user.type per ogni tipo
    """
    print("  Creating 'all_types' table (type testing)...")
    
    cursor.execute('''
        CREATE TABLE all_types (
            id INTEGER PRIMARY KEY,
            int_val INTEGER,
            real_val REAL,
            text_val TEXT,
            blob_val BLOB,
            null_val TEXT
        )
    ''')
    
    records = []
    for i in range(100):
        records.append((
            random.randint(-1000000, 1000000),
            random.uniform(-1000000, 1000000),
            random_text(10, 100),
            random_blob(random.randint(10, 100)),
            None  # Sempre NULL
        ))
    
    cursor.executemany('''
        INSERT INTO all_types (int_val, real_val, text_val, blob_val, null_val)
        VALUES (?, ?, ?, ?, ?)
    ''', records)
    
    print(f"    Inserted {len(records)} all_types records")

def create_many_columns_table(cursor):
    """
    Tabella many_columns - molte colonne
    Testa: readdir con molti file, schema complesso
    """
    print(f"  Creating 'many_columns' table ({CONFIG['max_columns_table']} columns)...")
    
    columns = ["id INTEGER PRIMARY KEY"]
    for i in range(CONFIG["max_columns_table"] - 1):
        col_type = random.choice(['INTEGER', 'REAL', 'TEXT'])
        columns.append(f"col_{i:03d} {col_type}")
    
    cursor.execute(f"CREATE TABLE many_columns ({', '.join(columns)})")
    
    # Inserisci alcuni record
    for _ in range(50):
        values = []
        for i in range(CONFIG["max_columns_table"] - 1):
            if i % 3 == 0:
                values.append(random.randint(0, 1000))
            elif i % 3 == 1:
                values.append(random.uniform(0, 1000))
            else:
                values.append(random_string(20))
        
        placeholders = ", ".join(["?"] * len(values))
        col_names = ", ".join([f"col_{i:03d}" for i in range(CONFIG["max_columns_table"] - 1)])
        cursor.execute(f"INSERT INTO many_columns ({col_names}) VALUES ({placeholders})", values)
    
    print(f"    Inserted 50 records with {CONFIG['max_columns_table']} columns each")

def create_fk_chain_tables(cursor):
    """
    Crea catene di FK: A -> B -> C -> D
    Testa: Risoluzione symlink in catena, FK profonde
    """
    print(f"  Creating FK chain tables ({CONFIG['num_fk_chains']} chains, depth {CONFIG['fk_chain_depth']})...")
    
    for chain_num in range(CONFIG["num_fk_chains"]):
        tables_in_chain = []
        
        for depth in range(CONFIG["fk_chain_depth"]):
            table_name = f"chain{chain_num}_level{depth}"
            tables_in_chain.append(table_name)
            
            if depth == 0:
                # Prima tabella della catena - niente FK
                cursor.execute(f'''
                    CREATE TABLE {table_name} (
                        id INTEGER PRIMARY KEY,
                        name TEXT NOT NULL,
                        value INTEGER
                    )
                ''')
            else:
                # Tabelle successive - FK alla precedente
                prev_table = tables_in_chain[depth - 1]
                cursor.execute(f'''
                    CREATE TABLE {table_name} (
                        id INTEGER PRIMARY KEY,
                        name TEXT NOT NULL,
                        parent_id INTEGER NOT NULL,
                        value INTEGER,
                        FOREIGN KEY (parent_id) REFERENCES {prev_table}(id)
                    )
                ''')
        
        # Popola le tabelle
        for depth, table_name in enumerate(tables_in_chain):
            for i in range(100):  # 100 record per livello
                if depth == 0:
                    cursor.execute(f'''
                        INSERT INTO {table_name} (name, value)
                        VALUES (?, ?)
                    ''', (f"{table_name}_record_{i}", random.randint(1, 1000)))
                else:
                    cursor.execute(f'''
                        INSERT INTO {table_name} (name, parent_id, value)
                        VALUES (?, ?, ?)
                    ''', (f"{table_name}_record_{i}", random.randint(1, 100), random.randint(1, 1000)))
    
    total_tables = CONFIG["num_fk_chains"] * CONFIG["fk_chain_depth"]
    print(f"    Created {total_tables} chain tables with FK relationships")

def create_composite_fk_table(cursor):
    """
    Tabella con FK composite (più colonne che puntano alle PK della stessa tabella)
    Testa: get_rowid_from_pks con multiple FK
    """
    print("  Creating 'composite_fk_test' tables...")
    
    # Tabella target con PK composita (simulata con UNIQUE)
    cursor.execute('''
        CREATE TABLE regions (
            id INTEGER PRIMARY KEY,
            country_code TEXT NOT NULL,
            region_code TEXT NOT NULL,
            name TEXT,
            UNIQUE(country_code, region_code)
        )
    ''')
    
    # Inserisci regioni
    regions_data = [
        ('IT', 'ER', 'Emilia-Romagna'),
        ('IT', 'LOM', 'Lombardia'),
        ('IT', 'VEN', 'Veneto'),
        ('US', 'CA', 'California'),
        ('US', 'NY', 'New York'),
        ('US', 'TX', 'Texas'),
        ('DE', 'BY', 'Bavaria'),
        ('DE', 'BE', 'Berlin'),
    ]
    cursor.executemany('''
        INSERT INTO regions (country_code, region_code, name)
        VALUES (?, ?, ?)
    ''', regions_data)
    
    # Tabella con FK alla region
    cursor.execute('''
        CREATE TABLE locations (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            region_id INTEGER,
            latitude REAL,
            longitude REAL,
            FOREIGN KEY (region_id) REFERENCES regions(id)
        )
    ''')
    
    cursor.execute("SELECT COUNT(*) FROM regions")
    num_regions = cursor.fetchone()[0]
    
    for i in range(200):
        cursor.execute('''
            INSERT INTO locations (name, region_id, latitude, longitude)
            VALUES (?, ?, ?, ?)
        ''', (
            f"Location_{i:03d}",
            random.randint(1, num_regions),
            random.uniform(-90, 90),
            random.uniform(-180, 180)
        ))
    
    print("    Created regions and locations tables")

def create_special_names_table(cursor):
    """
    Tabella con nomi di colonne "difficili"
    Testa: parsing dei path, caratteri speciali
    """
    print("  Creating 'special_names' table...")
    
    # SQLite permette questi nomi se quotati
    cursor.execute('''
        CREATE TABLE special_names (
            id INTEGER PRIMARY KEY,
            "Column With Spaces" TEXT,
            "column_with_underscore" TEXT,
            "MixedCaseColumn" TEXT,
            "123_starts_with_number" TEXT,
            "ALLCAPS" TEXT
        )
    ''')
    
    for i in range(50):
        cursor.execute('''
            INSERT INTO special_names 
            ("Column With Spaces", "column_with_underscore", "MixedCaseColumn", 
             "123_starts_with_number", "ALLCAPS")
            VALUES (?, ?, ?, ?, ?)
        ''', (
            random_text(10, 50),
            random_text(10, 50),
            random_text(10, 50),
            random_text(10, 50),
            random_text(10, 50)
        ))
    
    print("    Created special_names table with 50 records")

def create_empty_table(cursor):
    """
    Tabella vuota
    Testa: readdir su tabella senza record
    """
    print("  Creating 'empty_table' table...")
    
    cursor.execute('''
        CREATE TABLE empty_table (
            id INTEGER PRIMARY KEY,
            name TEXT,
            value INTEGER
        )
    ''')
    
    print("    Created empty_table (0 records)")

def create_single_record_table(cursor):
    """
    Tabella con un solo record
    Testa: edge case con record singolo
    """
    print("  Creating 'single_record' table...")
    
    cursor.execute('''
        CREATE TABLE single_record (
            id INTEGER PRIMARY KEY,
            important_data TEXT NOT NULL,
            timestamp TEXT
        )
    ''')
    
    cursor.execute('''
        INSERT INTO single_record (important_data, timestamp)
        VALUES (?, ?)
    ''', ("This is the only record in this table", datetime.now().isoformat()))
    
    print("    Created single_record table (1 record)")

# =============================================================================
# MAIN
# =============================================================================

def main():
    if len(sys.argv) < 2:
        db_path = "stress_test.db"
    else:
        db_path = sys.argv[1]
    
    # Rimuovi DB esistente
    if os.path.exists(db_path):
        os.remove(db_path)
        print(f"Removed existing database: {db_path}")
    
    print(f"\n{'='*60}")
    print("VFS2DB STRESS TEST DATABASE GENERATOR")
    print(f"{'='*60}")
    print(f"Database path: {db_path}")
    print(f"Random seed: {CONFIG['random_seed']}")
    print(f"{'='*60}\n")
    
    # Imposta seed per riproducibilità
    random.seed(CONFIG["random_seed"])
    
    # Connessione al database
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    # Abilita foreign keys
    cursor.execute("PRAGMA foreign_keys = ON")
    
    print("Creating tables...\n")
    
    try:
        # Tabelle base
        create_users_table(cursor)
        create_categories_table(cursor)
        create_products_table(cursor)
        create_orders_table(cursor)
        create_order_items_table(cursor)
        
        # Tabelle per test specifici
        create_large_data_table(cursor)
        create_blob_table(cursor)
        create_all_types_table(cursor)
        create_many_columns_table(cursor)
        
        # Tabelle per test FK
        create_fk_chain_tables(cursor)
        create_composite_fk_table(cursor)
        
        # Edge cases
        create_special_names_table(cursor)
        create_empty_table(cursor)
        create_single_record_table(cursor)
        
        # Commit
        conn.commit()
        
        print(f"\n{'='*60}")
        print("DATABASE SUMMARY")
        print(f"{'='*60}")
        
        # Statistiche
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
        tables = cursor.fetchall()
        
        total_records = 0
        for (table_name,) in tables:
            cursor.execute(f"SELECT COUNT(*) FROM [{table_name}]")
            count = cursor.fetchone()[0]
            total_records += count
            
            cursor.execute(f"PRAGMA table_info([{table_name}])")
            columns = len(cursor.fetchall())
            
            cursor.execute(f"PRAGMA foreign_key_list([{table_name}])")
            fks = len(cursor.fetchall())
            
            print(f"  {table_name:30s} | {count:6d} records | {columns:3d} cols | {fks:2d} FKs")
        
        print(f"{'='*60}")
        print(f"TOTAL: {len(tables)} tables, {total_records} records")
        
        # Dimensione file
        conn.close()
        file_size = os.path.getsize(db_path)
        print(f"Database size: {file_size / (1024*1024):.2f} MB")
        print(f"{'='*60}\n")
        
        print("Database generated successfully!")
        print(f"\nTo use with VFS2DB:")
        print(f"  ./vfs2db -f -o db={db_path} /path/to/mountpoint")
        
    except Exception as e:
        conn.rollback()
        print(f"\nERROR: {e}")
        raise
    finally:
        if conn:
            conn.close()

if __name__ == "__main__":
    main()
