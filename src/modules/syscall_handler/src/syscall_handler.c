/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   syscall_handler.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Implementation of syscall handlers for the VFS2DB filesystem.
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

extern sqlite3*  db;        /**< Database connection handle */
extern DbSchema* db_schema; /**< Database schema structure */

static __thread Arena* arena = NULL; /**< Thread-local memory arena for efficient allocations */

static inline void ensure_arena_init() {
    if (!arena) {
        LOG_DEBUG("Creating thread-local arena");

        arena = arena_create(ARENA_DEFAULT_SIZE);
        if (!arena) {
            LOG_FATAL("Failed to create thread-local arena");
            exit(EXIT_FAILURE);
        }
    }

    LOG_TRACE("Resetting thread-local arena for reuse");
    arena_reset(arena);
}

static inline struct tokens* tokenize_path(const char* path) {
    // We expect paths in the form of /table/record/attribute.vfs2db, so we can have at most 3
    // tokens (table, record, attribute). We will ignore the .vfs2db extension in this function and
    // handle it separately.
    if (!path) {
        LOG_WARN("tokenize_path called with NULL path");
        return NULL;
    }

    // Allocate tokens structure in arena
    struct tokens* toks = arena_alloc(arena, sizeof(struct tokens));
    if (!toks) {
        LOG_ERROR("Failed to allocate tokens struct");
        return NULL;
    }

    // Make a mutable copy of the path in the arena for tokenization
    char* path_copy = arena_strdup(arena, path);
    if (!path_copy) {
        LOG_ERROR("Failed to duplicate path string");
        return NULL;
    }

    // Skip leading slash if present
    char* cursor = path_copy;
    if (cursor[0] == '/')
        cursor++;

    // Tokenize the path using '/' as a delimiter
    char* t         = strtok(cursor, "/");
    toks->table     = t ? arena_strdup(arena, t) : NULL;
    t               = strtok(NULL, "/");
    toks->record    = t ? arena_strdup(arena, t) : NULL;
    t               = strtok(NULL, "/");
    toks->attribute = t ? arena_strdup(arena, t) : NULL;

    LOG_TRACE("Tokenized path '%s': table=%s, record=%s, attr=%s", path,
              toks->table ? toks->table : "(null)", toks->record ? toks->record : "(null)",
              toks->attribute ? toks->attribute : "(null)");

    return toks;
}

static inline char* remove_extension(const char* path) {
    // We expect paths to end with ".vfs2db" for files, so we can remove the last 7 characters to
    // get the base path for tokenization. If the path does not end with ".vfs2db", we will treat it
    // as a directory path and return it as is (after duplicating it in the arena).
    int noext_path_length = strlen(path) - 7;
    if (noext_path_length <= 0) {
        LOG_WARN("Path too short to have extension: %s", path);
        return NULL;
    }

    // Allocate new string in arena for path without extension
    char* noext_path = arena_alloc(arena, noext_path_length + 1);
    if (!noext_path) {
        LOG_ERROR("Failed to allocate path without extension");
        return NULL;
    }

    strncpy(noext_path, path, noext_path_length);
    noext_path[noext_path_length] = 0;

    return noext_path;
}

static inline int check_symlink(struct tokens* toks) {
    // Check if table exists in schema
    Schema* table = find_schema_by_name(db_schema, toks->table);
    if (!table) {
        LOG_WARN("Table not found in schema: %s", toks->table);
        return 0;
    }

    // Check if attribute is a foreign key in the table schema
    if (find_fk_by_name(table, toks->attribute)) {
        LOG_TRACE("Attribute '%s' is a foreign key (symlink)", toks->attribute);
        return 1;
    }

    return 0;
}

