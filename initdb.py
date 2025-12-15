import sqlite3
import time
import stat

def create_test_db():
    try:
        # 1. Connect to the database
        conn = sqlite3.connect('test.db')
        c = conn.cursor()
        
        # IMPORTANT: Enable Foreign Key support (SQLite has this off by default)
        c.execute("PRAGMA foreign_keys = ON;")

        print("Database 'test.db' connection established.")

        # ---------------------------------------------------------
        # 2. Create the 'Parent' Table (Users)
        # ---------------------------------------------------------
        c.execute('''
            CREATE TABLE IF NOT EXISTS users (
                name TEXT,
                surname TEXT,
                email TEXT UNIQUE,
                PRIMARY KEY (name, surname)
            ) STRICT
        ''')
        print("Table 'users' created.")

        # ---------------------------------------------------------
        # 3. Create the 'Child' Table (Orders) with Foreign Key
        # ---------------------------------------------------------
        # The 'user_id' column references the 'id' column in the 'users' table.
        c.execute('''
            CREATE TABLE IF NOT EXISTS orders (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                product_name TEXT NOT NULL,
                price REAL,
                user_name TEXT NOT NULL,
                user_surname TEXT NOT NULL,
                FOREIGN KEY (user_name, user_surname) REFERENCES users (name, surname) ON DELETE CASCADE
            ) STRICT
        ''')
        print("Table 'orders' created (with Foreign Key).")

        c.execute('''
            CREATE TABLE IF NOT EXISTS history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                order_id INTEGER NOT NULL,
                FOREIGN KEY (order_id) REFERENCES orders (id) ON DELETE CASCADE
            ) STRICT
        ''')
        print("Table 'history' created (with Foreign Key).")

        # ---------------------------------------------------------
        # 4. Fill the tables with records
        # ---------------------------------------------------------
        
        # Insert Users
        users_data = [
            ('Alice', 'Smith', 'alice@example.com'),
            ('Bob', 'Jones', 'bob@example.com'),
            ('Charlie', 'Day', 'charlie@example.com'),
            ('Charlie', 'Kirk', 'a' * 500000000),
        ]
        c.executemany("INSERT INTO users (name, surname, email) VALUES (?, ?, ?)", users_data)
        
        # Insert Orders
        # We manually assign user_id 1 to Alice, 2 to Bob, etc.
        orders_data = [
            ('Laptop', 1200.00, 'Alice', 'Smith'),      # Order for Alice
            ('Mouse', 25.50, 'Alice', 'Smith'),         # Another order for Alice
            ('Monitor', 300.00, 'Bob', 'Jones'),      # Order for Bob
            ('Keyboard', 50.00, 'Charlie', 'Day')       # Order for Charlie
        ]
        c.executemany("INSERT INTO orders (product_name, price, user_name, user_surname) VALUES (?, ?, ?, ?)", orders_data)
        
        history_data = [
            (1,),
            (2,),
            (3,),
            (4,),
        ]
        c.executemany("INSERT INTO history (order_id) VALUES (?)", history_data)
        
        print("Records inserted successfully.")

        # ---------------------------------------------------------
        # 5. Commit and Close
        # ---------------------------------------------------------
        conn.commit()
        
        # Optional: Verify data by joining tables
        print("\n--- Verifying Data (Join Query) ---")
        c.execute('''
            SELECT users.name, orders.product_name, orders.price 
            FROM orders 
            JOIN users ON orders.user_name = users.name AND orders.user_surname = users.surname
        ''')
        rows = c.fetchall()
        for row in rows:
            print(f"User: {row[0]} | Bought: {row[1]} | Price: ${row[2]}")

    except sqlite3.Error as e:
        print(f"An error occurred: {e}")
    finally:
        if conn:
            conn.close()
            print("\nConnection closed.")

create_test_db()