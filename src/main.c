/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   main.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Main entry point for the VFS2DB filesystem.
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

#include "syscall_handler.h"
#include "logger.h"

sqlite3* db = NULL;         /**< Database connection handle */
DbSchema* db_schema = NULL; /**< Database schema structure */

/**
 * @brief FUSE operations structure mapping filesystem calls to handler functions.
 * 
 * Each member of this structure corresponds to a specific filesystem operation,
 * and is assigned to the appropriate handler function defined in syscall_handler.h.
 * 
 */
static const struct fuse_operations vfs2db_oper = {
	.getattr        = vfs2db_getattr,
    .getxattr       = vfs2db_getxattr,
	.readdir        = vfs2db_readdir,
	.read           = vfs2db_read,
    .write          = vfs2db_write,
    .create         = vfs2db_create,
    .readlink       = vfs2db_readlink,
    .init           = vfs2db_init,
    .destroy        = vfs2db_destroy,
};

/**
 * @brief Structure to hold command-line options.
 * 
 * This structure is used to parse and store command-line options
 * provided to the FUSE filesystem, specifically the database path.
 */
struct options {
    const char* db_path;
    const char* log_level;
    const char* log_file;
};

/**
 * @brief FUSE option specifications.
 * 
 * This array defines the command-line options that can be passed to the FUSE filesystem.
 * The "db=%s" option allows the user to specify the path to the database file.
 */
#define OPTION(t, p) { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("db=%s", db_path),
    OPTION("log=%s", log_level),
    OPTION("logfile=%s", log_file),
    FUSE_OPT_END
};

int main(int argc, char *argv[]) {
    if (logger_init_default() != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }

    LOG_INFO("=== VFS2DB Filesystem Starting ===");
    LOG_INFO("Version 0.0.2");

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct options opt = { NULL, NULL, NULL };

    if (fuse_opt_parse(&args, &opt, option_spec, NULL) == -1) {
        LOG_FATAL("Failed to parse command-line arguments.");
        logger_cleanup();
        return 1;
    }

    // reconfigure logger if specified in command line
    if (opt.log_level != NULL || opt.log_file != NULL) {
        LogLevel level = opt.log_level
            ? logger_get_level_from_string(opt.log_level)
            : LOG_LEVEL_INFO;

        LOG_INFO("Reconfiguring logger: level=%s, file=%s",
            LOG_LEVEL_NAMES[level],
            opt.log_file ? opt.log_file : "(none)");

        if (logger_init(level, opt.log_file, true, true, true, true) != 0)
            LOG_ERROR("Failed to reconfigure logger, continuing with default settings.");
    }

    LOG_DEBUG("Parsing command-line arguments...");

    if (opt.db_path == NULL) {
        LOG_FATAL("Database path not specified");
        LOG_INFO("Usage: %s -o db=<path> [mount_point]", argv[0]);
        LOG_INFO("Options:");
        LOG_INFO("  -o db=<path>       Path to SQLite database (required)");
        LOG_INFO("  -o log=<level>     Log level: trace,debug,info,warn,error,fatal");
        LOG_INFO("  -o logfile=<path>  Log to file in addition to stderr");

        fuse_opt_free_args(&args);
        logger_cleanup();
        return 1;
    }

    LOG_INFO("Opening SQLite database...: %s", opt.db_path);

    int check = sqlite3_open_v2(opt.db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (check != SQLITE_OK) {
        LOG_FATAL("Cannot open database: %s", sqlite3_errmsg(db));
        // free(opt.db_path);
        fuse_opt_free_args(&args);
        logger_cleanup();
        return 1;
    }

    LOG_INFO("Database opened successfully");

    LOG_INFO("Mounting FUSE filesystem...");
    LOG_DEBUG("FUSE arguments: argc=%d", args.argc);
    for (int i = 0; i < args.argc; i++) {
        LOG_TRACE("  argv[%d]: %s", i, args.argv[i]);
    }

    int res = fuse_main(args.argc, args.argv, &vfs2db_oper, NULL);

    LOG_INFO("FUSE main loop has exited with code: %d", res);
    LOG_INFO("Unmounting FUSE filesystem...");

    free(opt.db_path);
    fuse_opt_free_args(&args);

    LOG_INFO("=== VFS2DB Filesystem Shutdown Complete ===");
    logger_cleanup();

    return res;
}