/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   syscall_handler.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
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

#include <fuse3/fuse.h>

// Portability constants
#ifndef FUSE_FILL_DIR_DEFAULTS
#define FUSE_FILL_DIR_DEFAULTS 0
#endif

#include "arena.h"
#include "const.h"
#include "db_handler.h"
#include "helpers.h"
#include "logger.h"
#include "parser.h"

/**
 * VFS2DB Init
 *
 * @brief Initializes the VFS2DB filesystem, setting up the database connection and loading the
 * schema.
 *
 * @param[in] conn    Pointer to fuse_conn_info structure (not used in this case)
 * @param[in] cfg     Pointer to fuse_config structure (not used in this case)
 *
 * @return Pointer to private data
 */
void* vfs2db_init(struct fuse_conn_info* conn, struct fuse_config* cfg);

/**
 * VFS2DB Destroy
 *
 * @brief Cleans up resources used by the VFS2DB filesystem, including closing the database
 * connection and freeing the schema.
 *
 * @param[in] private_data Pointer to private data (not used in this case)
 *
 * @return void
 */
void vfs2db_destroy(void* private_data);

/**
 * VFS2DB Getattr
 *
 * @brief Retrieves the attributes of a file or directory in the VFS2DB filesystem.
 *
 * @param[in]  path    The file or directory path
 * @param[out] st      Pointer to a stat structure to be filled with attributes
 * @param[in]  fi      Pointer to fuse_file_info structure (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_getattr(const char* path, struct stat* st, struct fuse_file_info* fi);

/**
 * VFS2DB Getxattr
 *
 * @brief Retrieves the value of an extended attribute for a file in the VFS2DB filesystem.
 *
 * @param[in]  path    The file path
 * @param[in]  name    The name of the extended attribute
 * @param[out] value   Buffer to store the attribute value
 * @param[in]  size    Size of the buffer
 *
 * @return Size of the attribute value on success, negative error code on failure
 */
int vfs2db_getxattr(const char* path, const char* name, char* value, size_t size);

/**
 * VFS2DB Setxattr
 *
 * @brief Sets the value of an extended attribute for a file in the VFS2DB filesystem.
 *
 * @param[in]  path    The file path
 * @param[in]  name    The name of the extended attribute
 * @param[in]  value   Buffer containing the attribute value to set
 * @param[in]  size    Size of the attribute value
 * @param[in]  flags   Flags for setting the attribute (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_setxattr(const char* path, const char* name, const char* value, size_t size, int flags);

/**
 * VFS2DB Utimens
 *
 * @brief Updates the access and modification times of a file in the VFS2DB filesystem. This
 * function is currently a placeholder and does not perform any actual time updates, as file
 * timestamps are not modifiable in this implementation.
 *
 * @param[in] path    The file path
 * @param[in] tv      Array of two timespec structures containing the new access and modification
 *                    times (not used in this case)
 * @param[in] fi      Pointer to fuse_file_info structure (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi);

/**
 * VFS2DB Readdir
 *
 * @brief Reads the contents of a directory in the VFS2DB filesystem, listing tables, records, or
 * attributes based on the directory level.
 *
 * @param[in]  path    The directory path
 * @param[out] buffer  Buffer to store the directory entries
 * @param[in]  filler  Function pointer to fill the buffer with directory entries
 * @param[in]  offset  Offset for reading the directory (not used in this case)
 * @param[in]  fi      Pointer to fuse_file_info structure (not used in this case)
 * @param[in]  flags   Flags for reading the directory (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset,
                   struct fuse_file_info* fi, enum fuse_readdir_flags flags);

/**
 * VFS2DB Mkdir
 *
 * @brief Creates a new directory in the VFS2DB filesystem, which corresponds to creating a new
 * table in the database or a new record.
 *
 * @param[in] path    The directory path to create
 * @param[in] mode    The directory mode (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_mkdir(const char* path, mode_t mode);

/**
 * VFS2DB Rmdir
 *
 * @brief Removes a directory in the VFS2DB filesystem, which corresponds to deleting a table or a
 * record from the database.
 *
 * @param[in] path    The directory path to remove
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_rmdir(const char* path);

/**
 * VFS2DB Create
 *
 * @brief Creates a new file in the VFS2DB filesystem, which corresponds to creating a new record in
 * the database. This function is currently a placeholder and not implemented yet, as record
 * creation logic is not implemented in the database handler yet.
 *
 * @param[in] path    The file path corresponding to the new record
 * @param[in] mode    The file mode (not used in this case)
 * @param[in] fi      Pointer to fuse_file_info structure (not used in this case
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_create(const char* path, mode_t mode, struct fuse_file_info* fi);

/**
 * VFS2DB Open
 *
 * @brief Opens a file in the VFS2DB filesystem. This function is currently a placeholder and does
 * not perform any actual opening logic, as the filesystem is stateless. The open operation is
 * effectively a no-op in this implementation, as all necessary checks and preparations are handled
 * in the getattr and readdir handlers.
 *
 * @param[in] path    The file path to open
 * @param[in] fi      Pointer to fuse_file_info structure (not used in this case)
 * @return 0 on success, negative error code on failure
 */
