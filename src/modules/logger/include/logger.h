/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   logger.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Type definitions for the logger and related structures.
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

#ifndef LOGGER_H
#define LOGGER_H

#define TIME_STR_LEN 20
#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_DIM     "\x1b[2m"

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// =============================================================
// Log Levels
// =============================================================
typedef enum LogLevel {
    LOG_LEVEL_TRACE = 0, /**< Every single operation */
    LOG_LEVEL_DEBUG,     /**< Info for debugging */
    LOG_LEVEL_INFO,      /**< Normal info about operations */
    LOG_LEVEL_WARN,      /**< Info about handled abnormal events */
    LOG_LEVEL_ERROR,     /**< Info about errors that compromise a whole operation */
    LOG_LEVEL_FATAL,     /**< Info about errors that compromise the whole system */
    LOG_LEVEL_OFF        /**< No log at all */
} LogLevel;

// =============================================================
// Logger Configuration
// =============================================================
typedef struct LoggerConfig {
    LogLevel        min_level;      /**< Minimum level to log (If NULL then output will be stderr)*/
    FILE*           output;         /**< File Stream where the messages will be written */
    bool            show_timestamp; /**< Show [YYYY-MM-DD HH:MM:SS]? */
    bool            show_location;  /**< Show File:Line? */
    bool            show_colors;    /**< Show colors for different levels? */
    bool            show_thread_id; /**< Show thread ID? */
    bool            initialized;    /**< Initialization flag */
    pthread_mutex_t lock;           /**< Mutex for thread-safe logging */
} LoggerConfig;

/**
 * @brief Names for the logging levels.
 */
static const char* const LOG_LEVEL_NAMES[] = {"TRACE", "DEBUG", "INFO", "WARN",
                                              "ERROR", "FATAL", "OFF"};

/**
 * @brief ANSI colors for every logger level.
 */
static const char* const LEVEL_COLORS[] = {
    "\x1b[94m",   // TRACE: light blue
    "\x1b[36m",   // DEBUG: cyan
    "\x1b[32m",   // INFO:  green
    "\x1b[33m",   // WARN:  yellow
    "\x1b[31;1m", // ERROR: red bold
    "\x1b[35;1m", // FATAL: magenta bold
};

// =============================================================
// Gloabal Logger Configuration
// =============================================================
extern LoggerConfig g_logger;

// =============================================================
// Initialization And Cleanup Functions
// =============================================================

/**
 * @brief Initializes the logger with default settings.
 *
 * Must be called BEFORE any other operation.
 * Default settings: logs INFO and above to stderr, with timestamp, location, colors and thread ID.
 *
 * @return 0 if initialization was successful, -1 otherwise.
 */
int logger_init_default(void);

/**
 * @brief Initializes the logger with custom settings.
 *
 * Must be called BEFORE any other operation.
 *
 * @param level             Minimum log level to log.
 * @param log_file_path     Path to the log file (if NULL, defaults to stderr).
 * @param show_timestamp    Whether to show timestamps in logs.
 * @param show_location     Whether to show file and line number in logs.
 * @param show_colors       Whether to use colors in logs.
 * @param show_thread_id    Whether to show thread ID in logs.
 *
 * @return 0 if initialization was successful, -1 otherwise.
 */
int logger_init(LogLevel level, const char* log_file_path, bool show_timestamp, bool show_location,
                bool show_colors, bool show_thread_id);

/**
 * @brief Cleans up the logger resources.
 */
void logger_cleanup(void);

/**
 * @brief Sets the minimum log level at runtime.
 *
 * @param level New minimum log level.
 */
void logger_set_level(LogLevel level);

/**
 * @brief Gets the current minimum log level.
 *
 * @return Current minimum log level.
 */
LogLevel logger_get_level(void);

/**
 * @brief Converts a string to a LogLevel.
 *
 * @param str String representation of the log level.
 * @return Corresponding LogLevel.
 */
LogLevel logger_get_level_from_string(const char* str);

// =============================================================
// Main Logging Function
// =============================================================

/**
 * @brief Main logging function.
 *
 * @param level     Log level of the message.
 * @param file_path Source file path where the log was called.
 * @param line      Line number in the source file.
 * @param func      Function name where the log was called.
 * @param fmt       Format string (like printf).
 * @param ...       Additional arguments for the format string.
 */
void logger_write(LogLevel level, const char* file_path, int line, const char* func,
                  const char* fmt, ...);

// =============================================================
// Generic Logging Macroes
// =============================================================

// Generic logging macro
#define LOG(level, ...)                                                                            \
    do {                                                                                           \
        if (g_logger.initialized && (level) >= g_logger.min_level) {                               \
            logger_write(level, __FILE__, __LINE__, __func__, __VA_ARGS__);                        \
        }                                                                                          \
    } while (0)

// Specific level macros
#define LOG_TRACE(...) LOG(LOG_LEVEL_TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  LOG(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(...)  LOG(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(...) LOG(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_FATAL(...) LOG(LOG_LEVEL_FATAL, __VA_ARGS__)

// =============================================================
// Specific Logging Macroes
// =============================================================

// Specific log for SQLITE errors
#define LOG_SQLITE_ERROR(db) LOG_ERROR("SQLite error: %s", sqlite3_errmsg(db))

// Specific logs for FUSE operations
#define LOG_FUSE_ENTER(op, path)  LOG_DEBUG("FUSE %s(\"%s\")", (op), (path))
#define LOG_FUSE_EXIT(op, result) LOG_DEBUG("FUSE %s -> %d", (op), (result))

// Macro for conditional logging
#define LOG_DEBUG_IF(cond, ...)                                                                    \
    do {                                                                                           \
        if (cond)                                                                                  \
            LOG_DEBUG(__VA_ARGS__);                                                                \
    } while (0)

// Macro for assert logging
#define LOG_ASSERT(cond, ...)                                                                      \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            LOG_FATAL("Assertion failed: " #cond);                                                 \
            LOG_FATAL(__VA_ARGS__);                                                                \
            abort();                                                                               \
        }                                                                                          \
    } while (0)

#endif // LOGGER_H