void* vfs2db_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    LOG_INFO("Initializing VFS2DB filesystem...");

    // Get database path from FUSE private data
    const char* db_path = (const char*)fuse_get_context()->private_data;
    if (!db_path) {
        LOG_FATAL("Database path not provided in FUSE private data");
        return NULL;
    }

    // Initialize query manager
    if (qm_init(db) != STATUS_OK) {
        LOG_FATAL("Failed to initialize Query Manager");
        return NULL;
    }

    // Load database schema
    db_schema = malloc(sizeof(DbSchema));
    if (!db_schema) {
        LOG_FATAL("Failed to allocate DbSchema");
        return NULL;
    }

    // Initialize the schema structure
    if (init_db_schema(db_schema) != STATUS_OK) {
        LOG_FATAL("Failed to initialize database schema");
        free(db_schema);
        db_schema = NULL;
        return NULL;
    }
    LOG_INFO("Found %d tables in database", count_schemas(db_schema));

    // For each table, get all the info
    HASH_FOREACH(current_schema, db_schema->tables_head) {
        LOG_DEBUG("Loading schema for table: %s", current_schema->name);
        if (init_schema(current_schema) != STATUS_OK) {
            LOG_ERROR("Failed to init schema for table: %s", current_schema->name);
        }
    }

    // Log schema summary at debug level
    if (logger_get_level() <= LOG_LEVEL_DEBUG) {
        LOG_DEBUG("=== Database Schema Summary ===");
        HASH_FOREACH(current_schema, db_schema->tables_head) {
            LOG_DEBUG("Table: %s (PKs=%d, FKs=%d, Attrs=%d)", current_schema->name,
                      count_pks(current_schema), count_fks(current_schema),
                      count_attributes(current_schema));
        }
        LOG_DEBUG("===============================");
    }

    LOG_INFO("=== VFS2DB initialization complete ===");
    return NULL;
}

void vfs2db_destroy(void* private_data) {
    LOG_INFO("Shutting down VFS2DB filesystem...");

    qm_cleanup();

    if (db) {
        sqlite3_close(db);
        LOG_DEBUG("SQLite database closed");
        db = NULL;
    }

    if (db_schema) {
        HASH_FOREACH(current_schema, db_schema->tables_head) {
            LOG_TRACE("Freeing schema: %s", current_schema->name);
            free_attr_set(current_schema);
            free_pk_set(current_schema);
            free_fk_hashmap(current_schema);
            free(current_schema->name);
        }
        free_schema_hashmap(db_schema);
        free(db_schema);
        db_schema = NULL;
        LOG_DEBUG("Database schema freed");
    }

    // Free FUSE args if passed as private_data
    if (private_data) {
        struct fuse_args* args = (struct fuse_args*)private_data;
        fuse_opt_free_args(args);
        LOG_DEBUG("FUSE arguments freed");
    }

    if (arena) {
        LOG_DEBUG("Destroying thread-local arena");
        arena_destroy(arena);
        arena = NULL;
    }
}

