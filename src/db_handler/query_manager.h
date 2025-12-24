/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   query_manager.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (...)
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

/**
 * Query Identifiers
 * 
 * @brief Enumeration of query identifiers used to reference SQL queries.
 * 
 */
typedef enum {
    QUERY_GET_TABLES_NAME,
    QUERY_GET_TABLE_INFO,
    QUERY_COUNT
} QueryID;

int  qm_init(sqlite3 *db);
void qm_cleanup();

char         *qm_get_query_str(QueryID qid);
sqlite3_stmt *qm_get_static(QueryID qid);

// TODO: variadic function to build a dynamic query

#endif // QUERY_MANAGER_H