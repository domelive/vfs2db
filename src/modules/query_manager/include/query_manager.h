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

#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "errors.h"
#include "logger.h"

/**
 * QueryID Enumeration
 *
 * @brief Enumeration of QueryIDs corresponding to different SQL queries used in the VFS2DB
 * filesystem. Each QueryID represents a specific SQL query template that can be either static or
 * dynamic.
 */
typedef enum {
    QUERY_SELECT_TABLES_NAME,
    QUERY_SELECT_TABLE_QUERY_STRING,
    QUERY_GET_SCHEMA_VERSION,
    QUERY_TPL_PRAGMA,

    QUERY_TPL_SELECT_TABLE_INFO,
    QUERY_TPL_SELECT_FK_ID,
    QUERY_TPL_SELECT_ATTRIBUTE_IS_NULL,
    QUERY_TPL_SELECT_ATTRIBUTE_SIZE,
    QUERY_TPL_SELECT_ATTRIBUTE,
    QUERY_TPL_SELECT_CHUNK_ATTRIBUTE,
    QUERY_TPL_SELECT_TABLE_ROWIDS,
    QUERY_TPL_SELECT_ROWID,
    QUERY_TPL_UPDATE_ATTRIBUTE,
    QUERY_TPL_UPDATE_ZERO_BLOB,
    QUERY_TPL_INSERT_RECORD_INTO_TABLE,
    QUERY_TPL_CREATE_EMPTY_TABLE,
    QUERY_TPL_ADD_PRIMARY_KEY_COLUMN,
    QUERY_TPL_ADD_ATTRIBUTE_COLUMN,
    QUERY_TPL_ADD_FOREIGN_KEY_COLUMN,
    QUERY_TPL_DROP_SCHEMA_COLUMN,
    QUERY_TPL_DROP_TABLE,
    QUERY_TPL_DELETE_RECORD_FROM_TABLE,

    QUERY_COUNT
} QueryID;

/**
 * Get Query String
 *
 * @brief Retrieves the SQL query string associated with the given QueryID.
 *
 * @param[in] qid The QueryID for which to retrieve the SQL query string
 *
 * @return The SQL query string if found, NULL otherwise
 */
char* qm_get_str(QueryID qid);

/**
 * Get Dynamic Query Statement
 *
 * @brief Builds a prepared SQLite statement for the given QueryID by formatting the associated SQL
 *
 * @param[in] db  Pointer to the SQLite database connection
 * @param[in] qid The QueryID for which to prepare the dynamic statement
 * @param[in] ... Variadic arguments to format the SQL query string
 *
 * @return Pointer to the prepared SQLite statement if successful, NULL otherwise
 */
sqlite3_stmt* qm_build_query_statement(sqlite3* db, QueryID qid, ...);

/**
 * Execute Multi-Statement Query
 *
 * @brief Prepares and executes a multi-statement SQLite query based on the given QueryID and
 * parameters. This is used for queries that consist of multiple SQL statements, such as
 * transactions.
 *
 * @param[in] db  Pointer to the SQLite database connection
 * @param[in] qid The QueryID for which to prepare the multi-statement query
 * @param[in] ... Variadic arguments to format the SQL query string
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t qm_exec_multi_stmt_query(sqlite3* db, QueryID qid, ...);

#endif // QUERY_MANAGER_H