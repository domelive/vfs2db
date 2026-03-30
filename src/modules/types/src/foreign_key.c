/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   foreign_key.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Foreign Key implementation file.
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

#include "foreign_key.h"
#include "schema.h"

Fk* find_fk_by_name(Schema* schema, const char* from) {
    if (!from) {
        LOG_TRACE("find_fk_by_name: 'from' parameter is NULL");
        return NULL;
    }

    Fk* fk;
    HASH_FIND_STR(schema->fks_head, from, fk);

    return fk;
}

void add_fk_to_schema(Schema* schema, Fk* fk) {
    Fk* existing_fk = find_fk_by_name(schema, fk->from);

    if (existing_fk != NULL)
        return;

    HASH_ADD_STR(schema->fks_head, from, fk);
}

bool is_fk_in_schema(Schema* schema, const char* fk_from) {
    Fk* fk;
    HASH_FIND_STR(schema->fks_head, fk_from, fk);
    return (fk != NULL);
}

void remove_fk_from_schema(Schema* schema, const char* fk_from) {
    Fk* fk;
    HASH_FIND_STR(schema->fks_head, fk_from, fk);
    if (fk) {
        HASH_DEL(schema->fks_head, fk);
        free(fk->from);
        free(fk->table);
        free(fk->to);
        free(fk);
    }
}

void free_fk_hashmap(Schema* schema) {
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

int count_fks(Schema* schema) { return HASH_COUNT(schema->fks_head); }