int vfs2db_getattr(const char* path, struct stat* st, struct fuse_file_info* fi) {
    (void)fi;
    status_t status = STATUS_OK;

    LOG_FUSE_ENTER("getattr", path);

    ensure_arena_init();

    memset(st, 0, sizeof(*st));

    // If path doesn't end with .vfs2db, it's a directory
    if (strncmp(&path[strlen(path) - 7], ".vfs2db", 7)) {
        if (strstr(path, ".Trash") != NULL) {
            LOG_TRACE("The system asked for .Trash-xxxx directory");
            return -ENOENT;
        }

        // tokenize path to get table, record, and attribute for schema lookup
        struct tokens* toks = tokenize_path(path);
        if (!toks) {
            LOG_ERROR("Failed to tokenize path: %s", path);
            LOG_FUSE_EXIT("getattr", -ENOMEM);
            return -ENOMEM;
        }

        if (toks->table) {
            // Check if table exists in schema
            Schema* table = find_schema_by_name(db_schema, toks->table);
            if (!table) {
                LOG_WARN("Table not found in schema: %s", toks->table);
                LOG_FUSE_EXIT("getattr", -ENOENT);
                return -ENOENT;
            }

            if (toks->record) {
                // Check if record exists in database
                if (record_exists(toks) != STATUS_OK) {
                    LOG_WARN("Record not found: %s/%s", toks->table, toks->record);
                    LOG_FUSE_EXIT("getattr", -ENOENT);
                    return -ENOENT;
                }

                if (toks->attribute) {
                    // Check if attribute exists in schema
                    if (!find_attribute_by_name(table, toks->attribute) &&
                        !find_pk_by_name(table, toks->attribute) &&
                        !find_fk_by_name(table, toks->attribute)) {
                        LOG_WARN("Attribute not found in schema: %s/%s/%s", toks->table,
                                 toks->record, toks->attribute);
                        LOG_FUSE_EXIT("getattr", -ENOENT);
                        return -ENOENT;
                    }
                }
            }
        }

        st->st_mode  = S_IFDIR | 0755;
        st->st_nlink = 2;
        st->st_uid   = getuid();
        st->st_gid   = getgid();
        st->st_atime = st->st_mtime = time(NULL);

        LOG_TRACE("getattr: %s is a directory", path);

        LOG_FUSE_EXIT("getattr", 0);
    }
    // If path ends with .vfs2db, it's a file (either regular or symlink)
    else {
        // Remove extension and tokenize path to get table, record, and attribute for schema lookup
        char* noext_path = remove_extension(path);
        if (!noext_path) {
            LOG_FUSE_EXIT("getattr", -ENOMEM);
            return -ENOMEM;
        }

        // Tokenize path to get table, record, and attribute components for schema lookup
        struct tokens* toks = tokenize_path(noext_path);
        if (!toks) {
            LOG_FUSE_EXIT("getattr", -ENOMEM);
            return -ENOMEM;
        }

        if (toks->table) {
            // Check if table exists in schema
            Schema* table = find_schema_by_name(db_schema, toks->table);
            if (!table) {
                LOG_WARN("Table not found in schema: %s", toks->table);
                LOG_FUSE_EXIT("getattr", -ENOENT);
                return -ENOENT;
            }

            if (toks->record) {
                // Check if record exists in database
                if (record_exists(toks) != STATUS_OK) {
                    LOG_WARN("Record not found: %s/%s", toks->table, toks->record);
                    LOG_FUSE_EXIT("getattr", -ENOENT);
                    return -ENOENT;
                }

                if (toks->attribute) {
                    // Check if attribute exists in schema
                    if (!find_attribute_by_name(table, toks->attribute) &&
                        !find_pk_by_name(table, toks->attribute) &&
                        !find_fk_by_name(table, toks->attribute)) {
                        LOG_WARN("Attribute not found in schema: %s/%s/%s", toks->table,
                                 toks->record, toks->attribute);
                        LOG_FUSE_EXIT("getattr", -ENOENT);
                        return -ENOENT;
                    }
                }
            }
        }

        // We will treat foreign keys as symlinks and attributes as regular files.
        int is_symlink = check_symlink(toks);
        if (is_symlink) {
            st->st_mode  = S_IFLNK | 0644;
            st->st_nlink = 1;
            st->st_uid   = getuid();
            st->st_gid   = getgid();
            st->st_atime = st->st_mtime = time(NULL);
            LOG_TRACE("getattr: %s is a symlink (FK)", path);
        } else {
            st->st_mode  = S_IFREG | 0644;
            st->st_nlink = 1;
            st->st_uid   = getuid();
            st->st_gid   = getgid();
            st->st_atime = st->st_mtime = time(NULL);
            LOG_TRACE("getattr: %s is a regular file", path);
        }

        size_t attr_size;
        TRY(get_attribute_size(toks, &attr_size), cleanup, "Failed to get attribute size for %s",
            path);

        st->st_size = attr_size;
        LOG_TRACE("getattr: size=%zu", attr_size);
    }

cleanup:
    int code = status_to_errno(status);
    LOG_FUSE_EXIT("getattr", code);
    return code;
}

