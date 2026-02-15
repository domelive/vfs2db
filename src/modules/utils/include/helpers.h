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

#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "uthash.h"

// =============================================================
// Macros
// =============================================================

/**
 * FOREACH
 *
 * @brief Macro to iterate over all elements in a UTHash hash table.
 *
 * @param[in] current The iterator variable for the current element
 * @param[in] head    The head of the UTHash hash table
 */
#define HASH_FOREACH(current, head)                                                                \
    for (typeof(head) current = NULL, _tmp_##current = NULL, _once_##current = (typeof(head))1;    \
         _once_##current; _once_##current = NULL)                                                  \
    HASH_ITER(hh, head, current, _tmp_##current)

// =============================================================
// Helper Functions for DbSchema and Schema
// =============================================================

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
static inline Schema* find_schema_by_name(DbSchema* db_schema, const char* name) {
    Schema* s;
    HASH_FIND_STR(db_schema->tables_head, name, s);
    return s;
}

/**
 * add_schema
 *
 * @brief Add Schema to DbSchema
 *
 * @param[in,out] db_schema     Pointer to the DbSchema structure to which the schema will be added
 * @param[in]     table_schema  Pointer to the Schema structure to add
 */
static inline void add_schema(DbSchema* db_schema, Schema* table_schema) {
    Schema* existing_schema = find_schema_by_name(db_schema, table_schema->name);
    if (existing_schema != NULL) {
        return;
    }
    HASH_ADD_STR(db_schema->tables_head, name, table_schema);
}

/**
 * count_schemas
 *
 * @brief Count the number of schemas in DbSchema
 *
 * @param[in] db_schema Pointer to the DbSchema structure containing all table schemas
 *
 * @return The number of schemas in the DbSchema
 */
static inline int count_schemas(DbSchema* db_schema) { return HASH_COUNT(db_schema->tables_head); }

// ============================================================
// Helper Functions for Foreign Keys
// ============================================================

/**
 * find_fk_by_name
 *
 * @brief Retrieve Foreign Key by 'from' Attribute Name
 *
 * @param[in] schema Pointer to the Schema structure containing foreign keys
 * @param[in] from   The 'from' attribute name of the foreign key to retrieve
 *
 * @return Pointer to the Fk structure if found, NULL otherwise
 */
static inline Fk* find_fk_by_name(Schema* schema, const char* from) {
    if (!from) {
        LOG_TRACE("find_fk_by_name: 'from' parameter is NULL");
        return NULL;
    }

    Fk* fk;
    HASH_FIND_STR(schema->fks_head, from, fk);

    return fk;
}

/**
 * add_fk
 *
 * @brief Add Foreign Key to Schema
 *
 * @param[in,out] schema Pointer to the Schema structure to which the foreign key will be added
 * @param[in]     fk     Pointer to the Fk structure to add
 */
static inline void add_fk_to_schema(Schema* schema, Fk* fk) {
    Fk* existing_fk = find_fk_by_name(schema, fk->from);
    if (existing_fk != NULL)
        return;
    HASH_ADD_STR(schema->fks_head, from, fk);
}

/**
 * count_fks
 *
 * @brief Count the number of foreign keys in a Schema
 *
 * @param[in] schema Pointer to the Schema structure containing foreign keys
 *
 * @return The number of foreign keys in the Schema
 */
static inline int count_fks(Schema* schema) { return HASH_COUNT(schema->fks_head); }

// ============================================================
// Helper Functions for Primary Key
// ============================================================

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
static inline bool is_pk_in_schema(Schema* schema, const char* pk_name) {
    Pk* pk;
    HASH_FIND_STR(schema->pk_head, pk_name, pk);
    return (pk != NULL);
}

/**
 * add_pk_to_schema
 *
 * @brief Add Primary Key to Schema
 *
 * @param[in,out] schema Pointer to the Schema structure to which the primary key will be added
 * @param[in]     pk     Pointer to the Pk structure to add
 */
static inline void add_pk_to_schema(Schema* schema, Pk* pk) {
    assert(!is_pk_in_schema(schema, pk->name));
    HASH_ADD_STR(schema->pk_head, name, pk);
}

/**
 * count_pks
 *
 * @brief Count the number of primary keys in a Schema
 *
 * @param[in] schema Pointer to the Schema structure containing primary keys
 *
 * @return The number of primary keys in the Schema
 */
static inline int count_pks(Schema* schema) { return HASH_COUNT(schema->pk_head); }

// ============================================================
// Helper Functions for Attributes
// ============================================================

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
static inline bool is_attribute_in_schema(Schema* schema, const char* attr_name) {
    Attr* attr;
    HASH_FIND_STR(schema->attr_head, attr_name, attr);
    return (attr != NULL);
}

/**
 * add_attribute_to_schema
 *
 * @brief Add Attribute to Schema
 *
 * @param[in] schema Pointer to the Schema structure to which the attribute will be added
 * @param[in] attr   Pointer to the Attr structure to add
 */
static inline void add_attribute_to_schema(Schema* schema, Attr* attr) {
    assert(!is_attribute_in_schema(schema, attr->name));
    HASH_ADD_STR(schema->attr_head, name, attr);
}

/**
 * count_attributes
 *
 * @brief Count the number of attributes in a Schema
 *
 * @param[in] schema Pointer to the Schema structure containing attributes
 *
 * @return The number of attributes in the Schema
 */
static inline int count_attributes(Schema* schema) { return HASH_COUNT(schema->attr_head); }

// =============================================================
// Helper Functions for Freeing Structures
// =============================================================

/**
 * free_pk_set
 *
 * @brief Free Primary Key Set in Schema
 *
 * @param[in,out] schema Pointer to the Schema structure whose primary key set will be freed
 */
static inline void free_pk_set(Schema* schema) {
    if (!schema)
        return;
    HASH_FOREACH(current_pk, schema->pk_head) {
        HASH_DEL(schema->pk_head, current_pk);
        free(current_pk->name);
        free(current_pk);
    }
}

/**
 * free_attr_set
 *
 * @brief Free Attribute Set in Schema
 *
 * @param[in,out] schema Pointer to the Schema structure whose attribute set will be freed
 */
static inline void free_attr_set(Schema* schema) {
    if (!schema)
        return;
    HASH_FOREACH(current_attr, schema->attr_head) {
        HASH_DEL(schema->attr_head, current_attr);
        free(current_attr->name);
        free(current_attr);
    }
}

/**
 * free_fk_hashmap
 *
 * @brief Free Foreign Key Hashmap in Schema
 *
 * @param[in,out] schema Pointer to the Schema structure whose foreign key hashmap will be freed
 */
static inline void free_fk_hashmap(Schema* schema) {
    if (!schema)
        return;
    HASH_FOREACH(current_fk, schema->fks_head) {
        HASH_DEL(schema->fks_head, current_fk);
        free(current_fk->from);
        free(current_fk->table);
        free(current_fk->to);
        free(current_fk);
    }
}

// TODO: Consider merging free_fk_hashmap and free_schema_hashmap into a single function that can
// free any hash map, given the appropriate free function for the elements.
/**
 * free_schema_hashmap
 *
 * @brief Free Foreign Key Hashmap in Schema
 *
 * @param[in,out] schema Pointer to the Schema structure whose foreign key hashmap will be freed
 *
 */
static inline void free_schema_hashmap(DbSchema* schema) {
    if (!schema)
        return;
    HASH_FOREACH(current_schema, schema->tables_head) {
        HASH_DEL(schema->tables_head, current_schema);
        free(current_schema);
    }
}

#endif // HELPERS_H