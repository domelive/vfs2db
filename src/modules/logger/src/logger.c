/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   logger.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Implementation of the logger and related functions.
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
#include "logger.h"

// =============================================================
// Global Logger Configuration
// =============================================================
LoggerConfig g_logger = {.min_level      = LOG_LEVEL_INFO,
                         .output         = NULL,
                         .show_timestamp = true,
                         .show_location  = true,
                         .show_colors    = true,
                         .show_thread_id = true,
                         .initialized    = false,
                         .lock           = PTHREAD_MUTEX_INITIALIZER};

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
int logger_init_default(void) { return logger_init(LOG_LEVEL_INFO, NULL, true, true, true, true); }

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
                bool show_colors, bool show_thread_id) {
    FILE* output = stderr;
    if (log_file_path != NULL) {
        output = fopen(log_file_path, "a");
        if (output == NULL) {
            fprintf(stderr, "Logger initialization failed: cannot open log file %s\n",
                    log_file_path);
            return -1;
        }
    }

    // set configuration
    g_logger.min_level      = level;
    g_logger.output         = output;
    g_logger.show_timestamp = show_timestamp;
    g_logger.show_location  = show_location;
    g_logger.show_colors    = show_colors;
    g_logger.show_thread_id = show_thread_id;

    pthread_mutex_init(&g_logger.lock, NULL);
    g_logger.initialized = true;

    LOG_INFO("Logger initialized with level %s", LOG_LEVEL_NAMES[level]);

    return 0;
}

/**
 * @brief Cleans up the logger resources.
 */
void logger_cleanup(void) {
    assert(g_logger.initialized && "Logger not initialized");

    if (g_logger.output && g_logger.output != stderr && g_logger.output != stdout) {
        fclose(g_logger.output);
    }

    pthread_mutex_destroy(&g_logger.lock);
    g_logger.initialized = false;
}

/**
 * @brief Sets the minimum log level at runtime.
 *
 * @param level New minimum log level.
 */
void logger_set_level(LogLevel level) {
    assert(g_logger.initialized && "Logger not initialized");

    pthread_mutex_lock(&g_logger.lock);
    g_logger.min_level = level;
    pthread_mutex_unlock(&g_logger.lock);
}

/**
 * @brief Gets the current minimum log level.
 *
 * @return Current minimum log level.
 */
LogLevel logger_get_level(void) {
    assert(g_logger.initialized && "Logger not initialized");

    pthread_mutex_lock(&g_logger.lock);
    LogLevel level = g_logger.min_level;
    pthread_mutex_unlock(&g_logger.lock);
    return level;
}

/**
 * @brief Converts a string to a LogLevel.
 *
 * @param str String representation of the log level.
 * @return Corresponding LogLevel.
 */
LogLevel logger_get_level_from_string(const char* str) {
    assert(g_logger.initialized && "Logger not initialized");

    pthread_mutex_lock(&g_logger.lock);

    if (!str) {
        pthread_mutex_unlock(&g_logger.lock);
        return LOG_LEVEL_INFO;
    }

    LogLevel ret = LOG_LEVEL_INFO;

    if (strcasecmp(str, "trace") == 0)
        ret = LOG_LEVEL_TRACE;
    if (strcasecmp(str, "debug") == 0)
        ret = LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "info") == 0)
        ret = LOG_LEVEL_INFO;
    if (strcasecmp(str, "warn") == 0)
        ret = LOG_LEVEL_WARN;
    if (strcasecmp(str, "error") == 0)
        ret = LOG_LEVEL_ERROR;
    if (strcasecmp(str, "fatal") == 0)
        ret = LOG_LEVEL_FATAL;
    if (strcasecmp(str, "off") == 0)
        ret = LOG_LEVEL_OFF;

    pthread_mutex_unlock(&g_logger.lock);

    return ret;
}

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
                  const char* fmt, ...) {
    assert(g_logger.initialized && "Logger not initialized");

    pthread_mutex_lock(&g_logger.lock);

    FILE* out = g_logger.output ? g_logger.output : stderr;

    // Timestamp
    if (g_logger.show_timestamp) {
        time_t     now     = time(NULL);
        struct tm* tm_info = localtime(&now);
        char       time_buf[20];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(out, "[%s] ", time_buf);
    }

    if (g_logger.show_thread_id) {
        fprintf(out, "[TID:%lu] ", (unsigned long)(pthread_self() % 10000));
    }

    // color handling and level name
    if (g_logger.show_colors) {
        fprintf(out, "%s%-5s%s ", LEVEL_COLORS[level], LOG_LEVEL_NAMES[level], ANSI_RESET);
    } else {
        fprintf(out, "%-5s ", LOG_LEVEL_NAMES[level]);
    }

    // Location handling
    if (g_logger.show_location) {
        const char* filename = strrchr(file_path, '/');
        filename             = filename ? filename + 1 : file_path;
        fprintf(out, "[%s:%d %s()] ", filename, line, func);
    }

    // Message
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);

    pthread_mutex_unlock(&g_logger.lock);
}