int vfs2db_getxattr(const char* path, const char* name, char* value, size_t size) {
    status_t status = STATUS_OK;

    LOG_FUSE_ENTER("getxattr", path);
    LOG_TRACE("getxattr: name=%s, bufsize=%zu", name, size);

    ensure_arena_init();

    // Validate attribute name
    if (strcmp(name, "user.type") != 0) {
        LOG_TRACE("getxattr: unsupported attribute '%s'", name);
        return -ENODATA;
    }

    // Remove extension and tokenize path to get table, record, and attribute for schema lookup
    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_ERROR("Failed to remove extension from path: %s", path);
        return -ENOMEM;
    }

    // Tokenize path to get table, record, and attribute components for schema lookup
    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_ERROR("Failed to tokenize path: %s", path);
        return -ENOMEM;
    }

    const char* t_str;
    int         attr_type;

    // Get attribute type from database schema to determine the value of user.type extended
    // attribute. This is used to provide type information about the attribute file in the VFS2DB
    // filesystem, which can be useful for clients to understand the type of data they are working
    // with when reading from or writing to the file.
    if (get_attribute_type(toks, &attr_type) != STATUS_OK) {
        LOG_ERROR("getxattr: failed to get attribute type for %s", path);
        return -EIO;
    }

    // Map SQLite type to string representation for user.type extended attribute.
    switch (attr_type) {
    case SQLITE_TEXT:
        t_str = "TEXT";
        break;
    case SQLITE_INTEGER:
        t_str = "INTEGER";
        break;
    case SQLITE_FLOAT:
        t_str = "FLOAT";
        break;
    case SQLITE_BLOB:
        t_str = "BLOB";
        break;
    case SQLITE_NULL:
        t_str = "NULL";
        break;
    default:
        t_str = "UNDEFINED";
        break;
    }

    LOG_DEBUG("getxattr: user.type=%s for %s", t_str, path);

    // Return size or copy data
    if (size == 0)
        return strlen(t_str);
    if (size < strlen(t_str))
        return -ERANGE;

    strncpy(value, t_str, size);

    LOG_FUSE_EXIT("getxattr", strlen(t_str));
    return strlen(t_str);
}

