/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_schema.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  DbSchema implementation file.
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

#include "db_schema.h"
#include "schema.h"

void remove_schema(DbSchema* db_schema, Schema* schema) {
    if (!db_schema || !schema)
        return;

    HASH_DEL(db_schema->tables_head, schema);
    free(schema->name);
    free(schema);
}

void free_schema_hashmap(DbSchema* schema) {
    if (!schema)
        return;

    HASH_FOREACH(current_schema, schema->tables_head) {
        free_schema_content(current_schema);
        remove_schema(schema, current_schema);
    }

    schema->tables_head = NULL;
}
