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

/**
 * Ensure Arena Init
 *
 * @brief Retrieves the thread-local memory arena, creating it if it doesn't exist.
 *
 * @return Pointer to the thread-local Arena
 */
void ensure_arena_init() {
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

/**
 * Tokenize Path
 *
 * @brief Splits the given path into its components: table, record, and attribute.
 *
 * @param[in] path The file path to tokenize
 *
 * @return Pointer to a tokens structure containing the split components
 */
static inline struct tokens* tokenize_path(const char* path) {
    // path := /table/record/attribute
    // path := table/record/attribute
    if (!path) {
        LOG_WARN("tokenize_path called with NULL path");
        return NULL;
    }

    struct tokens* toks = arena_alloc(arena, sizeof(struct tokens));
    if (!toks) {
        LOG_ERROR("Failed to allocate tokens struct");
        return NULL;
    }

    char* path_copy = arena_strdup(arena, path);
    if (!path_copy) {
        LOG_ERROR("Failed to duplicate path string");
        return NULL;
    }

    char* cursor = path_copy;
    if (cursor[0] == '/')
        cursor++;

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

/**
 * Remove Extension
 *
 * @brief Removes the ".vfs2db" extension from the given path.
 *
 * @param[in] path The file path from which to remove the extension
 *
 * @return Pointer to a new string without the extension
 */
static inline char* remove_extension(const char* path) {
    int noext_path_length = strlen(path) - 7;
    if (noext_path_length <= 0) {
        LOG_WARN("Path too short to have extension: %s", path);
        return NULL;
    }

    char* noext_path = arena_alloc(arena, noext_path_length + 1);
    if (!noext_path) {
        LOG_ERROR("Failed to allocate path without extension");
        return NULL;
    }

    strncpy(noext_path, path, noext_path_length);
    noext_path[noext_path_length] = 0;

    return noext_path;
}

/**
 * Check Symlink
 *
 * @brief Checks if the given attribute in the tokens structure is a foreign key.
 *
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute information
 *
 * @return 1 if the attribute is a foreign key, 0 otherwise
 */
static inline int check_symlink(struct tokens* toks) {
    Schema* table = find_schema_by_name(db_schema, toks->table);
    if (!table) {
        LOG_WARN("Table not found in schema: %s", toks->table);
        return 0;
    }

    // Check if attribute is fk
    if (find_fk_by_name(table, toks->attribute)) {
        LOG_TRACE("Attribute '%s' is a foreign key (symlink)", toks->attribute);
        return 1;
    }

    return 0;
}

/**
 * VFS2DB Init
 *
 * @brief Initializes the VFS2DB filesystem by loading the database schema.
 *
 * @return Pointer to private data (not used in this case)
 */
void* vfs2db_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    LOG_INFO("Initializing VFS2DB filesystem...");

    const char* db_path = (const char*)fuse_get_context()->private_data;
    if (!db_path) {
        LOG_FATAL("Database path not provided in FUSE private data");
        return NULL;
    }

    // Initialize Query Manager
    if (qm_init(db) != STATUS_OK) {
        LOG_FATAL("Failed to initialize Query Manager");
        return NULL;
    }

    // Get all the tables
    db_schema = malloc(sizeof(DbSchema));
    if (!db_schema) {
        LOG_FATAL("Failed to allocate DbSchema");
        return NULL;
    }

    char* db_name                = strdup(db_path);
    db_name[strlen(db_path) - 3] = 0;
    db_name += 3;
    db_schema->db_name = db_name;
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

/**
 * VFS2DB Destroy
 *
 * @brief Cleans up resources when the VFS2DB filesystem is unmounted.
 *
 * @param[in] private_data Pointer to private data (not used in this case)
 */
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

/**
 * VFS2DB Getattr
 *
 * @brief Retrieves the attributes of a file or directory in the VFS2DB filesystem.
 *
 * @param[in]  path  The file or directory path
 * @param[out] st    Pointer to a stat structure to be filled with attributes
 * @param[in]  fi    Pointer to fuse_file_info structure (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_getattr(const char* path, struct stat* st, struct fuse_file_info* fi) {
    (void)fi;

    LOG_FUSE_ENTER("getattr", path);

    ensure_arena_init();

    memset(st, 0, sizeof(*st));

    // /heavy_files/content.vfs2db

    // Check if directory --> doesn't finish with .vfs2db
    // path: test/ciao/1.vfs2db
    if (strncmp(&path[strlen(path) - 7], ".vfs2db", 7)) {
        if (strstr(path, ".Trash") != NULL) {
            LOG_TRACE("The system asked for .Trash-xxxx directory");
            return -ENOENT;
        }

        st->st_mode  = S_IFDIR | 0755;
        st->st_nlink = 2;
        st->st_uid   = getuid();
        st->st_gid   = getgid();
        st->st_atime = st->st_mtime = time(NULL);

        LOG_TRACE("getattr: %s is a directory", path);
        LOG_FUSE_EXIT("getattr", 0);
    } else {
        char* noext_path = remove_extension(path);
        if (!noext_path) {
            LOG_FUSE_EXIT("getattr", -ENOMEM);
            return -ENOMEM;
        }

        struct tokens* toks = tokenize_path(noext_path);
        if (!toks) {
            LOG_FUSE_EXIT("getattr", -ENOMEM);
            return -ENOMEM;
        }

        if (!toks->table || !toks->record || !toks->attribute) {
            LOG_ERROR("getattr: incomplete path tokens for %s", path);
            LOG_FUSE_EXIT("getattr", -ENOENT);
            return -ENOENT;
        }

        // We need to check if it is a symlink
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
        if (get_attribute_size(toks, &attr_size) == STATUS_DB_ERROR) {
            LOG_ERROR("Failed to get attribute size for %s", path);
            LOG_FUSE_EXIT("getattr", -EIO);
            return -EIO;
        }

        st->st_size = attr_size;
        LOG_TRACE("getattr: size=%zu", attr_size);
    }

    LOG_FUSE_EXIT("getattr", 0);
    return 0;
}

/**
 * VFS2DB Getxattr
 *
 * @brief Retrieves the extended attribute of a file in the VFS2DB filesystem.
 *
 * @param[in]  path  The file path
 * @param[in]  name  The name of the extended attribute to retrieve
 * @param[out] value Pointer to a buffer where the attribute value will be stored
 * @param[in]  size  Size of the buffer
 *
 * @return Size of the attribute value on success, negative error code on failure
 */
int vfs2db_getxattr(const char* path, const char* name, char* value, size_t size) {
    LOG_FUSE_ENTER("getxattr", path);
    LOG_TRACE("getxattr: name=%s, bufsize=%zu", name, size);

    ensure_arena_init();

    if (strcmp(name, "user.type") != 0) {
        LOG_TRACE("getxattr: unsupported attribute '%s'", name);
        return -ENODATA;
    }

    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_ERROR("Failed to remove extension from path: %s", path);
        return -ENOMEM;
    }

    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_ERROR("Failed to tokenize path: %s", path);
        return -ENOMEM;
    }

    const char* t_str;
    int         attr_type;
    if (get_attribute_type(toks, &attr_type) != STATUS_OK) {
        LOG_ERROR("getxattr: failed to get attribute type for %s", path);
        return -EIO;
    }

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

    // 3. Return size or copy data
    if (size == 0)
        return strlen(t_str);
    if (size < strlen(t_str))
        return -ERANGE;

    strncpy(value, t_str, size);

    LOG_FUSE_EXIT("getxattr", strlen(t_str));
    return strlen(t_str);
}

