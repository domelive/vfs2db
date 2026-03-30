/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   schema.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Schema implementation file.
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

#include "schema.h"
#include "db_schema.h"

Schema* find_schema_by_name(DbSchema* db_schema, const char* name) {
    Schema* s;
    HASH_FIND_STR(db_schema->tables_head, name, s);
    return s;
}

void add_schema(DbSchema* db_schema, Schema* table_schema) {
    Schema* existing_schema = find_schema_by_name(db_schema, table_schema->name);
    if (existing_schema != NULL) {
        return;
    }
    HASH_ADD_STR(db_schema->tables_head, name, table_schema);
}

int count_schemas(DbSchema* db_schema) { return HASH_COUNT(db_schema->tables_head); }

void free_schema_content(Schema* schema) {
    if (!schema)
        return;

    free_pk_set(schema);
    free_attr_set(schema);
    free_fk_hashmap(schema);
}