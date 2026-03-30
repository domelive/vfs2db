/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   schema.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Schema Header File, defining the structure and functions for managing database schemas.
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

#ifndef SCHEMA_H
#define SCHEMA_H

#include "attribute.h"
#include "foreign_key.h"
#include "helpers.h"
#include "primary_key.h"
#include "uthash.h"

typedef struct DbSchema DbSchema;

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

/**
 * find_schema_by_name
 *
 * @brief Retrieve Schema by Name
 *
 * @param[in] db_schema Pointer to the DbSchema structure containing all table schemas
 * @param[in] name      The name of the table whose schema is to be retrieved
 *
 * @return Pointer to the Schema structure if found, NULL otherwise
 */
Schema* find_schema_by_name(DbSchema* db_schema, const char* name);

/**
 * add_schema
 *
 * @brief Add Schema to DbSchema
 *
 * @param[in,out] db_schema     Pointer to the DbSchema structure to which the schema will be added
 * @param[in]     table_schema  Pointer to the Schema structure to add
 *
 * @todo consider adding the TRY macro to handle error cases, such as memory allocation failure or
 * duplicate schema name.
 */
void add_schema(DbSchema* db_schema, Schema* table_schema);

/**
 * count_schemas
 *
 * @brief Count the number of schemas in DbSchema
 *
 * @param[in] db_schema Pointer to the DbSchema structure containing all table schemas
 *
 * @return The number of schemas in the DbSchema
 */
int count_schemas(DbSchema* db_schema);

/**
 * free_schema_content
 *
 * @brief Free all content of a Schema, including primary keys, attributes, and foreign keys
 *
 * @param[in,out] schema Pointer to the Schema structure whose content will be freed
 */
void free_schema_content(Schema* schema);

#endif // SCHEMA_H