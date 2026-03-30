/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   parser.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Parser Header File, defining the structure for parsing database schema information.
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

#ifndef PATH_PARSER_H
#define PATH_PARSER_H

#include "arena.h"
#include "logger.h"

typedef struct PathFieldsResult {
    char* table;
    char* record;
    char* attribute;
} PathFieldsResult;

typedef struct DotSchemaFieldsResult {
    char* column_name;
    char* column_type;
    char* column_spec;
} DotSchemaFieldsResult;

PathFieldsResult*      parser_parse_path(Arena* arena, const char* path);
DotSchemaFieldsResult* parser_parse_dot_schema(Arena* arena, const char* dot_schema);

#endif // PATH_PARSER_H