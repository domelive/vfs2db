/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Database Handler Header File
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

#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#include "query_manager.h"
#include "types.h"
#include "errors.h"

extern sqlite3* db;
extern DbSchema* db_schema;

status_t init_db_schema(DbSchema* db_schema);
status_t init_schema(Schema* schema);
status_t get_attribute_bytes(struct tokens* toks, char** bytes);
status_t get_attribute_size(struct tokens* toks, size_t* size);
status_t get_attribute_type(struct tokens* toks, int* type);
status_t update_attribute_value(struct tokens* toks, const char* buffer, size_t size, int append);
status_t get_table_rowids(const char* table, char* records[], int* n_records);
status_t get_rowid_from_pks(const char* table, Fk* fks[], char* fks_values[], int num_fks, int* row_id);

#endif // DB_HANDLER_H