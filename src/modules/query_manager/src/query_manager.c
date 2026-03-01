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
 * - `sql`:        The SQL query string.
 * - `is_dynamic`: Flag indicating whether the query is dynamic (1) or static (0).
 * - `stmt`:       Pointer to a prepared SQLite statement (if applicable).
 */
typedef struct query_t {
    const char*   sql;
    int           is_dynamic;
    sqlite3_stmt* stmt;
} query_t;

static query_t query_store[] = {
    [QUERY_SELECT_TABLES_NAME] = {"SELECT "
                                  "name "
                                  "FROM "
                                  "sqlite_master "
                                  "WHERE "
                                  "type='table' AND name NOT LIKE 'sqlite_%';",
                                  0, NULL},

    [QUERY_TPL_SELECT_TABLE_INFO] = {"SELECT "
                                     "ti.name AS column_name,"
                                     "ti.type AS column_type,"
                                     "ti.pk AS is_pk,"
                                     "fk.\"table\" AS fk_table,"
                                     "fk.\"to\" AS fk_column_name "
                                     "FROM "
                                     "pragma_table_info('%s') ti "
                                     "LEFT JOIN "
                                     "pragma_foreign_key_list('%s') fk "
                                     "ON ti.name = fk.\"from\";",
                                     1, NULL},

    [QUERY_TPL_SELECT_ATTRIBUTE_IS_NULL] = {"SELECT "
                                            "%s IS NULL "
                                            "FROM "
                                            "%s "
                                            "WHERE "
                                            "rowid = ?",
                                            1, NULL},

    [QUERY_TPL_SELECT_ATTRIBUTE_SIZE] = {"SELECT "
                                         "LENGTH(%s) "
                                         "FROM "
                                         "%s "
                                         "WHERE "
                                         "rowid = ?",
                                         1, NULL},

    [QUERY_TPL_SELECT_ATTRIBUTE] = {"SELECT "
                                    "%s "
                                    "FROM "
                                    "%s "
                                    "WHERE "
                                    "rowid = ?",
                                    1, NULL},

    [QUERY_TPL_SELECT_CHUNK_ATTRIBUTE] = {"SELECT "
                                          "substr(%s, %ld + 1, %ld) "
                                          "FROM "
                                          "%s "
                                          "WHERE "
                                          "rowid = ?",
                                          1, NULL},

    [QUERY_TPL_UPDATE_ATTRIBUTE] = {"UPDATE "
                                    "%s "
                                    "SET "
                                    "%s = ? "
                                    "WHERE "
                                    "rowid = ?",
                                    1, NULL},

    [QUERY_TPL_SELECT_TABLE_ROWIDS] = {"SELECT "
                                       "rowid "
                                       "FROM "
                                       "%s",
                                       1, NULL},

    [QUERY_TPL_SELECT_ROWID] = {"SELECT "
                                "rowid "
                                "FROM "
                                "%s "
                                "WHERE rowid = ?",
                                1, NULL},

    [QUERY_TPL_UPDATE_ZERO_BLOB] = {"UPDATE "
                                    "%s "
                                    "SET "
                                    "%s = CAST(IFNULL(%s, X'') || ? AS BLOB) "
                                    "WHERE "
                                    "rowid = ?",
                                    1, NULL},

    [QUERY_TPL_INSERT_RECORD_INTO_TABLE] = {"INSERT INTO "
                                            "%s (rowid) "
                                            "VALUES (%s)",
                                            1, NULL},

    [QUERY_TPL_CREATE_EMPTY_TABLE] = {"CREATE TABLE IF NOT EXISTS "
                                      "%s (rowid INTEGER PRIMARY KEY AUTOINCREMENT)",
                                      1, NULL},

    [QUERY_TPL_DROP_SCHEMA_COLUMN] = {"ALTER TABLE "
                                      "%s "
                                      "DROP COLUMN "
                                      "%s",
                                      1, NULL},
};

