/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_schema.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  DbSchema Header File, defining the structure for representing the global database schema.
 * @date   Created on 2026-03-30
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

#ifndef DB_SCHEMA_H
#define DB_SCHEMA_H

#include "helpers.h"
#include "uthash.h"

typedef struct Schema Schema;

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

/**
 * remove_schema
 *
 * @brief Remove Schema from DbSchema
 *
 * Removes the specified `Schema` from the `DbSchema`'s hash table and frees its associated memory.
 *
 * @param[in] db_schema Pointer to the `DbSchema` containing the schema to be removed
 * @param[in] schema Pointer to the `Schema` to be removed from the `DbSchema`
 *
 * @note This function does not free the `DbSchema` itself, only the specified `Schema` and its
 * contents.
 */
void remove_schema(DbSchema* db_schema, Schema* schema);

/**
 * free_schema_hashmap
 *
 * @brief Free Schema Hashmap
 *
 * Frees all `Schema` structures in the `DbSchema`'s hash table and their associated memory, then
 * sets the `tables_head` pointer to NULL.
 *
 * @param[in] schema Pointer to the `DbSchema` whose schema hash map will be freed
 */
void free_schema_hashmap(DbSchema* schema);

#endif // DB_SCHEMA_H