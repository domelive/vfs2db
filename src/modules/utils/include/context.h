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

#ifndef CONTEXT_H
#define CONTEXT_H

#include "const.h"
#include "db_schema.h"

#include <sqlite3.h>

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

    int schema_version;
} Vfs2DbContext;

#endif // CONTEXT_H
