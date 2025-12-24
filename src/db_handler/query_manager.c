/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   query_manager.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (...)
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
 * Query String Store
 * 
 * @brief Array of SQL query strings indexed by QueryID.
 * 
 */
static const char* sql_store[] = {
    [QUERY_GET_TABLES_NAME] = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';",
    [QUERY_GET_TABLE_INFO]  = "SELECT " 
                                    "ti.name AS column_name,"
                                    "ti.pk AS is_pk,"
                                    "fk.\"table\" AS fk_table,"
                                    "fk.\"to\" AS fk_column_name "
                              "FROM "
                                    "pragma_table_info('%s') ti "
                                    "LEFT JOIN "
                                    "pragma_foreign_key_list('%s') fk "
                              "ON ti.name = fk.\"from\";",
};

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
char *qm_get_query_str(QueryID qid) {
    if (qid < 0 || qid >= QUERY_COUNT) return NULL;
    return sql_store[qid];
}