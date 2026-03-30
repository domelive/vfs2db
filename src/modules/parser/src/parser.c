/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   parser.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Parser Source File, implementing functions for parsing database schema information.
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

#include "parser.h"

PathFieldsResult* parser_parse_path(Arena* arena, const char* path) {
    LOG_TRACE("Parsing path: %s", path ? path : "(null)");

    // We expect paths in the form of /table/record/attribute.vfs2db, so we can have at most 3
    // tokens (table, record, attribute). We will ignore the .vfs2db extension in this function and
    // handle it separately.
    if (!path) {
        LOG_WARN("tokenize_path called with NULL path");
        return NULL;
    }

    // Allocate tokens structure in arena
    PathFieldsResult* fields = arena_calloc(arena, 1, sizeof(PathFieldsResult));
    if (!fields) {
        LOG_ERROR("Failed to allocate tokens struct");
        return NULL;
    }

    // Make a mutable copy of the path in the arena for tokenization
    char* path_copy = arena_strdup(arena, path);
    if (!path_copy) {
        LOG_ERROR("Failed to duplicate path string");
        return NULL;
    }

    // Skip leading slash if present
    char* cursor = path_copy;
    if (cursor[0] == '/')
        cursor++;

    // Tokenize the path using '/' as a delimiter
    // First token is the table name
    char* t       = strtok(cursor, "/");
    fields->table = t ? arena_strdup(arena, t) : NULL;

    // Second token is the record name
    t              = strtok(NULL, "/");
    fields->record = t ? arena_strdup(arena, t) : NULL;

    // Third token is the attribute name (we will remove the .vfs2db extension later if present)
    t                 = strtok(NULL, "/");
    fields->attribute = t ? arena_strdup(arena, t) : NULL;

    LOG_TRACE("Tokenized path '%s': table=%s, record=%s, attr=%s", path,
              fields->table ? fields->table : "(null)", fields->record ? fields->record : "(null)",
              fields->attribute ? fields->attribute : "(null)");

    return fields;
}

DotSchemaFieldsResult* parser_parse_dot_schema(Arena* arena, const char* column_name) {
    LOG_TRACE("Parsing .schema column name: %s", column_name ? column_name : "(null)");

    if (!column_name) {
        LOG_WARN("tokenize_dot_schema called with NULL column_name");
        return NULL;
    }

    DotSchemaFieldsResult* fields = arena_calloc(arena, 1, sizeof(DotSchemaFieldsResult));
    if (!fields) {
        LOG_ERROR("Failed to allocate DotSchemaFieldsResult struct");
        return NULL;
    }

    // Tokenize the path using '/' as a delimiter
    // First token is the table name
    char* t             = strtok(column_name, ".");
    fields->column_name = t ? arena_strdup(arena, t) : NULL;

    // Second token is the record name
    t                   = strtok(NULL, ".");
    fields->column_type = t ? arena_strdup(arena, t) : NULL;

    // Third token is the attribute name (we will remove the .vfs2db extension later if present)
    t                   = strtok(NULL, ".");
    fields->column_spec = t ? arena_strdup(arena, t) : NULL;

    LOG_TRACE("Tokenized column name '%s': name=%s, type=%s, spec=%s", column_name,
              fields->column_name ? fields->column_name : "(null)",
              fields->column_type ? fields->column_type : "(null)",
              fields->column_spec ? fields->column_spec : "(null)");

    return fields;
}