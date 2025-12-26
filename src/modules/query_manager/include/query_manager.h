/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   query_manager.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Query Manager Header File
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

#ifndef QUERY_MANAGER_H
#define QUERY_MANAGER_H

#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <stdarg.h>

#include "errors.h"

/**
 * Query Identifiers
 * 
 * @brief Enumeration of query identifiers used to reference SQL queries.
 * 
 */
typedef enum {
    QUERY_SELECT_TABLES_NAME,
    
    QUERY_TPL_SELECT_TABLE_INFO,
    QUERY_TPL_SELECT_ATTRIBUTE,
    QUERY_TPL_UPDATE_ATTRIBUTE,
    QUERY_TPL_UPDATE_ATTRIBUTE_APPEND,
    QUERY_TPL_SELECT_TABLE_ROWIDS,

    QUERY_COUNT
} QueryID;

status_t qm_init(sqlite3 *db);

char* qm_get_str(QueryID qid);
sqlite3_stmt* qm_get_static_query_statement(QueryID qid);
sqlite3_stmt* qm_build_dynamic_query_statement(sqlite3* db, QueryID qid, ...);

void qm_cleanup();

// TODO: variadic function to build a dynamic query

#endif // QUERY_MANAGER_H