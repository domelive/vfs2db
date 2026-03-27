import sqlite3
import os

DB_NAME = "demo_db.db"

def create_db():
    # Rimuove il db precedente se esiste per avere una demo pulita
    if os.path.exists(DB_NAME):
        os.remove(DB_NAME)

    # Connessione al database
    conn = sqlite3.connect(DB_NAME)
    cursor = conn.cursor()

    # Abilita il supporto alle foreign keys
    cursor.execute("PRAGMA foreign_keys = ON;")

    # Creazione tabella USERS con singola Primary Key (name)
    cursor.execute('''
    CREATE TABLE users (
        name TEXT PRIMARY KEY,
        surname TEXT,
        email TEXT
    )
    ''')

    # Creazione tabella ORDERS con singola Foreign Key verso users (name)
    cursor.execute('''
    CREATE TABLE orders (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        product_name TEXT,
        price REAL,
        user_name TEXT,
        FOREIGN KEY (user_name) REFERENCES users (name)
    )
    ''')

    # --- INSERIMENTO DATI STRATEGICI PER LA DEMO ---
    # Nota: I nomi ora devono essere univoci, ma nel nostro caso lo sono già.
    users_data = [
        ('Alice', 'Smith', 'alice@gmail.com'),
        ('Bob', 'Stark', 'bob@yahoo.com'),
        ('Charlie', 'Brown', 'charlie.br@gmail.com'),
        ('Diana', 'Spencer', 'diana@hotmail.com')
    ]
    cursor.executemany('INSERT INTO users (name, surname, email) VALUES (?, ?, ?)', users_data)

    # Ordini strategici: il cognome non serve più come riferimento
    orders_data = [
        ('MacBook Pro 14', 1200.00, 'Alice'),
        ('Mouse Wireless', 25.50, 'Alice'),
        ('iPhone 15 Pro', 999.00, 'Bob'),
        ('Monitor 4K', 600.00, 'Charlie'),
        ('iPad Pro', 850.00, 'Diana'),
        ('Cavo USB-C', 15.00, 'Charlie')
    ]
    cursor.executemany('INSERT INTO orders (product_name, price, user_name) VALUES (?, ?, ?)', orders_data)

    # Commit e chiusura
    conn.commit()
    conn.close()

    print(f"Database '{DB_NAME}' generato con successo e pronto per la demo!")

if __name__ == "__main__":
    create_db()
