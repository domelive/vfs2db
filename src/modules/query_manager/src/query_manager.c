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

typedef enum QueryType {
    QUERY_TYPE_SINGLE = 0, /**< Static query with a fixed SQL string */
    QUERY_TYPE_MULTI  = 1, /**< Multi-statement query that may contain multiple SQL statements */
} QueryType;

/**
 * Query Structure
 *
 * @brief Structure representing a SQL query.
 *
 * Includes the following fields:
 * - `sql`:        The SQL query string.
 * - `type`:       The type of the query.
 */
typedef struct Query {
    const char* sql;
    QueryType   type;
} Query;

static Query query_store[] = {
    [QUERY_SELECT_TABLES_NAME] = {"SELECT "
                                  "name "
                                  "FROM "
                                  "sqlite_master "
                                  "WHERE "
                                  "type='table' AND name NOT LIKE 'sqlite_%';",
                                  QUERY_TYPE_SINGLE},

    [QUERY_SELECT_TABLE_QUERY_STRING] = {"SELECT "
                                         "sql "
                                         "FROM "
                                         "sqlite_master "
                                         "WHERE "
                                         "type='table' AND name = ?;",
                                         QUERY_TYPE_SINGLE},

    [QUERY_GET_SCHEMA_VERSION] = {"PRAGMA schema_version;", QUERY_TYPE_SINGLE},

    [QUERY_TPL_PRAGMA] = {"PRAGMA %s=%s;", QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_TABLE_INFO] = {"SELECT "
                                     "ti.name AS column_name,"
                                     "ti.type AS column_type,"
                                     "ti.pk AS is_pk,"
                                     "fk.\"table\" AS fk_table,"
                                     "fk.\"to\" AS fk_column_name, "
                                     "fk.id AS fk_id "
                                     "FROM "
                                     "pragma_table_info('%s') ti "
                                     "LEFT JOIN "
                                     "pragma_foreign_key_list('%s') fk "
                                     "ON ti.name = fk.\"from\";",
                                     QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_FK_ID] = {"SELECT "
                                "id "
                                "FROM "
                                "pragma_foreign_key_list('%s') "
                                "WHERE "
                                "\"from\" = ? AND \"table\" = ? AND \"to\" = ?;",
                                QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_ATTRIBUTE_IS_NULL] = {"SELECT "
                                            "%s IS NULL OR %s = '' "
                                            "FROM "
                                            "%s "
                                            "WHERE "
                                            "rowid = ?",
                                            QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_ATTRIBUTE_SIZE] = {"SELECT "
                                         "LENGTH(%s) "
                                         "FROM "
                                         "%s "
                                         "WHERE "
                                         "rowid = ?",
                                         QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_ATTRIBUTE] = {"SELECT "
                                    "%s "
                                    "FROM "
                                    "%s "
                                    "WHERE "
                                    "rowid = ?",
                                    QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_CHUNK_ATTRIBUTE] = {"SELECT "
                                          "substr(%s, %ld + 1, %ld) "
                                          "FROM "
                                          "%s "
                                          "WHERE "
                                          "rowid = ?",
                                          QUERY_TYPE_SINGLE},

    [QUERY_TPL_UPDATE_ATTRIBUTE] = {"UPDATE "
                                    "%s "
                                    "SET "
                                    "%s = ? "
                                    "WHERE "
                                    "rowid = ?",
                                    QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_TABLE_ROWIDS] = {"SELECT "
                                       "rowid "
                                       "FROM "
                                       "%s",
                                       QUERY_TYPE_SINGLE},

    [QUERY_TPL_SELECT_ROWID] = {"SELECT "
                                "rowid "
                                "FROM "
                                "%s "
                                "WHERE rowid = ?",
                                QUERY_TYPE_SINGLE},

    [QUERY_TPL_UPDATE_ZERO_BLOB] = {"UPDATE "
                                    "%s "
                                    "SET "
                                    "%s = CAST(IFNULL(%s, X'') || ? AS BLOB) "
                                    "WHERE "
                                    "rowid = ?",
                                    QUERY_TYPE_SINGLE},

    [QUERY_TPL_INSERT_RECORD_INTO_TABLE] = {"INSERT INTO "
                                            "%s (rowid) "
                                            "VALUES (%s)",
                                            QUERY_TYPE_SINGLE},

    [QUERY_TPL_CREATE_EMPTY_TABLE] = {"CREATE TABLE IF NOT EXISTS "
                                      "%s (rowid INTEGER PRIMARY KEY AUTOINCREMENT)",
                                      QUERY_TYPE_SINGLE},

    [QUERY_TPL_DROP_SCHEMA_COLUMN] = {"ALTER TABLE "
                                      "%s "
                                      "DROP COLUMN "
                                      "%s",
                                      QUERY_TYPE_SINGLE},

    [QUERY_TPL_ADD_PRIMARY_KEY_COLUMN] = {"ALTER TABLE "
                                          "%s "
                                          "ADD COLUMN "
                                          "%s %s PRIMARY KEY",
                                          QUERY_TYPE_SINGLE},

    [QUERY_TPL_ADD_ATTRIBUTE_COLUMN] = {"ALTER TABLE "
                                        "%s "
                                        "ADD COLUMN "
                                        "%s %s",
                                        QUERY_TYPE_SINGLE},

    [QUERY_TPL_ADD_FOREIGN_KEY_COLUMN] = {"PRAGMA foreign_keys=0; "
                                          "BEGIN TRANSACTION; "
                                          "ALTER TABLE %s RENAME TO %s_old; "
                                          "%s "
                                          "INSERT INTO %s SELECT *, '' FROM %s_old; "
                                          "DROP TABLE %s_old; "
                                          "COMMIT; "
                                          "PRAGMA foreign_keys=%d;",
                                          QUERY_TYPE_MULTI},

    [QUERY_TPL_DROP_TABLE] = {"DROP TABLE IF EXISTS %s;", QUERY_TYPE_SINGLE},

    [QUERY_TPL_DELETE_RECORD_FROM_TABLE] = {"DELETE FROM %s WHERE rowid = ?;", QUERY_TYPE_SINGLE},
};

