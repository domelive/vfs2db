/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   types.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Type definitions for database schema and related structures.
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

#ifndef HELPERS_H
#define HELPERS_H

#include <assert.h>
#include <sqlite3.h>
#include <stdbool.h>

#include "logger.h"
#include "uthash.h"

// =============================================================
// Macros
// =============================================================

/**
 * FOREACH
 *
 * @brief Macro to iterate over all elements in a UTHash hash table.
 *
 * @param[in] current The iterator variable for the current element
 * @param[in] head    The head of the UTHash hash table
 */
#define HASH_FOREACH(current, head)                                                                \
    for (typeof(head) current = NULL, _tmp_##current = NULL, _once_##current = (typeof(head))1;    \
         _once_##current; _once_##current = NULL)                                                  \
    HASH_ITER(hh, head, current, _tmp_##current)

/**
 * sqlite_str_to_type
 *
 * @brief Parses a SQLite column type string and determines the corresponding SQLite type affinity.
 *
 * SQLite uses type affinity to determine how to store and compare values in a column, based on the
 * declared type of the column. This function analyzes the type string and returns the appropriate
 * SQLite type affinity constant (e.g., SQLITE_INTEGER, SQLITE_TEXT, SQLITE_BLOB, SQLITE_FLOAT)
 * based on the presence of certain keywords in the type string.
 *
 * @return The SQLite type affinity constant corresponding to the given type string
 */
static inline int sqlite_str_to_type(const char* typestr) {
    LOG_TRACE("Parsing SQLITE type...");

    if (!typestr) {
        LOG_TRACE("Column has no type affinity, defaulting to TEXT");
        return SQLITE_TEXT;
    }

    if (strcasestr(typestr, "INT")) {
        LOG_TRACE("Column type '%s' has INTEGER affinity", typestr);
        return SQLITE_INTEGER;
    }

    if (strcasestr(typestr, "CHAR") || strcasestr(typestr, "CLOB") || strcasestr(typestr, "TEXT")) {
        LOG_TRACE("Column type '%s' has TEXT affinity", typestr);
        return SQLITE_TEXT;
    }

    if (strcasestr(typestr, "BLOB")) {
        LOG_TRACE("Column type '%s' has BLOB affinity", typestr);
        return SQLITE_BLOB;
    }

    if (strcasestr(typestr, "REAL") || strcasestr(typestr, "FLOA") || strcasestr(typestr, "DOUB")) {
        LOG_TRACE("Column type '%s' has FLOAT affinity", typestr);
        return SQLITE_FLOAT;
    }

    LOG_TRACE("Fallback to SQLITE_TEXT");

    return SQLITE_TEXT;
}

/**
 * sqlite_type_to_str
 *
 * @brief Convert SQLite Type Integer to String Representation
 */
static inline const char* sqlite_type_to_str(int sqlite_type) {
    switch (sqlite_type) {
    case SQLITE_INTEGER:
        return "INTEGER";
    case SQLITE_FLOAT:
        return "FLOAT";
    case SQLITE_TEXT:
        return "TEXT";
    case SQLITE_BLOB:
        return "BLOB";
    case SQLITE_NULL:
        return "NULL";
    default:
        return "UNKNOWN";
    }
}

#endif // HELPERS_H
