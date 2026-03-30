/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   primary_key.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Primary Key Header File, defining the structure and functions for managing primary keys
 * in the database schema.
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

#ifndef PRIMARY_KEY_H
#define PRIMARY_KEY_H

#include "helpers.h"
#include "uthash.h"

#include <stdbool.h>

typedef struct Schema Schema;

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
 * find_pk_by_name
 *
 * @brief Retrieve Primary Key by Name
 *
 * @param[in] schema Pointer to the Schema structure containing primary keys
 * @param[in] name   The name of the primary key to retrieve
 *
 * @return Pointer to the Pk structure if found, NULL otherwise
 */
Pk* find_pk_by_name(Schema* schema, const char* name);

/**
 * is_pk_in_schema
 *
 * @brief Check if Primary Key Exists in Schema
 *
 * @param[in] schema   Pointer to the Schema structure
 * @param[in] pk_name  The name of the primary key to check
 *
 * @return true if the primary key exists in the schema, false otherwise
 */
bool is_pk_in_schema(Schema* schema, const char* pk_name);

/**
 * add_pk_to_schema
 *
 * @brief Add Primary Key to Schema
 *
 * @param[in,out] schema Pointer to the Schema structure to which the primary key will be added
 * @param[in]     pk     Pointer to the Pk structure to add
 */
void add_pk_to_schema(Schema* schema, Pk* pk);

/**
 * count_pks
 *
 * @brief Count the number of primary keys in a Schema
 *
 * @param[in] schema Pointer to the Schema structure containing primary keys
 *
 * @return The number of primary keys in the Schema
 */
int count_pks(Schema* schema);

/**
 * remove_pk_from_schema
 *
 * @brief Remove Primary Key from Schema
 *
 * @param[in,out] schema Pointer to the Schema structure from which the primary key will be removed
 * @param[in]     pk_name  The name of the primary key to remove
 */
void remove_pk_from_schema(Schema* schema, const char* pk_name);

/**
 * free_pk_set
 *
 * @brief Free Primary Key Set in Schema
 *
 * @param[in,out] schema Pointer to the Schema structure whose primary key set will be freed
 */
void free_pk_set(Schema* schema);

#endif /* PRIMARY_KEY_H */