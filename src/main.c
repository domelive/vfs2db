/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   main.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (...)
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

sqlite3* db = NULL;
DbSchema* db_schema = NULL;

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
    const char *db_path;
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
    FUSE_OPT_END
};

int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct options opt = { NULL };

    if (fuse_opt_parse(&args, &opt, option_spec, NULL) == -1) {
        return 1;
    }

    if (opt.db_path == NULL) {
        fprintf(stderr, "Errore: Devi specificare il path del database come primo argomento.\n");
        fuse_opt_free_args(&args);
        return 1;
    }

    int check = sqlite3_open_v2(opt.db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (check != SQLITE_OK) {
        fprintf(stderr, "sqlite3_open_v2 failed: %s\n", sqlite3_errmsg(db));
        free(opt.db_path);
        fuse_opt_free_args(&args);
        return 1;
    }

    int res = fuse_main(args.argc, args.argv, &vfs2db_oper, NULL);

    free(opt.db_path);
    fuse_opt_free_args(&args);
    return res;
}