/**
 * VFS2DB Readdir
 *
 * @brief Reads the contents of a directory in the VFS2DB filesystem.
 *
 * @param[in]  path    The directory path
 * @param[out] buffer  Pointer to a buffer where directory entries will be stored
 * @param[in]  filler  Function pointer to add entries to the buffer
 * @param[in]  offset  Offset within the directory (not used in this case)
 * @param[in]  fi      Pointer to fuse_file_info structure (not used in this case)
 * @param[in]  flags   Flags for reading the directory (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset,
                   struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    LOG_FUSE_ENTER("readdir", path);

    ensure_arena_init();

    filler(buffer, ".", NULL, 0, FUSE_FILL_DIR_DEFAULTS);
    filler(buffer, "..", NULL, 0, FUSE_FILL_DIR_DEFAULTS);

    // path: /
    // path: /orders    |   /orders/
    // path: /orders/2  |   /orders/2/

    // Togliamo slash alla fine, se c'e'
    char* path_copy = arena_strdup(arena, path);
    if (!path_copy) {
        LOG_FUSE_EXIT("readdir", -ENOMEM);
        return -ENOMEM;
    }

    if (path_copy[strlen(path) - 1] == '/') {
        path_copy[strlen(path) - 1] = 0;
    }

    struct tokens* toks = tokenize_path(path_copy);
    if (!toks) {
        LOG_FUSE_EXIT("readdir", -ENOMEM);
        return -ENOMEM;
    }

    int slash_count = COUNT_CHAR(path_copy, '/');
    LOG_TRACE("readdir: slash_count=%d, table=%s, record=%s", slash_count,
              toks->table ? toks->table : "(null)", toks->record ? toks->record : "(null)");

    switch (slash_count) {
    case 0: {
        LOG_DEBUG("readdir: listing tables");

        int count = 0;
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

        if (get_table_rowids(toks->table, record_list, &n_records) == STATUS_DB_ERROR) {
            LOG_ERROR("Failed to get rowids for table '%s'", toks->table);
            break;
        }

        for (int i = 0; i < n_records; i++) {
            filler(buffer, record_list[i], NULL, 0, FUSE_FILL_DIR_DEFAULTS);
        }

        LOG_TRACE("readdir: listed %d records", n_records);
        break;
    }

    case 2: {
        LOG_DEBUG("readdir: listing attributes for %s/%s", toks->table, toks->record);

        Schema* table = find_schema_by_name(db_schema, toks->table);
        if (!table) {
            LOG_ERROR("Table not found: %s", toks->table);
            break;
        }

        char file[MAX_SIZE];
        int  count = 0;

        // Pk
        HASH_FOREACH(current_pk, table->pk_head) {
            snprintf(file, sizeof(file), "%s.vfs2db", current_pk->name);
            filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            count++;
        }

        // Attr
        HASH_FOREACH(current_attr, table->attr_head) {
            snprintf(file, sizeof(file), "%s.vfs2db", current_attr->name);
            filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            count++;
        }

        // Fk
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

    LOG_FUSE_EXIT("readdir", 0);
    return 0;
}

/**
 * VFS2DB Read
 *
 * @brief Reads data from a file in the VFS2DB filesystem.
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
                struct fuse_file_info* fi) {
    (void)fi;

    LOG_FUSE_ENTER("read", path);
    LOG_TRACE("read: size=%zu, offset=%ld", size, offset);

    ensure_arena_init();

    size_t path_len = strlen(path);
    if (path_len < 7)
        return -1; // Safety check

    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("read", -ENOMEM);
        return -ENOMEM;
    }

    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("read", -ENOMEM);
        return -ENOMEM;
    }

    size_t total_size;
    char*  bytes = NULL;

    if (get_attribute_size(toks, &total_size) == STATUS_DB_ERROR) {
        LOG_ERROR("read: failed to get attribute size");
        return -1;
    }

    // assert (offset <= total_size);
    if (offset >= total_size) {
        LOG_TRACE("read: offset %ld >= size %zu, returning EOF", offset, total_size);
        LOG_FUSE_EXIT("read", 0);
        return 0;
    }

    // Let's use the cache
    if (get_attribute_chunk_bytes(toks, offset, &bytes) == STATUS_DB_ERROR) {
        LOG_ERROR("read: failed to get attribute bytes");
        LOG_FUSE_EXIT("read", -EIO);
        return -1;
    }

    size_t bytes_to_copy = MIN(size, total_size - offset);
    memcpy(buffer, bytes, bytes_to_copy);

    LOG_DEBUG("read: returned %zu bytes from offset %ld", bytes_to_copy, offset);
    LOG_FUSE_EXIT("read", bytes_to_copy);

    return bytes_to_copy;
}

/**
 * VFS2DB Write
 *
 * @brief Writes data to a file in the VFS2DB filesystem.
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
                 struct fuse_file_info* fi) {
    (void)fi;

    LOG_FUSE_ENTER("write", path);
    LOG_TRACE("write: size=%zu, offset=%ld", size, offset);

    ensure_arena_init();

    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("write", -ENOMEM);
        return -ENOMEM;
    }

    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("write", -ENOMEM);
        return -ENOMEM;
    }

    if (update_attribute_value(toks, buffer, size, offset) == STATUS_DB_ERROR) {
        LOG_ERROR("write: failed to update attribute");
        LOG_FUSE_EXIT("write", -EIO);
        return -EIO;
    }

    LOG_DEBUG("write: wrote %zu bytes at offset %ld)", size, offset);
    LOG_FUSE_EXIT("write", size);

    return (int)size;
}

/**
 * VFS2DB Truncate
 *
 * @brief Truncates a file in the VFS2DB filesystem to a specified size.
 *
 * @param[in] path    The file path
 * @param[in] size    The size to truncate to
 * @param[in] fi      Pointer to fuse_file_info structure (not used in this case)
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_truncate(const char* path, off_t size, struct fuse_file_info* fi) {
    LOG_FUSE_ENTER("truncate", path);

    ensure_arena_init();

    struct tokens* toks = tokenize_path(path);
    if (!toks) {
        LOG_FUSE_EXIT("truncate", -ENOMEM);
        return -ENOMEM;
    }

    if (size == 0) {
        if (set_attribute_null(toks) == STATUS_DB_ERROR) {
            LOG_ERROR("truncate: failed to set attribute to NULL");
            LOG_FUSE_EXIT("truncate", -EIO);
            return -EIO;
        }
    } else {        
        LOG_WARN("truncate: truncation to non-zero size is not supported, setting attribute to NULL instead");
    }

    LOG_FUSE_EXIT("truncate", 0);
    return 0;
}

/**
 * VFS2DB Create
 * @todo Implement the function to insert a new record in the database
 *
 * @brief Creates a new file in the VFS2DB filesystem, which corresponds to inserting a new
 * record in the database.
 *
 * @param[in] path The file path to create
 * @param[in] mode The file mode (permissions)
 * @param[in] fi   Pointer to fuse_file_info structure (not used in this case
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void)mode;
    (void)fi;

    LOG_FUSE_ENTER("create", path);

    // TODO: Implement record creation
    LOG_WARN("create: not implemented yet");

    LOG_FUSE_EXIT("create", 0);
    return 0;
}

/**
 * VFS2DB Readlink
 *
 * @brief Reads the target of a symbolic link in the VFS2DB filesystem.
 *
 * @param[in] path   The symbolic link path
 * @param[out] buffer Buffer to store the link target
 * @param[in] size   Size of the buffer
 *
 * @return 0 on success, negative error code on failure
 */
