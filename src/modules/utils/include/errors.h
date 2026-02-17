/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Database Handler Source File
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

#ifndef ERRORS_H
#define ERRORS_H

#include <sqlite3.h>
#include <errno.h>

/**
 * Status Codes
 *
 * @brief Enumeration of status codes used throughout the database handler.
 */
typedef enum status_t {
    STATUS_OK,
    
    STATUS_ALLOC_ERROR,

    STATUS_DB_ERROR,
    STATUS_DB_BUSY,
    STATUS_DB_LOCKED,
    STATUS_DB_READONLY,
    STATUS_DB_IOERROR,
    STATUS_DB_CORRUPT,
    STATUS_DB_FULL,
    STATUS_DB_CANTOPEN,
    STATUS_DB_PERMISSION,
    STATUS_DB_NOTFOUND,
    
    STATUS_CACHE_FULL,
    STATUS_CACHE_ERROR,
} status_t;

/**
 * Convert SQLite error code to status_t
 *
 * @param[in] sqlite_code The SQLite error code to convert
 *
 * @return Corresponding status_t value
 */
static inline status_t sqlite_to_status(int sqlite_code) {
    switch (sqlite_code) {
    case SQLITE_OK:
        return STATUS_OK;
    case SQLITE_BUSY:
        return STATUS_DB_BUSY;
    case SQLITE_LOCKED:
        return STATUS_DB_LOCKED;
    case SQLITE_READONLY:
        return STATUS_DB_READONLY;
    case SQLITE_IOERR:
        return STATUS_DB_IOERROR;
    case SQLITE_CORRUPT:
        return STATUS_DB_CORRUPT;
    case SQLITE_FULL:
        return STATUS_DB_FULL;
    case SQLITE_CANTOPEN:
        return STATUS_DB_CANTOPEN;
    case SQLITE_PERM:
        return STATUS_DB_PERMISSION;
    case SQLITE_NOTFOUND:
        return STATUS_DB_NOTFOUND;
    default:
        return STATUS_DB_ERROR;
    }
}

/**
 * Convert status_t to errno code
 * 
 * @param[in] status The status_t value to convert
 * @return Corresponding errno code (negative value)
 */
static inline int status_to_errno(status_t status) {
    switch (status) {
    case STATUS_OK:
        return 0;
    case STATUS_DB_BUSY:
        return -EBUSY;
    case STATUS_DB_LOCKED:
        return -EDEADLK;
    case STATUS_DB_READONLY:
        return -EROFS;
    case STATUS_DB_IOERROR:
    case STATUS_DB_CORRUPT:
        return -EIO;
    case STATUS_DB_FULL:
        return -ENOSPC;
    case STATUS_DB_CANTOPEN:
        return -ENOENT;
    case STATUS_DB_PERMISSION:
        return -EACCES;
    case STATUS_DB_NOTFOUND:
        return -ENOENT;
    default:
        return -EIO;
    }
}

#define TRY(call, label, fmt, ...) \
    do { \
        if ((status = (call)) != STATUS_OK) { \
            LOG_ERROR(fmt, ##__VA_ARGS__); \
            goto label; \
        } \
    } while(0);

#define TRY_NOT_NULL(ptr, label, err_status, fmt, ...) \
    do { \
        if (!(ptr)) { \
            LOG_ERROR(fmt, ##__VA_ARGS__); \
            status = err_status; \
            goto label; \
        } \
    } while(0);

#define TRY_SQLITE(call, ok_status, label, fmt, ...) \
    do { \
        int call_result = (call); \
        if (call_result != ok_status) { \
            LOG_SQLITE_ERROR(db); \
            status = sqlite_to_status(call_result); \
            goto label; \
        } \
    } while(0);


#endif // ERRORS_H