/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (...)
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
#include "../utils/types.h"

extern sqlite3 *db;

int  init_db_schema(DbSchema *db_schema);
int  init_schema(Schema *schema);

int  get_attribute_size(struct tokens* toks);
int  get_attribute_value(struct tokens* toks, char **bytes, size_t *size);
int  get_attribute_type(struct tokens *toks);
int  update_attribute_value(struct tokens* toks, const char *buffer, size_t size, int append);
void make_root_select(sqlite3_stmt **pstmt);
void make_table_select(sqlite3_stmt **pstmt, const char *table);
void make_record_select(sqlite3_stmt **pstmt, const char *table);
void get_table_fks(sqlite3_stmt **pstmt, const char *table);
void get_foreign_table_attribute_name(struct tokens *toks, char **ftable, char **fattribute);
int  get_all_fkpk_relationships_length(const char *src_table, const char *dst_table);
void get_all_fkpk_relationships(const char *src_table, const char *dst_table, struct pkfk_relation *pkfk);
void fill_fk_values(const char *table, const char *record, struct pkfk_relation *pkfk, int pkfk_length);
int  get_rowid_from_pks(const char *table, struct pkfk_relation *pkfk, int pkfk_length);

#endif // DB_HANDLER_H