int vfs2db_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset,
                   struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    status_t status = STATUS_OK;

    LOG_FUSE_ENTER("readdir", path);

    ensure_arena_init();

    // Add current and parent directory entries. This is required for proper directory navigation.
    filler(buffer, ".", NULL, 0, FUSE_FILL_DIR_DEFAULTS);
    filler(buffer, "..", NULL, 0, FUSE_FILL_DIR_DEFAULTS);

    // Make a mutable copy of the path in the arena for tokenization and manipulation.
    char* path_copy = arena_strdup(arena, path);
    if (!path_copy) {
        LOG_FUSE_EXIT("readdir", -ENOMEM);
        return -ENOMEM;
    }

    // Remove trailing slash if present to ensure consistent tokenization and schema lookup.
    if (path_copy[strlen(path) - 1] == '/') {
        path_copy[strlen(path) - 1] = 0;
    }

    struct tokens* toks = tokenize_path(path_copy);
    if (!toks) {
        LOG_FUSE_EXIT("readdir", -ENOMEM);
        return -ENOMEM;
    }

    // Count the number of slashes in the path to determine the directory level.
    int slash_count = COUNT_CHAR(path_copy, '/');
    LOG_TRACE("readdir: slash_count=%d, table=%s, record=%s", slash_count,
              toks->table ? toks->table : "(null)", toks->record ? toks->record : "(null)");

    // Based on the number of slashes, we determine what to list in the directory:
    // - 0 slashes: list tables in the database
    // - 1 slash:   list records in the specified table
    // - 2 slashes: list attributes of the specified record
    switch (slash_count) {
    case 0: {
        LOG_DEBUG("readdir: listing tables");

        int count = 0;

        // List all tables in the database schema as directory entries.
        HASH_FOREACH(current_schema, db_schema->tables_head) {
            filler(buffer, current_schema->name, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            count++;
        }

        LOG_TRACE("readdir: found %d tables", count);
        break;
    }

    case 1: {
        LOG_DEBUG("readdir: listing records for table '%s'", toks->table);

        char* record_list[MAX_SIZE];
        int   n_records;

        // Get all record rowids for the specified table from the database.
        TRY(get_table_rowids(toks->table, record_list, &n_records), cleanup,
            "Failed to get rowids for table '%s'", toks->table);

        // Add each record as a directory entry. The record names are derived from their rowids,
        // which are unique identifiers for the records in the table.
        for (int i = 0; i < n_records; i++) {
            filler(buffer, record_list[i], NULL, 0, FUSE_FILL_DIR_DEFAULTS);
        }

        LOG_TRACE("readdir: listed %d records", n_records);
        break;
    }

    case 2: {
        LOG_DEBUG("readdir: listing attributes for %s/%s", toks->table, toks->record);

        // Check if /table/record/ exists
        TRY(record_exists(toks), cleanup, "Record not found: %s/%s", toks->table, toks->record);

        Schema* table;

        // Look up the table schema for the specified table to get information about its attributes,
        // primary keys, and foreign keys, which will be listed as files in the directory.
        TRY_NOT_NULL(table = find_schema_by_name(db_schema, toks->table), cleanup, STATUS_DB_ERROR,
                     "Table not found in schema: %s", toks->table);

        char file[MAX_SIZE];
        int  count = 0;

        // Primary Keys (PKs)
        HASH_FOREACH(current_pk, table->pk_head) {
            snprintf(file, sizeof(file), "%s.vfs2db", current_pk->name);
            filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            count++;
        }

        // Attributes
        HASH_FOREACH(current_attr, table->attr_head) {
            snprintf(file, sizeof(file), "%s.vfs2db", current_attr->name);
            filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            count++;
        }

        // Foreign Keys (FKs) - treated as symlinks to other tables, so we list them as files with
        // .vfs2db extension. The actual symlink target will be determined in the getattr handler
        // based on the FK definition in the schema.
        HASH_FOREACH(current_fk, table->fks_head) {
            snprintf(file, sizeof(file), "%s.vfs2db", current_fk->from);
            filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            count++;
        }

        LOG_TRACE("readdir: listed %d attributes", count);
        break;
    }

    default:
        LOG_ERROR("readdir: unexpected path depth %d for '%s'", slash_count, path);
        break;
    }

cleanup:
    int code = status_to_errno(status);
    LOG_FUSE_EXIT("readdir", code);
    return code;
}

int vfs2db_mkdir(const char* path, mode_t mode) {
    LOG_FUSE_ENTER("mkdir", path);

    ensure_arena_init();

    status_t status = STATUS_OK;

cleanup:
    int code = status_to_errno(status);
    LOG_FUSE_EXIT("mkdir", code);
    return code;
}

int vfs2db_open(const char* path, struct fuse_file_info* fi) {
    status_t status = STATUS_OK;

    LOG_FUSE_ENTER("open", path);

    ensure_arena_init();

    // Remove extension and tokenize path to get table, record, and attribute for schema lookup.
    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("open", -ENOMEM);
        return -ENOMEM;
    }

    // Tokenize path to get table, record, and attribute components for schema lookup.
    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("open", -ENOMEM);
        return -ENOMEM;
    }

    // If any component is missing, we will treat it as a non-existent file.
    if (!toks->table || !toks->attribute || !toks->record) {
        LOG_ERROR("open: invalid path '%s', missing table, record, or attribute", path);
        LOG_FUSE_EXIT("open", -ENOENT);
        return -ENOENT;
    }

    int flags = fi->flags;

    // If file doesn't exist and O_CREAT isn't specified, error
    // /metrics/123/attribute.vfs2db

    // If O_CREAT
    if (flags & O_CREAT) {
        LOG_WARN("open: O_CREAT flag is not supported yet, file creation is not implemented");
        return 0;
    }

    // If O_TRUNC
    if (flags & O_TRUNC) {
        vfs2db_truncate(path, 0, NULL);
    }

    return 0;
}

