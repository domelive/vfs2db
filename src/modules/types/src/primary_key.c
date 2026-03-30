/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   primary_key.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Primary Key implementation file.
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

#include "primary_key.h"
#include "schema.h"

Pk* find_pk_by_name(Schema* schema, const char* name) {
    if (!name) {
        LOG_TRACE("find_pk_by_name: 'name' parameter is NULL");
        return NULL;
    }

    Pk* pk;
    HASH_FIND_STR(schema->pk_head, name, pk);

    return pk;
}

bool is_pk_in_schema(Schema* schema, const char* pk_name) {
    Pk* pk;
    HASH_FIND_STR(schema->pk_head, pk_name, pk);
    return (pk != NULL);
}

void add_pk_to_schema(Schema* schema, Pk* pk) {
    assert(!is_pk_in_schema(schema, pk->name));
    HASH_ADD_STR(schema->pk_head, name, pk);
}

int count_pks(Schema* schema) { return HASH_COUNT(schema->pk_head); }

void remove_pk_from_schema(Schema* schema, const char* pk_name) {
    Pk* pk;
    HASH_FIND_STR(schema->pk_head, pk_name, pk);
    if (pk) {
        HASH_DEL(schema->pk_head, pk);
        free(pk->name);
        free(pk);
    }
}

void free_pk_set(Schema* schema) {
    if (!schema)
        return;
    HASH_FOREACH(current_pk, schema->pk_head) {
        HASH_DEL(schema->pk_head, current_pk);
        free(current_pk->name);
        free(current_pk);
    }
}