int vfs2db_open(const char* path, struct fuse_file_info* fi);

/**
 * VFS2DB Read
 *
 * @brief Reads data from a file in the VFS2DB filesystem, retrieving the value of an attribute.
 *
 * @param[in]  path    The file path
 * @param[out] buffer  Buffer to store the read data
 * @param[in]  size    Size of the buffer
 * @param[in]  offset  Offset within the file to start reading
 * @param[in]  fi      Pointer to fuse_file_info structure (not used in this case)
 *
 * @return Number of bytes read on success, negative error code on failure
 */
int vfs2db_read(const char* path, char* buffer, size_t size, off_t offset,
                struct fuse_file_info* fi);

/**
 * VFS2DB Symlink
 *
 * @brief Creates a symbolic link in the VFS2DB filesystem, which corresponds to creating a foreign
 * key reference in the database. This function is currently a placeholder and not implemented yet,
 * as the logic for handling foreign key references and their corresponding symbolic links is not
 * implemented yet.
 *
 * @param[in] target  The target path that the symbolic link points to (not used in this case)
 * @param[in] linkpath The symbolic link path to create (not used in this case
 * @return 0 on success, negative error code on failure
 */
int vfs2db_symlink(const char* target, const char* linkpath);

/**
 * VFS2DB Unlink
 *
 * @brief Deletes a file in the VFS2DB filesystem, which corresponds to deleting an attribute or
 * record from the database. This function is currently a placeholder and not implemented yet.
 *
 * @param[in] path    The file path to delete
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_unlink(const char* path);

/**
 * VFS2DB Write
 *
 * @brief Writes data to a file in the VFS2DB filesystem, updating the value of an attribute.
 *
 * @param[in] path    The file path
 * @param[in] buffer  Buffer containing the data to write
 * @param[in] size    Size of the data to write
 * @param[in] offset  Offset within the file to start writing
 * @param[in] fi      Pointer to fuse_file_info structure (not used in this case)
 *
 * @return Number of bytes written on success, negative error code on failure
 */
int vfs2db_write(const char* path, const char* buffer, size_t size, off_t offset,
                 struct fuse_file_info* fi);

/**
 * VFS2DB Truncate
 *
 * @brief Truncates a file in the VFS2DB filesystem, which corresponds to resizing an attribute
 * value in the database. This function is currently a placeholder and not implemented yet.
 *
 * @param[in] path    The file path to truncate
 * @param[in] size    The new size of the file
 * @param[in] fi      Pointer to fuse_file_info structure (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_truncate(const char* path, off_t size, struct fuse_file_info* fi);

/**
 * VFS2DB Readlink
 *
 * @brief Reads the target of a symbolic link in the VFS2DB filesystem, resolving foreign key
 * references to their target paths.
 *
 * @param[in] path    The symbolic link path
 * @param[out] buffer Buffer to store the link target
 * @param[in] size    Size of the buffer
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_readlink(const char* path, char* buffer, size_t size);

#endif // SYSCALL_HANDLER_H
