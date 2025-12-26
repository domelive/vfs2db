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

// =============================================================
// Metadata Structures
// =============================================================

/**
 * @brief Structure representing a Foreign Key relationship in a database schema.
 * 
 * Includes the following fields:
 * 
 * - `from`:      The attribute in the current table that is the foreign key.
 * 
 * - `table`:     The name of the referenced table.
 * 
 * - `to`:        The attribute in the referenced table that the foreign key points to.
 * 
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid memory leaks.
 * 
 */
typedef struct Fk {
    char *from;  
    char *table;
    char *to;
} Fk;

/**
 * @brief Structure representing the schema of a database table.
 * 
 * Includes the following fields:
 * 
 * - `name`:    Name of the table.
 * 
 * - `pk`:      Array of strings representing the names of primary key attributes.
 * 
 * - `attr`:    Array of strings representing the names of other attributes.
 * 
 * - `fks`:     Array of pointers to `Fk` structures representing foreign key relationships.
 * 
 * - `n_pk`:    Number of primary key attributes.
 * 
 * - `n_attr`:  Number of other attributes.
 * 
 * - `n_fks`:   Number of foreign key relationships.
 * 
 * @note All string fields are dynamically allocated and should be freed appropriately to avoid memory leaks.
 */
typedef struct Schema {
    char *name;
    char *pk[MAX_SIZE];
    char *attr[MAX_SIZE];
    
    Fk   *fks[MAX_SIZE];
    
    int   n_pk;
    int   n_attr;
    int   n_fks;
} Schema; 

// =============================================================
// Global Structures
// =============================================================

/**
 * @brief Structure representing the overall database schema.
 * 
 * Includes the following fields:
 * 
 * - `tables`:   Array of pointers to `Schema` structures representing the tables in the database.
 * 
 * - `n_tables`: Number of tables in the database.
 *
 */
typedef struct DbSchema {
    Schema *tables[MAX_SIZE];
    int     n_tables;
} DbSchema;

// =============================================================
// Other Structures
// =============================================================

// FIX: make it a type, like Tokens
struct tokens {
    char *table;
    char *record;
    char *attribute;
};

#endif // TYPES_H