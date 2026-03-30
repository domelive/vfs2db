/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   attribute.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Attribute Header File, defining the structure and functions for managing attributes
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

#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "helpers.h"
#include "uthash.h"

#include <stdbool.h>

typedef struct Schema Schema;

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
 * find_attribute_by_name
 *
 * @brief Retrieve Attribute by Name
 *
 * @param[in] schema Pointer to the Schema structure containing attributes
 * @param[in] name   The name of the attribute to retrieve
 *
 * @return Pointer to the Attr structure if found, NULL otherwise
 */
Attr* find_attribute_by_name(Schema* schema, const char* name);

/**
 * is_attribute_in_schema
 *
 * @brief Check if Attribute Exists in Schema
 *
 * @param[in] schema    Pointer to the Schema structure
 * @param[in] attr_name The name of the attribute to check
 *
 * @return true if the attribute exists in the schema, false otherwise
 */
bool is_attribute_in_schema(Schema* schema, const char* attr_name);

/**
 * add_attribute_to_schema
 *
 * @brief Add Attribute to Schema
 *
 * @param[in] schema Pointer to the Schema structure to which the attribute will be added
 * @param[in] attr   Pointer to the Attr structure to add
 */
void add_attribute_to_schema(Schema* schema, Attr* attr);

/**
 * remove_attribute_from_schema
 *
 * @brief Remove Attribute from Schema
 *
 * @param[in,out] schema Pointer to the Schema structure from which the attribute will be removed
 * @param[in]     column_name The name of the attribute to remove
 */
void remove_attribute_from_schema(Schema* schema, const char* column_name);

/**
 * count_attributes
 *
 * @brief Count the number of attributes in a Schema
 *
 * @param[in] schema Pointer to the Schema structure containing attributes
 *
 * @return The number of attributes in the Schema
 */
int count_attributes(Schema* schema);

/**
 * free_attr_set
 *
 * @brief Free Attribute Set in Schema
 *
 * @param[in,out] schema Pointer to the Schema structure whose attribute set will be freed
 */
void free_attr_set(Schema* schema);

#endif /* ATTRIBUTE_H */