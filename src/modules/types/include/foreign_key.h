/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   foreign_key.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Foreign Key Header File, defining the structure and functions for managing foreign keys
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

#ifndef FOREIGN_KEY_H
#define FOREIGN_KEY_H

#include "helpers.h"
#include "uthash.h"

typedef struct Schema Schema;

/**
 * Fk Structure
 *
 * @brief Structure representing a Foreign Key relationship in a database schema.
 *
 */
typedef struct Fk {
    int id;          /**< The ID of the foreign key. */
    int sqlite_type; /**< The SQLite data type of the foreign key. */

    char* table; /**< The name of the referenced table. */
    char* from;  /**< The attribute in the current table that is the foreign key. */
    char* to;    /**< The attribute in the referenced table that the foreign key points to. */

    UT_hash_handle hh; /**< UTHash handle for hash table operations. */
} Fk;

/**
 * find_fk_by_name
 *
 * @brief Find a foreign key in the schema by its 'from' attribute name.
 *
 * This function searches the foreign key hash map in the given Schema for a foreign key with the
 * specified 'from' attribute name. If found, it returns a pointer to the Fk structure; otherwise,
 * it returns NULL.
 *
 * @param[in] schema Pointer to the Schema structure containing the foreign keys
 * @param[in] from The 'from' attribute name of the foreign key to find (e.g., the name of the
 * attribute in the current table that is the foreign key)
 *
 * @return Pointer to the Fk structure if found, NULL otherwise
 */
Fk* find_fk_by_name(Schema* schema, const char* from);

/**
 * add_fk_to_schema
 *
 * @brief Add a foreign key to the schema.
 *
 * This function adds a foreign key to the hash map in the given Schema.
 *
 * @param[in,out] schema Pointer to the Schema structure to which the foreign key will be added
 * @param[in]     fk     Pointer to the Fk structure representing the foreign key to add
 */
void add_fk_to_schema(Schema* schema, Fk* fk);

/**
 * is_fk_in_schema
 *
 * @brief Check if a foreign key exists in the schema.
 *
 * This function checks if a foreign key with the specified 'from' attribute name exists in the
 * schema.
 *
 * @param[in] schema Pointer to the Schema structure to check
 * @param[in] fk_from The 'from' attribute name of the foreign key to check
 *
 * @return true if the foreign key exists, false otherwise
 */
bool is_fk_in_schema(Schema* schema, const char* fk_from);

/**
 * remove_fk_from_schema
 *
 * @brief Remove a foreign key from the schema.
 *
 * This function removes a foreign key with the specified 'from' attribute name from the schema.
 *
 * @param[in,out] schema Pointer to the Schema structure from which the foreign key will be removed
 * @param[in]     fk_from The 'from' attribute name of the foreign key to remove
 */
void remove_fk_from_schema(Schema* schema, const char* fk_from);

/**
 * free_fk_hashmap
 *
 * @brief Free the memory allocated for the foreign key hash map in the given Schema.
 *
 * This function iterates through all foreign keys in the schema's hash map, removes them from
 * the hash map, and frees the memory allocated for each foreign key's fields and the foreign
 * key structure itself.
 */
void free_fk_hashmap(Schema* schema);

/**
 * count_fks
 *
 * @brief Count the number of foreign keys in the schema.
 * This function iterates through the foreign key hash map in the given Schema and counts the
 * number of foreign keys present.
 *
 * @param[in] schema Pointer to the Schema structure containing the foreign keys
 * @return The number of foreign keys in the schema
 */
int count_fks(Schema* schema);

#endif /* FOREIGN_KEY_H */