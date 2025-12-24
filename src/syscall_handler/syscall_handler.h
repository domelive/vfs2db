/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   syscall_handler.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (...)
 * @brief  Declaration of syscall handlers for the VFS2DB filesystem.
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

#ifndef SYSCALL_HANDLER_H
#define SYSCALL_HANDLER_H

#define FUSE_USE_VERSION 30

// Portability constants
#ifndef FUSE_FILL_DIR_DEFAULTS
#define FUSE_FILL_DIR_DEFAULTS 0
#endif

#include <fuse3/fuse.h>

#include "../db_handler/db_handler.h"

#define COUNT_CHAR(str, ch)                                                    \
  ({                                                                           \
    int count = 0;                                                             \
    for (int i = 0; str[i] != '\0'; i++) {                                     \
      if (str[i] == ch)                                                        \
        count++;                                                               \
    }                                                                          \
    count;                                                                     \
  })

void *vfs2db_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
void  vfs2db_destroy(void *private_data);

int vfs2db_getattr(const char *path, struct stat *st,
                   struct fuse_file_info *fi);
int vfs2db_getxattr(const char *path, const char *name, char *value,
                    size_t size);
int vfs2db_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi,
                   enum fuse_readdir_flags flags);
int vfs2db_read(const char *path, char *buffer, size_t size, off_t offset,
                struct fuse_file_info *fi);
int vfs2db_write(const char *path, const char *buffer, size_t size,
                 off_t offset, struct fuse_file_info *fi);
int vfs2db_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int vfs2db_readlink(const char *path, char *buffer, size_t size);

#endif // SYSCALL_HANDLER_H