int vfs2db_readlink(const char* path, char* buffer, size_t size) {
    LOG_FUSE_ENTER("readlink", path);

    ensure_arena_init();

    char* noext_path = remove_extension(path);
    if (!noext_path) {
        LOG_FUSE_EXIT("readlink", -ENOMEM);
        return -ENOMEM;
    }

    struct tokens* toks = tokenize_path(noext_path);
    if (!toks) {
        LOG_FUSE_EXIT("readlink", -ENOMEM);
        return -ENOMEM;
    }

    // Get schema
    Schema* table = find_schema_by_name(db_schema, toks->table);
    if (!table) {
        LOG_ERROR("readlink: table not found: %s", toks->table);
        LOG_FUSE_EXIT("readlink", -ENOENT);
        return -ENOENT;
    }

    // Get fk
    Fk* fk = find_fk_by_name(table, toks->attribute);
    if (!fk) {
        LOG_ERROR("readlink: FK not found: %s.%s", toks->table, toks->attribute);
        LOG_FUSE_EXIT("readlink", -ENOENT);
        return -ENOENT;
    }

    LOG_DEBUG("readlink: FK %s -> %s(%s)", fk->from, fk->table, fk->to);

    // Get all fks with the same 'table' value
    Fk* fks[count_fks(table)];
    int n_same_fks = 0;

    HASH_FOREACH(current_fk, table->fks_head) {
        if (strncmp(fk->table, current_fk->table, strlen(fk->table)) == 0) {
            fks[n_same_fks++] = current_fk;
        }
    }

    LOG_TRACE("readlink: found %d FKs to table '%s'", n_same_fks, fk->table);

    // Get values of the fks
    char* fk_values[n_same_fks];
    for (int i = 0; i < n_same_fks; i++) {
        char*         value   = NULL;
        struct tokens fk_toks = {
            .table = toks->table, .record = toks->record, .attribute = fks[i]->from};

        if (get_attribute_chunk_bytes(&fk_toks, 0, &value) != STATUS_OK) {
            LOG_ERROR("readlink: failed to get FK value for %s", fks[i]->from);
            LOG_FUSE_EXIT("readlink", -EIO);
            return -EIO;
        }

        fk_values[i] = value;
        LOG_TRACE("readlink: FK value %s=%s", fks[i]->from, value);
    }

    int row_id;
    if (get_rowid_from_pks(fk->table, fks, fk_values, n_same_fks, &row_id) != STATUS_OK) {
        LOG_ERROR("readlink: failed to resolve FK target");
        LOG_FUSE_EXIT("readlink", -EIO);
        return -EIO;
    }

    LOG_DEBUG("readlink: resolved to %s/%d/%s", fk->table, row_id, fk->to);

    // 6. creare il path del record -> ../../ftable/row_id/fattribute.vfs2db
    snprintf(buffer, size, "../../%s/%d/%s.vfs2db", fk->table, row_id, fk->to);

    LOG_DEBUG("readlink: target=%s", buffer);
    LOG_FUSE_EXIT("readlink", 0);

    return 0;
}
