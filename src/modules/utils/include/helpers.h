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

#ifndef HELPERS_H
#define HELPERS_H

#include "uthash.h"
#include "types.h"

// =============================================================
// Helper Functions for DbSchema and Schema
// =============================================================

/**
 * @brief Retrieve Schema by Name
 * 
 * @param[in] db_schema Pointer to the DbSchema structure containing all table schemas
 * @param[in] name      The name of the table whose schema is to be retrieved
 * 
 * @return Pointer to the Schema structure if found, NULL otherwise
 * 
 */
static inline Schema* find_schema_by_name(DbSchema* db_schema, const char* name) {
    Schema* s;
    HASH_FIND_STR(db_schema->tables_head, name, s);
    return s;
}

/**
 * @brief Add Schema to DbSchema
 * 
 * @param[in,out] db_schema     Pointer to the DbSchema structure to which the schema will be added
 * @param[in]     table_schema  Pointer to the Schema structure to add
 * 
 */
static inline void add_schema(DbSchema* db_schema, Schema* table_schema) {
    Schema* existing_schema = find_schema_by_name(db_schema, table_schema->name);
    if (existing_schema != NULL) {
        return;
    }
    HASH_ADD_STR(db_schema->tables_head, name, table_schema);
}

/**
 * @brief Count the number of schemas in DbSchema
 * 
 * @param[in] db_schema Pointer to the DbSchema structure containing all table schemas
 * 
 * @return The number of schemas in the DbSchema
 * 
 */
static inline int count_schemas(DbSchema* db_schema) {
    return HASH_COUNT(db_schema->tables_head);
}

#endif // HELPERS_H