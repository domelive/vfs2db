# vfs2db

**A FUSE driver that lets you mount and navigate an SQLite database as a standard UNIX filesystem.**

> _Turns every table into a directory, every record into a subdirectory, and every attribute into a readable and writable file._

---

## Table of Contents

- [Overview](#overview)
- [How It Works](#how-it-works)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Build Instructions](#build-instructions)
- [Usage Summary](#usage-summary)
  - [The Power of UNIX Tools](#the-power-of-unix-tools)
- [Architecture & Performance](#architecture--performance)
- [Limitations](#limitations)
- [Known Bugs](#known-bugs)
- [License & Authors](#license--authors)

---

## Overview

**vfs2db** provides a highly adaptable filesystem interface to SQLite databases.

- **Legacy Compatibility:** Run old applications that expect text files directly against a modern SQLite database—zero code changes required.
- **Terminal-Friendly:** Browse and manipulate data without writing a single line of SQL using standard terminal commands (`ls`, `cat`, `grep`, `echo`).
- **Deep Integrations:** Write bash/Python scripts or use `sed`/`awk` directly on your database structure.

---

## How It Works

The driver dynamically maps your SQLite schema into a 3-tier hierarchical filesystem:

```text
/<mountpoint>/
├── <table_name>/                 → Directory (Table)
│   ├── <rowid>/                  → Subdirectory (Record)
│   │   ├── <column_A>.vfs2db     → Regular File (Readable/Writable)
│   │   ├── <column_B>.vfs2db     → Regular File (Primary Key)
│   │   └── <fk_column>.vfs2db    → Symbolic Link (Foreign Key)
```

**Extended Attributes:** You can even use `getfattr -n user.type <file>` to inspect the SQLite native datatype (`TEXT`, `INTEGER`, `FLOAT`, `BLOB`, `NULL`) for any given file!

---

## Getting Started

### Prerequisites

You'll need a Linux environment equipped with:

- **C Compiler** (GCC or Clang with C11 support)
- **CMake** (>= 3.15) and **pkg-config**
- **FUSE 3** (`libfuse3-dev` / `fuse3`)
- **SQLite 3** (`libsqlite3-dev` / `sqlite`)

### Build Instructions

The project uses CMake. From the root directory:

```bash
mkdir -p build && cd build
cmake ..
make
```

### Running the Driver

You can spin up a quick test database using the provided python script, then mount it:

```bash
# 1. Create a test database
python3 initdb.py

# 2. Create a mount point
mkdir -p /tmp/mydb

# 3. Mount in foreground mode
./build/src/vfs2db -f -o db=test.db /tmp/mydb
```

_To unmount, simply `Ctrl+C` if running in foreground, or run `fusermount3 -u /tmp/mydb`._

---

## Usage Summary

Once mounted, interacting with your database is as easy as managing files.

- **Read an attribute:** `cat /tmp/mydb/users/1/name.vfs2db`
- **Update an attribute:** `echo -n "new_email@example.com" > /tmp/mydb/users/1/email.vfs2db` (Always use `-n` to prevent unwanted newlines)
- **Nullify an attribute:** `truncate -s 0 /tmp/mydb/users/1/email.vfs2db`
- **Follow a Foreign Key:** Foreign keys act as symlinks mapping directly to their target record's folder.

### Explorable Demo Scripts

The `demos/` folder contains script examples illustrating practical applications:

- **`demo_intermediate.sh`**: Shows how to script targeted database writes. E.g., querying for `@gmail.com` users, checking if their order price is over a threshold, and using `echo` to apply a 10% discount.
- **`demo_complex_query.sh`**: Demonstrates "JOIN" equivalents in bash. It recursively greps for a product name, follows foreign key symlinks using `readlink` to reach the user directory, and filters users based on string patterns against their 'surname.vfs2db'.
- **`demo_advanced.sh`**: Showcases the "legacy compatibility" angle. By soft-linking `.vfs2db` driver files to paths hardcoded in old shell scripts, legacy processes can continuously write values to what they think are standard text files, automatically updating the underlying database in real-time.

### The Power of UNIX Tools

Because your database is now a filesystem, command-line integrations are practically limitless:

```bash
# Count records in 'orders' table
ls /tmp/mydb/orders/ | wc -l

# Search for "error" across all records in 'logs' table
grep -rl "error" /tmp/mydb/logs/

# Find all database values matching a specific type
find /tmp/mydb -name "*.vfs2db" -type f
```

---

## Architecture & Performance

The `src/modules/` directory breaks down the codebase logically:

- **`syscall_handler`**: Implements the native FUSE callbacks translating file paths into SQLite concepts.
- **`db_handler`**: Central hub for executing DB transactions and strict schema validation.
- **`query_manager`**: Prepares and catalogs static/dynamic SQL statements securely, eliminating overhead and SQL injection vectors.
- **Performance Systems**:
  - **Memory Arena:** Provides thread-local, O(1) resets for temporary allocations (eliminating repeated `malloc`/`free` loops within FUSE ops).

---

## Limitations

- `set_attribute_empty` function does not work for `NOT NULL` attributes.
- The driver cannot handle adding or changing any constraint.
- The driver cannot handle database views.

## Known Bugs

- when you modify a FK with `ln -s target linkpath.vfs2db.lnk`, it works but it tells you that the command failed.

---

## License & Authors

**License:** GNU General Public License v3.0

**Authors:**

- Domenico Livera — domenico.livera@gmail.com
- Nicola Travaglini — nicola1.travaglini@gmail.com
