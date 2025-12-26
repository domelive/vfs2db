/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   query_manager.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Query Manager Source File
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

#include "query_manager.h"

/**
 * Query Structure
 * 
 * @brief Structure representing a SQL query.
 * 
 * Includes the following fields:
 * 
 * - `sql`:        The SQL query string.
 * 
 * - `is_dynamic`: Flag indicating whether the query is dynamic (1) or static (0).
 * 
 * - `stmt`:       Pointer to a prepared SQLite statement (if applicable).
 * 
 */
typedef struct query_t {
    const char* sql;
    int is_dynamic;
    sqlite3_stmt* stmt;
} query_t;

/**
 * Query String Store
 * 
 * @brief Array of SQL query strings indexed by QueryID.
 * 
 */
static query_t query_store[] = {
    [QUERY_SELECT_TABLES_NAME] = {
        "SELECT "
            "name " 
        "FROM " 
            "sqlite_master "
        "WHERE "
            "type='table' AND name NOT LIKE 'sqlite_%';",
        0,
        NULL
    },
    
    [QUERY_TPL_SELECT_TABLE_INFO] = {
        "SELECT " 
            "ti.name AS column_name,"
            "ti.pk AS is_pk,"
            "fk.\"table\" AS fk_table,"
            "fk.\"to\" AS fk_column_name "
        "FROM "
            "pragma_table_info('%s') ti "
            "LEFT JOIN "
            "pragma_foreign_key_list('%s') fk "
        "ON ti.name = fk.\"from\";",
        1,
        NULL
    },
    
    [QUERY_TPL_SELECT_ATTRIBUTE] = {
        "SELECT " 
            "%s "
        "FROM "
            "%s "
        "WHERE "
            "rowid = ?",
        1,
        NULL
    },
    
    [QUERY_TPL_UPDATE_ATTRIBUTE] = {
        "UPDATE "
            "%s "
        "SET "
            "%s = ? "
        "WHERE "
            "rowid = ?",
        1,
        NULL
    },

    [QUERY_TPL_UPDATE_ATTRIBUTE_APPEND] = {
        "UPDATE "
            "%s "
        "SET "
            "%s = \"%s\" || ? "
        "WHERE "
            "rowid = ?",
        1,
        NULL
    },

    [QUERY_TPL_SELECT_TABLE_ROWIDS] = {
        "SELECT "
            "rowid "
        "FROM "
            "%s",
        1,
        NULL
    }
};

/**
 * Initialize Query Manager
 * 
 * @brief Initializes the Query Manager by preparing static SQL statements.
 * 
 * @param[in] db Pointer to the SQLite database connection
 * 
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 * 
 */
status_t qm_init(sqlite3* db) {
    for (int i = 0; i < QUERY_COUNT; i++) {
        if (!query_store[i].is_dynamic) {
            int rc = sqlite3_prepare_v2(db, query_store[i].sql, -1, &query_store[i].stmt, NULL);
            if (rc != SQLITE_OK) {
                printf("Failed to prepare static query %d: %s\n", i, sqlite3_errmsg(db));
                return STATUS_DB_ERROR;
            }
        }
    }
    return STATUS_OK;
}

/**
 * Get Query String
 * 
 * @brief Retrieves the SQL query string corresponding to the given QueryID.
 * 
 * @param[in] qid The QueryID for which to retrieve the SQL query string
 * 
 * @return The SQL query string if found, NULL otherwise
 * 
 */
char *qm_get_str(QueryID qid) {
    if (qid < 0 || qid >= QUERY_COUNT) return NULL;
    return query_store[qid].sql;
}

/**
 * Get Static Query Statement
 * 
 * @brief Retrieves the prepared SQLite statement corresponding to the given QueryID.
 * 
 * @param[in] qid The QueryID for which to retrieve the prepared statement
 * 
 * @return Pointer to the prepared SQLite statement if found, NULL otherwise
 * 
 */
sqlite3_stmt* qm_get_static_query_statement(QueryID qid) {
    if (qid < 0 || qid >= QUERY_COUNT || query_store[qid].is_dynamic) return NULL;
    
    sqlite3_stmt* s = query_store[qid].stmt;
    
    sqlite3_reset(s);
    sqlite3_clear_bindings(s);

    return s;
}

/**
 * Get Dynamic Query Statement
 * 
 * @brief Prepares and retrieves a dynamic SQLite statement based on the given QueryID and parameters.
 * 
 * @param[in] db  Pointer to the SQLite database connection
 * @param[in] qid The QueryID for which to prepare the dynamic statement
 * @param[in] ... Variadic arguments to format the SQL query string
 * 
 * @return Pointer to the prepared SQLite statement if successful, NULL otherwise
 * 
 */
sqlite3_stmt* qm_build_dynamic_query_statement(sqlite3* db, QueryID qid, ...) {
    if (qid < 0 || qid >= QUERY_COUNT || !query_store[qid].is_dynamic) return NULL;

    const char* tpl = query_store[qid].sql;
    char buffer[2048];

    va_list args;
    va_start(args, qid);

    vsnprintf(buffer, sizeof(buffer), tpl, args);

    va_end(args);

    printf("Dynamic Query: %s\n", buffer);

    sqlite3_stmt* s;
    if (sqlite3_prepare_v2(db, buffer, -1, &s, NULL) != SQLITE_OK) {
        printf("Failed to prepare dynamic query %d: %s\n", qid, sqlite3_errmsg(db));
        return NULL;
    }

    return s;
}

/** 
 * Cleanup Query Manager
 * 
 * @brief Cleans up the Query Manager by finalizing prepared SQL statements.
 * 
*/
void qm_cleanup() {
    for (int i = 0; i < QUERY_COUNT; i++) {
        if (query_store[i].stmt) {
            sqlite3_finalize(query_store[i].stmt);
            query_store[i].stmt = NULL;
        }
    }
}