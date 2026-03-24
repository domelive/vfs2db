/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   types.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Type definitions for database schema and related structures.
 * @date   Created on 2025-12-23
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TYPES_H
#define TYPES_H

#include "const.h"
#include "uthash.h"

// =============================================================
// Metadata Structures
// =============================================================

/**
 * @brief Structure representing a Primary Key attribute in a database schema.
 *
 * Includes the following fields:
 * - `name`: The name of the primary key attribute.
 * - `sqlite_type`: The SQLite data type of the primary key attribute.
 * - `hh`: UTHash handle for hash table operations.
 *
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid
 * memory leaks.
 */
typedef struct Pk {
    char*          name;
    int            sqlite_type;
    UT_hash_handle hh;
} Pk;

/**
 * @brief Structure representing a non-primary key attribute in a database schema.
 *
 * Includes the following fields:
 * - `name`: The name of the attribute.
 * - `sqlite_type`: The SQLite data type of the attribute.
 * - `hh`: UTHash handle for hash table operations.
 *
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid
 * memory leaks.
 */
typedef struct Attr {
    char*          name;
    int            sqlite_type;
    UT_hash_handle hh;
} Attr;

/**
 * @brief Structure representing a Foreign Key relationship in a database schema.
 *
 * Includes the following fields:
 * - `from`:      The attribute in the current table that is the foreign key.
 * - `table`:     The name of the referenced table.
 * - `to`:        The attribute in the referenced table that the foreign key points to.
 * - `hh`:        UTHash handle for hash table operations.
 *
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid
 * memory leaks.
 */
typedef struct Fk {
    char*          from;
    char*          table;
    char*          to;
    int            id;
    int            sqlite_type;
    UT_hash_handle hh;
} Fk;

/**
 * @brief Structure representing a database table schema.
 *
 * Includes the following fields:
 * - `name`: The name of the table.
 * - `pk_head`: Pointer to the head of a hashmap of `Pk` structures representing primary key
 * attributes.
 * - `attr_head`: Pointer to the head of a hashmap of `Attr` structures representing non-primary
 * key attributes.
 * - `fks_head`: Pointer to the head of a hashmap of `Fk` structures representing foreign key
 * relationships.
 * - `hh`: UTHash handle for hash table operations.
 *
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid
 * memory leaks.
 */
typedef struct Schema {
    char* name;
    char* sql;

    Pk*   pk_head;
    Attr* attr_head;
    Fk*   fks_head;

    UT_hash_handle hh;
} Schema;

// =============================================================
// Global Structures
// =============================================================

/**
 * @brief Structure representing the overall database schema.
 *
 * Includes the following fields:
 * - `tables_head`: Pointer to the head of a hashmap of `Schema` structures representing all
 * tables in the database.
 *
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid
 * memory leaks.
 */
typedef struct DbSchema {
    Schema* tables_head;
} DbSchema;

// =============================================================
// Context Structures
// =============================================================

/**
 * @brief Structure representing the context for the VFS2DB filesystem.
 *
 * Includes the following fields:
 * - `db_conn`: Pointer to the SQLite database connection.
 * - `db_schema`: Pointer to the in-memory representation of the database schema.
 * - `db_path`: The file path to the SQLite database.
 * - `foreign_keys_on`: Flag indicating whether foreign key constraints are enabled in the database.
 *
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid
 * memory leaks.
 */
typedef struct Vfs2DbContext {
    sqlite3*  db_conn;
    DbSchema* db_schema;

    const char* db_path;

    bool foreign_keys_on;
} Vfs2DbContext;

// =============================================================
// Other Structures
// =============================================================

// FIX: make it a type, like Tokens
struct tokens {
    char* table;
    char* record;
    char* attribute;
};

/**
 * @brief Structure representing tokens extracted from a file path for schema lookup.
 *
 * Includes the following fields:
 * - `column_name`: The name of the column (attribute) in the database schema.
 * - `column_type`: The declared type of the column in the database schema.
 * - `column_spec`: Additional specifications for the column (i.e. "PK" | "FK" | "ATTR").
 *
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid
 * memory leaks.
 */
typedef struct DotSchemaTokens {
    char* column_name;
    char* column_type;
    char* column_spec;
} DotSchemaTokens;

#endif // TYPES_H