int vfs2db_read(const char* path, char* buffer, size_t size, off_t offset,
                struct fuse_file_info* fi) {
    (void)fi;

    LOG_FUSE_ENTER("read", path);
    LOG_TRACE("read: size=%zu, offset=%ld", size, offset);

    ensure_arena_init();

    // Remove extension and tokenize path to get table, record, and attribute for schema lookup.
    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("read", -ENOMEM);
        return -ENOMEM;
    }

    // Tokenize path to get table, record, and attribute components for schema lookup.
    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("read", -ENOMEM);
        return -ENOMEM;
    }

    size_t total_size;
    char*  bytes = NULL;

    // Get the total size of the attribute value from the database to determine how many bytes we
    // can read from the specified offset. This is necessary to handle cases where the read request
    // goes beyond the end of the attribute value.
    if (get_attribute_size(toks, &total_size) == STATUS_DB_ERROR) {
        LOG_ERROR("read: failed to get attribute size");
        return -1;
    }

    // If the offset is beyond the end of the attribute value, we return 0 to indicate EOF.
    if (offset >= total_size) {
        LOG_TRACE("read: offset %ld >= size %zu, returning EOF", offset, total_size);
        LOG_FUSE_EXIT("read", 0);
        return 0;
    }

    // Get the attribute value bytes from the database for the specified offset.
    if (get_attribute_chunk_bytes(toks, offset, &bytes) == STATUS_DB_ERROR) {
        LOG_ERROR("read: failed to get attribute bytes");
        LOG_FUSE_EXIT("read", -EIO);
        return -1;
    }

    // Calculate how many bytes we can copy to the buffer based on the requested size and the total
    // size of the attribute value. We need to ensure that we do not read beyond the end of the
    // attribute value, so we take the minimum of the requested size and the available bytes.
    size_t bytes_to_copy = MIN(size, total_size - offset);
    memcpy(buffer, bytes, bytes_to_copy);

    LOG_DEBUG("read: returned %zu bytes from offset %ld", bytes_to_copy, offset);
    LOG_FUSE_EXIT("read", bytes_to_copy);

    return bytes_to_copy;
}

int vfs2db_write(const char* path, const char* buffer, size_t size, off_t offset,
                 struct fuse_file_info* fi) {
    (void)fi;

    LOG_FUSE_ENTER("write", path);
    LOG_TRACE("write: size=%zu, offset=%ld", size, offset);

    ensure_arena_init();

    // Remove extension and tokenize path to get table, record, and attribute for schema lookup.
    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("write", -ENOMEM);
        return -ENOMEM;
    }

    // Tokenize path to get table, record, and attribute components for schema lookup.
    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("write", -ENOMEM);
        return -ENOMEM;
    }

    // Update the attribute value in the database with the new data from the buffer at the specified
    // offset.
    if (update_attribute_value(toks, buffer, size, offset) == STATUS_DB_ERROR) {
        LOG_ERROR("write: failed to update attribute");
        LOG_FUSE_EXIT("write", -EIO);
        return -EIO;
    }

    LOG_DEBUG("write: wrote %zu bytes at offset %ld)", size, offset);
    LOG_FUSE_EXIT("write", size);

    return (int)size;
}

int vfs2db_truncate(const char* path, off_t size, struct fuse_file_info* fi) {
    status_t status = STATUS_OK;

    LOG_FUSE_ENTER("truncate", path);

    ensure_arena_init();

    // Remove extension and tokenize path to get table, record, and attribute for schema lookup.
    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("truncate", -ENOMEM);
        return -ENOMEM;
    }

    // Tokenize path to get table, record, and attribute components for schema lookup.
    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("truncate", -ENOMEM);
        return -ENOMEM;
    }

    // Truncation to zero size is supported by setting the attribute value to NULL in the database.
    if (size == 0) {
        TRY(set_attribute_empty(toks), cleanup, "Failed to set attribute to NULL for %s", path);
    } else {
        LOG_WARN("truncate: truncation to non-zero size is not supported, setting attribute to "
                 "NULL instead");
    }

cleanup:
    int code = status_to_errno(status);
    LOG_FUSE_EXIT("truncate", code);
    return code;
}

int vfs2db_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void)mode;
    (void)fi;

    LOG_FUSE_ENTER("create", path);

    // TODO: Implement record creation
    LOG_WARN("create: not implemented yet");

    LOG_FUSE_EXIT("create", 0);
    return 0;
}