status_t qm_init(sqlite3* db) {
    LOG_DEBUG("Initializing Query Manager...");

    int static_count  = 0;
    int dynamic_count = 0;

    // Prepare static queries and store the prepared statements in the query store for later use.
    // Dynamic queries will be prepared on demand when requested, so they are not prepared at
    // initialization.
    for (int i = 0; i < QUERY_COUNT; i++) {
        if (!query_store[i].is_dynamic) {
            LOG_TRACE("Preparing static query %d: %.50s...", i, query_store[i].sql);

            int rc = sqlite3_prepare_v2(db, query_store[i].sql, -1, &query_store[i].stmt, NULL);
            if (rc != SQLITE_OK) {
                LOG_ERROR("Failed to prepare static query %d: %s", i, sqlite3_errmsg(db));
                return STATUS_DB_ERROR;
            }
            static_count++;
        } else {
            dynamic_count++;
        }
    }

    LOG_INFO("Query Manager initialized: %d static queries, %d dynamic queries.", static_count,
             dynamic_count);

    return STATUS_OK;
}

char* qm_get_str(QueryID qid) {
    if (qid < 0 || qid >= QUERY_COUNT) {
        LOG_WARN("Invalid QueryID: %d", qid);
        return NULL;
    }

    return (char*)query_store[qid].sql;
}

sqlite3_stmt* qm_get_static_query_statement(QueryID qid) {
    if (qid < 0 || qid >= QUERY_COUNT) {
        LOG_ERROR("Invalid QueryID: %d", qid);
        return NULL;
    }

    // Static queries should have their statements prepared at initialization and stored in the
    // query store. If the query is dynamic, it should not be retrieved using this function.
    if (query_store[qid].is_dynamic) {
        LOG_ERROR("QueryID %d is dynamic, use qm_build_dynamic_query_statement", qid);
        return NULL;
    }

    sqlite3_stmt* s = query_store[qid].stmt;

    // Reset the statement to clear any previous bindings and state before returning it for use.
    // This ensures that the statement is in a clean state when retrieved for execution, preventing
    // issues from previous executions from affecting the current use of the statement.
    sqlite3_reset(s);
    sqlite3_clear_bindings(s);

    LOG_TRACE("Retrieved static statement for QueryID %d", qid);

    return s;
}

sqlite3_stmt* qm_build_dynamic_query_statement(sqlite3* db, QueryID qid, ...) {
    if (qid < 0 || qid >= QUERY_COUNT) {
        LOG_ERROR("Invalid QueryID: %d", qid);
        return NULL;
    }

    if (!query_store[qid].is_dynamic) {
        LOG_ERROR("QueryID %d is static, use qm_get_static_query_statement", qid);
        return NULL;
    }

    const char* tpl = query_store[qid].sql;
    char        buffer[2048];

    // Format the SQL query string using the provided variadic arguments. This allows us to create
    // dynamic SQL queries based on templates defined in the query store, which can be used for
    // operations that require variable components in the SQL, such as table names or column names.
    va_list args;
    va_start(args, qid);
    vsnprintf(buffer, sizeof(buffer), tpl, args);
    va_end(args);

    LOG_TRACE("Built dynamic query: %.100s%s", buffer, strlen(buffer) > 100 ? "..." : "");

    sqlite3_stmt* s;

    // Prepare the formatted SQL query string to create a SQLite statement that can be executed.
    if (sqlite3_prepare_v2(db, buffer, -1, &s, NULL) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare dynamic query %d: %s", qid, sqlite3_errmsg(db));
        return NULL;
    }

    return s;
}

void qm_cleanup() {
    LOG_DEBUG("Cleaning up Query Manager...");

    int finalized = 0;
    for (int i = 0; i < QUERY_COUNT; i++) {
        if (query_store[i].stmt) {
            sqlite3_finalize(query_store[i].stmt);
            query_store[i].stmt = NULL;
            finalized++;
        }
    }

    LOG_INFO("Query Manager cleanup complete: %d statements finalized", finalized);
}