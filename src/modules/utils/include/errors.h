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

#include <errno.h>
#include <sqlite3.h>

/**
 * Status Codes
 *
 * @brief Enumeration of status codes used throughout the database handler.
 *
 * This enumeration includes general status codes (e.g., STATUS_OK, STATUS_ALLOC_ERROR) as well as
 * specific status codes for SQLite errors (e.g., STATUS_DB_BUSY, STATUS_DB_LOCKED). The status
 * codes are designed to provide a clear and consistent way to represent the outcome of operations.
 */
typedef enum status_t {
    STATUS_OK,
    STATUS_ISNULL,

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
 *
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

/**
 * TRY Macro
 *
 * @brief Macro to simplify error handling by checking the result of a function call and jumping
 * to a specified label if the result indicates an error.
 *
 * @param[in] call   The function call to execute and check for errors
 * @param[in] label  The label to jump to if an error occurs
 * @param[in] fmt    The format string for the error message (optional)
 * @param[in] ...    Additional arguments for the error message (optional)
 *
 * This macro evaluates the provided function call and checks if the result is not STATUS_OK.
 * If an error is detected, it logs an error message using the provided format string and
 * additional arguments, sets the status variable to the error code, and jumps to the specified
 * label for cleanup or further error handling.
 */
#define TRY(call, label, fmt, ...)                                                                 \
    do {                                                                                           \
        if ((status = (call)) != STATUS_OK) {                                                      \
            LOG_ERROR(fmt, ##__VA_ARGS__);                                                         \
            goto label;                                                                            \
        }                                                                                          \
    } while (0);

/**
 * TRY_NOT_NULL Macro
 *
 * @brief Macro to check if a pointer is not NULL and jump to a specified label if it is NULL.
 *
 * @param[in] ptr    The pointer to check for NULL
 * @param[in] label  The label to jump to if the pointer is NULL
 * @param[in] err_status The status_t error code to set if the pointer is NULL
 * @param[in] fmt    The format string for the error message (optional)
 * @param[in] ...    Additional arguments for the error message (optional)
 *
 * This macro checks if the provided pointer is NULL. If it is NULL, it logs an error message
 * using the provided format string and additional arguments, sets the status variable to the
 * specified error code, and jumps to the specified label for cleanup or further error handling.
 */
#define TRY_NOT_NULL(ptr, label, err_status, fmt, ...)                                             \
    do {                                                                                           \
        if (!(ptr)) {                                                                              \
            LOG_ERROR(fmt, ##__VA_ARGS__);                                                         \
            status = err_status;                                                                   \
            goto label;                                                                            \
        }                                                                                          \
    } while (0);

/**
 * TRY_SQLITE Macro
 * @brief Macro to check the result of an SQLite function call and jump to a specified label if the
 * result is not the expected OK status.
 *
 * @param[in] call       The SQLite function call to execute and check for errors
 * @param[in] ok_status   The expected SQLite status code indicating success (e.g., SQLITE_OK)
 * @param[in] label      The label to jump to if an error occurs
 * @param[in] fmt        The format string for the error message (optional)
 * @param[in] ...      Additional arguments for the error message (optional)
 *
 * This macro evaluates the provided SQLite function call and checks if the result is not equal to
 * the expected OK status. If an error is detected, it logs an error message using the provided
 * format string and additional arguments, sets the status variable to the corresponding status_t
 * error code based on the SQLite error code, and jumps to the specified label for cleanup or
 * further error handling.
 */
#define TRY_SQLITE(call, ok_status, label, fmt, ...)                                               \
    do {                                                                                           \
        int call_result = (call);                                                                  \
        if (call_result != ok_status) {                                                            \
            LOG_SQLITE_ERROR(db);                                                                  \
            LOG_ERROR(fmt, ##__VA_ARGS__);                                                         \
            status = sqlite_to_status(call_result);                                                \
            goto label;                                                                            \
        }                                                                                          \
    } while (0);

#endif // ERRORS_H