int vfs2db_readlink(const char* path, char* buffer, size_t size) {
    LOG_FUSE_ENTER("readlink", path);

    ensure_arena_init();

    status_t status = STATUS_OK;

    // Remove extension and tokenize path to get table, record, and attribute for schema lookup.
    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("readlink", -ENOMEM);
        return -ENOMEM;
    }

    // Tokenize path to get table, record, and attribute components for schema lookup.
    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("readlink", -ENOMEM);
        return -ENOMEM;
    }

    // Look up the table schema for the specified table to get information about its foreign keys,
    // which will be used to determine the target of the symbolic link.
    Schema* table;
    TRY_NOT_NULL(table = find_schema_by_name(db_schema, toks->table), cleanup, STATUS_DB_ERROR,
                 "Table not found in schema: %s", toks->table);

    // Look up the foreign key definition for the specified attribute to get the target table and
    // attribute that this symbolic link points to. The foreign key definition will provide the
    // necessary information to resolve the link target based on the database schema.
    Fk* fk;
    TRY_NOT_NULL(fk = find_fk_by_name(table, toks->attribute), cleanup, STATUS_DB_ERROR,
                 "FK not found for attribute '%s' in table '%s'", toks->attribute, toks->table);

    LOG_DEBUG("readlink: FK %s -> %s(%s)", fk->from, fk->table, fk->to);

    int  n_same_fks = 0;
    Fk** fks        = NULL;

    // We need to find all foreign keys in the same table that point to the same target table as the
    // current FK, because we need to get the values of all those FKs to resolve the link target
    // based on the combination of their values (in case of composite foreign keys).
    TRY_NOT_NULL(fks = arena_calloc(arena, count_fks(table), sizeof(Fk*)), cleanup,
                 STATUS_ALLOC_ERROR, "Failed to allocate fks array for table '%s'", table->name);

    // Find all FKs in the same table that point to the same target table.
    // This is necessary to handle cases where there are multiple foreign keys in the same table
    // that point to the same target table, which can be used to create composite foreign keys.
    HASH_FOREACH(current_fk, table->fks_head) {
        if (strncmp(fk->table, current_fk->table, strlen(fk->table)) == 0) {
            fks[n_same_fks++] = current_fk;
        }
    }

    char** fk_values = arena_calloc(arena, n_same_fks, sizeof(char*));

    LOG_TRACE("readlink: found %d FKs to table '%s'", n_same_fks, fk->table);

    // Get the values of all the FKs that point to the same target table to resolve the link target
    // based on the combination of their values.
    for (int i = 0; i < n_same_fks; i++) {
        char* value = NULL;

        struct tokens fk_toks = {
            .table = toks->table, .record = toks->record, .attribute = fks[i]->from};

        // Get the value of the FK attribute from the database for the specified record to use in
        // resolving the link target.
        TRY(get_attribute_chunk_bytes(&fk_toks, 0, &value), cleanup,
            "Failed to get FK value for %s.%s", fk_toks.table, fk_toks.attribute);

        fk_values[i] = value;
        LOG_TRACE("readlink: FK value %s=%s", fks[i]->from, value);
    }

    int row_id;

    // Resolve the rowid of the target record in the target table based on the values of the FKs
    // that point to the same target table.
    TRY(get_rowid_from_pks(fk->table, fks, fk_values, n_same_fks, &row_id), cleanup,
        "Failed to resolve FK target for %s.%s", toks->table, toks->attribute);

    LOG_DEBUG("readlink: resolved to %s/%d/%s", fk->table, row_id, fk->to);

    // Construct the link target path based on the resolved rowid and the target attribute specified
    // in the FK definition.
    snprintf(buffer, size, "../../%s/%d/%s.vfs2db", fk->table, row_id, fk->to);

    LOG_DEBUG("readlink: target=%s", buffer);

cleanup:
    int code = status_to_errno(status);
    LOG_FUSE_EXIT("readlink", code);
    return code;
}
