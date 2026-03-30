/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   attribute.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Attribute implementation file.
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

#include "attribute.h"
#include "schema.h"

Attr* find_attribute_by_name(Schema* schema, const char* name) {
    if (!name) {
        LOG_TRACE("find_attribute_by_name: 'name' parameter is NULL");
        return NULL;
    }

    Attr* attr;
    HASH_FIND_STR(schema->attr_head, name, attr);

    return attr;
}

bool is_attribute_in_schema(Schema* schema, const char* attr_name) {
    Attr* attr;
    HASH_FIND_STR(schema->attr_head, attr_name, attr);
    return (attr != NULL);
}

void add_attribute_to_schema(Schema* schema, Attr* attr) {
    assert(!is_attribute_in_schema(schema, attr->name));
    HASH_ADD_STR(schema->attr_head, name, attr);
}

void remove_attribute_from_schema(Schema* schema, const char* column_name) {
    Attr* attr;
    HASH_FIND_STR(schema->attr_head, column_name, attr);
    if (attr) {
        HASH_DEL(schema->attr_head, attr);
        free(attr->name);
        free(attr);
    }
}

int count_attributes(Schema* schema) { return HASH_COUNT(schema->attr_head); }

void free_attr_set(Schema* schema) {
    if (!schema)
        return;

    HASH_FOREACH(current_attr, schema->attr_head) {
        HASH_DEL(schema->attr_head, current_attr);
        free(current_attr->name);
        free(current_attr);
    }
}