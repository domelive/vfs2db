# vfs2db

**FUSE driver to navigate an SQLite database as if it were a filesystem.**

> Turns every table into a directory, every record into a subdirectory, and every attribute into a readable and writable file.

## Table of Contents

- [Motivations](#motivations)
- [Database-to-Filesystem Mapping](#database-to-filesystem-mapping)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Creating a Test Database](#creating-a-test-database)
- [Running the Driver](#running-the-driver)
- [Command-Line Options](#command-line-options)
- [Usage Examples](#usage-examples)
  - [Browsing tables](#browsing-tables)
  - [Listing records](#listing-records)
  - [Reading attributes](#reading-attributes)
  - [Writing attributes](#writing-attributes)
  - [Truncating attributes](#truncating-attributes)
  - [Foreign keys as symlinks](#foreign-keys-as-symlinks)
  - [Extended attributes](#extended-attributes)
  - [Using UNIX tools](#using-unix-tools)
- [Architecture](#architecture)
  - [Project structure](#project-structure)
  - [Modules](#modules)
  - [Implemented FUSE operations](#implemented-fuse-operations)
  - [Optimizations](#optimizations)
- [Unmounting the Filesystem](#unmounting-the-filesystem)
- [Known Bugs](#known-bugs)
- [License](#license)
- [Authors](#authors)

---

## Motivations

- **Legacy and compatibility**: Do you have an old application that only reads from text files? With this driver you can read data from a modern database without rewriting a single line of code.
- **Ease of use**: Navigate a database without learning SQL — just use `ls`, `cat`, `echo` and other terminal commands.
- **Scripting power**: Use bash, Python, grep, awk or sed directly on a database. For example, to search for a string across all records: `grep -r "error" /mnt/db/logs`.
- **Adaptability**: The driver exposes any table structure as a hierarchical filesystem view.

## Database-to-Filesystem Mapping

The driver maps the SQLite database structure into a three-level filesystem hierarchy:

```
/<mountpoint>/
├── <table_1>/                    →  Database table
│   ├── <rowid_1>/                →  Record (identified by rowid)
│   │   ├── <attribute_1>.vfs2db  →  Attribute (regular file, readable/writable)
│   │   ├── <attribute_2>.vfs2db  →  Attribute
│   │   └── <fk_attr>.vfs2db     →  Foreign key (symlink to the referenced record)
│   ├── <rowid_2>/
│   │   └── ...
│   └── ...
├── <table_2>/
│   └── ...
└── ...
```

| DB Concept         | Filesystem Representation             |
| ------------------ | ------------------------------------- |
| Table              | First-level directory                 |
| Record (row)       | Second-level directory (name = rowid) |
| Attribute (column) | `.vfs2db` file (regular file)         |
| Primary key        | `.vfs2db` file (regular file)         |
| Foreign key        | `.vfs2db` file (symlink)              |

## Prerequisites

- **Operating system**: Linux
- **C compiler**: GCC or Clang with C11 support
- **CMake** >= 3.15
- **FUSE 3** (`libfuse3-dev` on Debian/Ubuntu, `fuse3` on Arch)
- **SQLite 3** (`libsqlite3-dev` on Debian/Ubuntu, `sqlite` on Arch)
- **pkg-config**

### Installing dependencies

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libfuse3-dev libsqlite3-dev
```

On Arch Linux:

```bash
sudo pacman -S base-devel cmake pkgconf fuse3 sqlite
```

## Building

The project uses CMake as its build system.

```bash
# From the project root
mkdir -p build && cd build
cmake ..
make
```

The executable is generated at `build/src/vfs2db`.

## Creating a Test Database

The project includes two Python scripts to create example databases:

### Simple database with foreign keys

```bash
python3 initdb.py
```

Creates `test.db` containing three tables linked by foreign keys:

- `users` (name, surname, email) — composite primary key (name, surname)
- `orders` (id, product_name, price, user_name, user_surname) — FK to `users`
- `history` (id, order_id) — FK to `orders`

## Running the Driver

```bash
# Create the mount point
mkdir -p /tmp/test

# Start the driver in foreground
./build/src/vfs2db -f -o db=test.db /tmp/test
```

The `-f` flag keeps the process in the foreground (useful for debugging). Without `-f`, the driver runs as a background daemon.

### Full example

```bash
# 1. Create the database
python3 initdb.py

# 2. Create the mount point
mkdir -p /tmp/test

# 3. Start the driver
./build/src/vfs2db -f -o db=test.db /tmp/test
```

## Command-Line Options

The driver accepts the following options, passed via `-o`:

| Option                 | Description                                                   | Required | Default |
| ---------------------- | ------------------------------------------------------------- | :------: | ------- |
| `db=<path>`            | Path to the SQLite database file                              |   Yes    | —       |
| `log=<level>`          | Log level: `trace`, `debug`, `info`, `warn`, `error`, `fatal` |    No    | `info`  |
| `logfile=<path>`       | Write logs to a file in addition to stderr                    |    No    | —       |
| `cache_enabled=<0\|1>` | Enable (`1`) or disable (`0`) the LRU cache                   |    No    | `1`     |

All standard FUSE options are also supported (e.g. `-f` for foreground, `-d` for FUSE debug).

### Startup examples

```bash
# Basic startup
./build/src/vfs2db -f -o db=test.db /tmp/test

# With verbose logging
./build/src/vfs2db -f -o db=test.db -o log=trace /tmp/test

# With file logging
./build/src/vfs2db -f -o db=test.db -o log=debug -o logfile=/tmp/vfs2db.log /tmp/test

# Without cache
./build/src/vfs2db -f -o db=test.db -o cache_enabled=0 /tmp/test
```

## Usage Examples

All examples below assume the driver has been mounted with:

```bash
python3 initdb.py
mkdir -p /tmp/test
./build/src/vfs2db -f -o db=test.db /tmp/test
```

### Browsing tables

```bash
$ ls /tmp/test/
history  orders  users
```

Each directory corresponds to a database table.

### Listing records

```bash
$ ls /tmp/test/users/
1  2  3  4

$ ls /tmp/test/orders/
1  2  3  4
```

Each subdirectory corresponds to a record, identified by its SQLite `rowid`.

### Reading attributes

```bash
# Read a user's name
$ cat /tmp/test/users/1/name.vfs2db
Alice

# Read the surname
$ cat /tmp/test/users/1/surname.vfs2db
Smith

# Read the email
$ cat /tmp/test/users/1/email.vfs2db
alice@example.com

# List all attributes of a record
$ ls /tmp/test/orders/1/
id.vfs2db  price.vfs2db  product_name.vfs2db  user_name.vfs2db  user_surname.vfs2db

# Read an order's price
$ cat /tmp/test/orders/1/price.vfs2db
1200.0

# Read the product
$ cat /tmp/test/orders/1/product_name.vfs2db
Laptop
```

### Writing attributes

```bash
# Update a user's email
$ echo -n "newemail@example.com" > /tmp/test/users/1/email.vfs2db

# Verify the change
$ cat /tmp/test/users/1/email.vfs2db
newemail@example.com

# Update an order's price
$ echo -n "999.99" > /tmp/test/orders/1/price.vfs2db

$ cat /tmp/test/orders/1/price.vfs2db
999.99
```

> **Note**: Use `echo -n` (no trailing newline) to avoid writing a newline character into the attribute value.

### Truncating attributes

```bash
# Clear an attribute value (sets it to NULL in the database)
$ truncate -s 0 /tmp/test/users/1/email.vfs2db

$ cat /tmp/test/users/1/email.vfs2db
# (empty output, value is NULL)
```

### Foreign keys as symlinks

Columns that are foreign keys are displayed as symbolic links pointing to the referenced record in the target table.

```bash
# In the orders table, user_name and user_surname are FKs to users
$ ls -l /tmp/test/orders/1/
total 0
-rw-r--r-- 1 user user  1 ... id.vfs2db
-rw-r--r-- 1 user user  6 ... price.vfs2db
-rw-r--r-- 1 user user  6 ... product_name.vfs2db
lrwxrwxrwx 1 user user  0 ... user_name.vfs2db -> ../../users/1/name.vfs2db
lrwxrwxrwx 1 user user  0 ... user_surname.vfs2db -> ../../users/1/surname.vfs2db

# Following the symlink reads the value from the referenced table
$ cat /tmp/test/orders/1/user_name.vfs2db
Alice

# Nested FKs: history.order_id points to orders
$ ls -l /tmp/test/history/1/
total 0
-rw-r--r-- 1 user user  1 ... id.vfs2db
lrwxrwxrwx 1 user user  0 ... order_id.vfs2db -> ../../orders/1/id.vfs2db

$ cat /tmp/test/history/1/order_id.vfs2db
1
```

### Extended attributes

The driver exposes the `user.type` extended attribute for every file, indicating the SQLite type of the value stored in the attribute.

```bash
$ getfattr -n user.type /tmp/test/users/1/name.vfs2db
# user.type="TEXT"

$ getfattr -n user.type /tmp/test/orders/1/price.vfs2db
# user.type="FLOAT"

$ getfattr -n user.type /tmp/test/orders/1/id.vfs2db
# user.type="INTEGER"
```

Possible types are: `TEXT`, `INTEGER`, `FLOAT`, `BLOB`, `NULL`.

### Using UNIX tools

One of the main advantages of the driver is the ability to use standard command-line tools on a database:

```bash
# Find all users with a gmail email
$ grep -rl "gmail" /tmp/test/users/*/email.vfs2db

# Count the number of records in a table
$ ls /tmp/test/orders/ | wc -l
4

# Find all orders with price greater than 100
$ for d in /tmp/test/orders/*/; do
    price=$(cat "${d}price.vfs2db" 2>/dev/null)
    name=$(cat "${d}product_name.vfs2db" 2>/dev/null)
    if [ "$(echo "$price > 100" | bc)" -eq 1 ] 2>/dev/null; then
        echo "$name: $price"
    fi
  done

# List all user names
$ for d in /tmp/test/users/*/; do
    cat "${d}name.vfs2db"
    echo
  done

# Export an entire table as CSV
$ for d in /tmp/test/users/*/; do
    name=$(cat "${d}name.vfs2db" 2>/dev/null)
    surname=$(cat "${d}surname.vfs2db" 2>/dev/null)
    email=$(cat "${d}email.vfs2db" 2>/dev/null)
    echo "${name},${surname},${email}"
  done

# Use find to discover all database files
$ find /tmp/test -name "*.vfs2db" -type f
```

## Architecture

### Project structure

```
vfs2db/
├── CMakeLists.txt            # Main build system
├── initdb.py                 # Script to create a test database
├── generate_stress_db.py     # Script to create a stress test database
├── run.sh                    # Script to build and launch the driver
├── LICENSE                   # GNU GPL v3
├── README.md
├── TODO.md
└── src/
    ├── CMakeLists.txt        # Executable build system
    ├── main.c                # Entry point and option parsing
    └── modules/
        ├── arena/            # Thread-local memory arena allocator
        ├── cache_manager/    # LRU cache for query results
        ├── db_handler/       # SQLite database access logic
        ├── logger/           # Configurable, thread-safe logging system
        ├── query_manager/    # SQL query management and preparation
        ├── syscall_handler/  # FUSE operations implementation
        └── utils/            # Constants, types, error macros, and helpers
```

### Modules

| Module              | Description                                                                                                                                                                                                                                           |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **syscall_handler** | Implements the FUSE callbacks (`getattr`, `readdir`, `read`, `write`, `truncate`, `readlink`, etc.). Handles path parsing in the form `/table/record/attribute.vfs2db` and delegates database operations to the `db_handler` module.                  |
| **db_handler**      | Contains all database access logic: schema initialization, attribute read/write, rowid management, foreign key resolution, and cache interaction.                                                                                                     |
| **query_manager**   | Manages a catalog of SQL queries (static and dynamic) with prepared statements to prevent SQL injection and improve performance.                                                                                                                      |
| **cache_manager**   | Implements an LRU (Least Recently Used) cache backed by a hashmap (uthash) to store query results. Supports hit, miss, and eviction metrics. Configurable size (default: 128 MB, 256 KB blocks).                                                      |
| **arena**           | Thread-local memory arena allocator. Each FUSE thread has its own arena (default: 5 MB) that is reset at the beginning of every operation, avoiding repeated `malloc`/`free` calls for temporary allocations (tokens, strings, auxiliary structures). |
| **logger**          | Thread-safe logging system with six severity levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL). Supports timestamps, source location (file, line, function), thread ID, and colored output. Configurable at runtime.                                    |
| **utils**           | Constant definitions, types (schema, PK, FK, attributes with uthash hashmaps), error handling macros (`TRY`, `TRY_NOT_NULL`, `TRY_SQLITE`), and helper functions for in-memory schema management.                                                     |

### Implemented FUSE operations

| Operation  |   Status    | Description                                                                   |
| ---------- | :---------: | ----------------------------------------------------------------------------- |
| `getattr`  | Implemented | Returns file and directory attributes. FKs are marked as symlinks.            |
| `getxattr` | Implemented | Returns the `user.type` extended attribute (SQLite type of the attribute).    |
| `readdir`  | Implemented | Lists tables (level 0), records (level 1), or attributes (level 2).           |
| `read`     | Implemented | Reads an attribute value from the database, with offset and chunking support. |
| `write`    | Implemented | Writes/updates an attribute value in the database.                            |
| `truncate` | Implemented | Truncates an attribute value (sets to NULL for size 0).                       |
| `readlink` | Implemented | Resolves FK symlinks to the referenced record path.                           |
| `open`     | Implemented | Handles file opening (supports `O_TRUNC`).                                    |
| `init`     | Implemented | Initializes the DB connection, query manager, and loads the schema.           |
| `destroy`  | Implemented | Releases all resources on shutdown.                                           |
| `mkdir`    | Placeholder | Table/record creation (not yet implemented).                                  |
| `create`   | Placeholder | File/attribute creation (not yet implemented).                                |

### Optimizations

- **LRU Cache**: Query results are stored in a hashmap-backed LRU cache. When an attribute is read multiple times, subsequent reads are served from the cache without querying the database. The cache is automatically invalidated on writes. Default size is 128 MB split into 256 KB blocks.
- **Memory Arena**: Each FUSE thread has a thread-local memory arena (5 MB by default). At the start of every FUSE operation, the arena is reset in O(1), avoiding hundreds of `malloc`/`free` calls for temporary allocations (tokens, strings, auxiliary structures).
- **Schema Hashmap**: The database schema (tables, PKs, FKs, attributes) is loaded once at startup and kept in O(1) hashmaps for fast lookups during filesystem operations.
- **Prepared Statements**: Static SQL queries are prepared once at startup through the query manager, avoiding repeated parsing overhead.

## Unmounting the Filesystem

```bash
# Unmount the filesystem
fusermount3 -u /tmp/test

# Or, if the driver is running in the foreground, press Ctrl+C
```

## Known Bugs

- `set_attribute_empty` function does not work for `NOT NULL` attributes.
- `vfs2db_readlink` syscall does not work for tables with multiple FKs pointing to the same PK in another table.
- `memory_arena` module does not support allocations larger than the arena size (5 MB by default).

## License

This project is released under the [GNU General Public License v3.0](LICENSE).

## Authors

- **Domenico Livera** — domenico.livera@gmail.com
- **Nicola Travaglini** — nicola1.travaglini@gmail.com