char* qm_get_str(QueryID qid) {
    if (qid < 0 || qid >= QUERY_COUNT) {
        LOG_WARN("Invalid QueryID: %d", qid);
        return NULL;
    }

    return (char*)query_store[qid].sql;
}

sqlite3_stmt* qm_build_query_statement(sqlite3* db, QueryID qid, ...) {
    if (qid < 0 || qid >= QUERY_COUNT) {
        LOG_ERROR("Invalid QueryID: %d", qid);
        return NULL;
    }

    const char* tpl = query_store[qid].sql;
    char        buffer[2048];

    va_list args;
    va_start(args, qid);
    vsnprintf(buffer, sizeof(buffer), tpl, args);
    va_end(args);

    LOG_TRACE("Built query: %.100s%s", buffer, strlen(buffer) > 100 ? "..." : "");

    sqlite3_stmt* s = NULL;

    if (sqlite3_prepare_v2(db, buffer, -1, &s, NULL) != SQLITE_OK) {
        LOG_ERROR("Failed to prepare query %d: %s", qid, sqlite3_errmsg(db));
        return NULL;
    }

    return s;
}

status_t qm_exec_multi_stmt_query(sqlite3* db, QueryID qid, ...) {
    status_t status = STATUS_OK;

    if (qid < 0 || qid >= QUERY_COUNT) {
        LOG_ERROR("Invalid QueryID: %d", qid);
        status = STATUS_ILLEGAL_INSTRUCTION;
    }

    if (query_store[qid].type != 2) {
        LOG_ERROR("QueryID %d is not a multi-statement query", qid);
        status = STATUS_ILLEGAL_INSTRUCTION;
    }

    const char* tpl = query_store[qid].sql;
    char        buffer[4096];

    va_list args;
    va_start(args, qid);
    vsnprintf(buffer, sizeof(buffer), tpl, args);
    va_end(args);

    LOG_TRACE("Built multi-statement query: %s", buffer);

    TRY_SQLITE(db, sqlite3_exec(db, buffer, NULL, NULL, NULL), SQLITE_OK, cleanup,
               "Failed to execute multi-statement query %d: %s", qid, sqlite3_errmsg(db));

cleanup:
    return status;
}
