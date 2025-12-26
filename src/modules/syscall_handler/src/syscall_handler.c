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

extern sqlite3*  db; /**< Database connection handle */
extern DbSchema* db_schema;

/**
 * Tokenize Path
 * 
 * @brief Splits the given path into its components: table, record, and attribute.
 * 
 * @param[in] path The file path to tokenize
 * 
 * @return Pointer to a tokens structure containing the split components
 * 
 */
static inline struct tokens* tokenize_path(const char* path) {
    // path := /table/record/attribute
    // path := table/record/attribute
    if (!path) return NULL;

    struct tokens* toks = malloc(sizeof(struct tokens));

    char* path_copy = strdup(path); 
    if (!path_copy) return NULL;

    char *cursor = path_copy;
    if (cursor[0] == '/') cursor++;

    char* t = strtok(cursor, "/");
    toks->table = t ? strdup(t) : NULL;
    t = strtok(NULL, "/");
    toks->record = t ? strdup(t) : NULL;
    t = strtok(NULL, "/");
    toks->attribute = t ? strdup(t) : NULL;

    free(path_copy);

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
 * 
 */
static inline char *remove_extension(const char *path) {
    int noext_path_length = strlen(path) - 7;
    if (noext_path_length <= 0) return NULL;

    char *noext_path = malloc(noext_path_length + 1);
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
 * 
 */
static inline int check_symlink(struct tokens* toks) {
    printf("check_symlink\n");
    // Get schema
    Schema* table;
    for (int i=0; i<db_schema->n_tables; i++) {
        if (strncmp(toks->table, db_schema->tables[i]->name, strlen(toks->table)) == 0) {
            table = db_schema->tables[i];
            break;
        }
    }
    // Check if attribute is fk
    for (int i=0; i<table->n_fks; i++) {
        if (strncmp(toks->attribute, table->fks[i]->from, strlen(toks->attribute)) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * VFS2DB Init
 * 
 * @brief Initializes the VFS2DB filesystem by loading the database schema.
 * 
 * @return Pointer to private data (NULL in this case)
 * 
 */
void* vfs2db_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    printf("init\n");

    // Initialize Query Manager
    qm_init(db);

    // Get all the tables
    db_schema = malloc(sizeof(DbSchema));
    if (!db_schema) return NULL;
    
    init_db_schema(db_schema);
    printf("\tNumber of tables: %d\n", db_schema->n_tables);

    // For each table, get all the info
    for (int i=0; i<db_schema->n_tables; i++) {
        init_schema(db_schema->tables[i]);
    }

    // Testing
    for (int i=0; i<db_schema->n_tables; i++) {
        // Print table name
        printf("Table: %s\n", db_schema->tables[i]->name);
        // Print pks
        for (int j=0; j<db_schema->tables[i]->n_pk; j++) {
            printf("\tPK: %s\n", db_schema->tables[i]->pk[j]);
        }
        // Print fks
        for (int j=0; j<db_schema->tables[i]->n_fks; j++) {
            printf("\tFK: %s -> %s(%s)\n", db_schema->tables[i]->fks[j]->from,
                                           db_schema->tables[i]->fks[j]->table,
                                           db_schema->tables[i]->fks[j]->to);
        }
        // Print attributes
        for (int j=0; j<db_schema->tables[i]->n_attr; j++) {
            printf("\tATT: %s\n", db_schema->tables[i]->attr[j]);
        }
    }

    return NULL;
}

/**
 * VFS2DB Destroy
 * 
 * @brief Cleans up resources when the VFS2DB filesystem is unmounted.
 * 
 * @param[in] private_data Pointer to private data (not used in this case)
 * 
 */
void vfs2db_destroy(void *private_data) {
    struct fuse_args *args = (struct fuse_args*) private_data;
    if (db) {
        qm_cleanup();
        sqlite3_close(db);
        printf("sqlite3_close executed correctly.\n");
    }

    fuse_opt_free_args(args);
    printf("fuse_opt_free_args executed correctly.\n");

    free(db_schema);
    printf("db schema deallocated.\n");
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
 * 
 */
int vfs2db_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    printf("getattr: %s\n", path);

    memset(st, 0, sizeof(*st));

    // Check if directory --> doesn't finish with .vfs2db
    // path: test/ciao/1.vfs2db
    if (strncmp(&path[strlen(path) - 7], ".vfs2db", 7)) {
        printf("\tDirectory\n");
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        st->st_uid = getuid();
        st->st_gid = getgid();
        st->st_atime = st->st_mtime = time(NULL);
    } else {
        printf("\tFile\n");

        char *noext_path = remove_extension(path);
        if (!noext_path) return -ENOMEM;
        struct tokens *toks = tokenize_path(noext_path);

        // We need to check if it is a symlink
        int is_symlink = check_symlink(toks);
        if (is_symlink) {
            st->st_mode = S_IFLNK | 0644;
            st->st_nlink = 1;
            st->st_uid = getuid();
            st->st_gid = getgid();
            st->st_atime = st->st_mtime = time(NULL);
        } else {
            st->st_mode = S_IFREG | 0644;
            st->st_nlink = 1;
            st->st_uid = getuid();
            st->st_gid = getgid();
            st->st_atime = st->st_mtime = time(NULL);
        }

        size_t attr_size;
        if (get_attribute_size(toks, &attr_size) == -1) {
            return -1;
        }

        st->st_size = attr_size;

        free(toks->table);
        free(toks->record);
        free(toks->attribute);
        free(toks);
        free(noext_path);

        printf("\tcontent size: %d\n", attr_size);
    }

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
 * 
 */
int vfs2db_getxattr(const char *path, const char *name, char *value, size_t size) {
    if (strcmp(name, "user.type") != 0) return -ENODATA;

    char *noext_path = remove_extension(path);
    if (!noext_path) return -ENOMEM;
    
    struct tokens *toks = tokenize_path(noext_path);

    const char* t_str;
    int attr_type;
    if (get_attribute_type(toks, &attr_type) == -1) {
        return -1;
    }
    switch (attr_type) {
        case SQLITE_TEXT:    t_str = "TEXT"; break;
        case SQLITE_INTEGER: t_str = "INTEGER"; break;
        case SQLITE_FLOAT:   t_str = "FLOAT"; break;
        case SQLITE_BLOB:    t_str = "BLOB"; break;
        case SQLITE_NULL:    t_str = "NULL"; break;
        default:             t_str = "UNDEFINED"; break;
    }

    printf("type: %s\n", t_str);

    // 3. Return size or copy data
    if (size == 0) return strlen(t_str);
    if (size < strlen(t_str)) return -ERANGE;
    
    strcpy(value, t_str);

    free(toks->table);
    free(toks->record);
    free(toks->attribute);
    free(toks);

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
 * 
 */
int vfs2db_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    printf("readdir: %s\n", path);
    filler(buffer, ".", NULL, 0, FUSE_FILL_DIR_DEFAULTS);
    filler(buffer, "..", NULL, 0, FUSE_FILL_DIR_DEFAULTS);

    // path: /
    // path: /orders    |   /orders/
    // path: /orders/2  |   /orders/2/

    // Togliamo slash alla fine, se c'e'
    char *path_copy = strdup(path);
    printf("\tpath: %s\n", path_copy);
    if (path_copy[strlen(path)-1] == '/') {
        path_copy[strlen(path)-1] = 0;
    }
    
    struct tokens *toks = tokenize_path(path_copy);
    printf("\t\tTable: %s\n", toks->table);
    printf("\t\tRecord: %s\n", toks->record);
    printf("\t\tAttribute: %s\n", toks->attribute);

    int slash_count = COUNT_CHAR(path_copy, '/');
    switch(slash_count) {
        case 0: {
            for (int i=0; i<db_schema->n_tables; i++) {
                printf("\tfile: %s\n", db_schema->tables[i]->name);
                filler(buffer, db_schema->tables[i]->name, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            }
            break;
        }

        case 1: {
            char *record_list[MAX_SIZE];
            int n_records;
            
            if (get_table_rowids(toks->table, record_list, &n_records) == STATUS_DB_ERROR) {
                break;
            }
            
            for (int i=0; i<n_records; i++) {
                printf("\tfile: %s\n", record_list[i]);
                filler(buffer, record_list[i], NULL, 0, FUSE_FILL_DIR_DEFAULTS);
                free(record_list[i]);
            }
            
            break;
        }

        case 2: {
            Schema* table;
            for (int i=0; i<db_schema->n_tables; i++) {
                if (strncmp(toks->table, db_schema->tables[i]->name, strlen(toks->table)) == 0) {
                    table = db_schema->tables[i];
                    break;
                }
            }

            char file[MAX_SIZE];

            // Pk
            for (int i=0; i<table->n_pk; i++) {
                snprintf(file, sizeof(file), "%s.vfs2db", table->pk[i]);
                filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            }

            // Attr
            for (int i=0; i<table->n_attr; i++) {
                snprintf(file, sizeof(file), "%s.vfs2db", table->attr[i]);
                filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            }

            // Fk
            for (int i=0; i<table->n_fks; i++) {
                snprintf(file, sizeof(file), "%s.vfs2db", table->fks[i]->from);
                filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
            }

            break;
        }

        default:
            fprintf(stderr, "\tHow the fuck did you end up here?");
            break;
    }

    free(path_copy);
    free(toks->table);
    free(toks->record);
    free(toks->attribute);
    free(toks);

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
 * 
 */
int vfs2db_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("read: %s\n", path);

    size_t path_len = strlen(path);
    if (path_len < 7) return -1; // Safety check

    char *noext_path = remove_extension(path);
    if (!noext_path) return -ENOMEM;

    struct tokens *toks = tokenize_path(noext_path);

    struct {
        char *bytes;
        size_t size;
    } content;

    content.bytes = NULL;

    if (get_attribute_size(toks, &content.size) == -1) {
        free(toks->table); free(toks->record); free(toks->attribute);
        free(toks); free(noext_path);
        return -1;
    }

    if (get_attribute_bytes(toks, &content.bytes) == -1) {
        free(toks->table); free(toks->record); free(toks->attribute);
        free(toks); free(noext_path); free(content.bytes);
        return -1;
    }

    if (offset >= content.size) {
        free(toks->table); free(toks->record); free(toks->attribute);
        free(toks); free(noext_path); free(content.bytes);
        return 0;
    }

    size_t bytes_available = content.size - offset;
    if (bytes_available > size) {
        bytes_available = size;
    }

    memcpy(buffer, content.bytes + offset, bytes_available);

    // Cleanup
    free(toks->table);
    free(toks->record);
    free(toks->attribute);
    free(toks);
    free(noext_path);
    free(content.bytes);

    return bytes_available;
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
 * 
 */
int vfs2db_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write: %s\n", path);
    printf("\tbuffer: %s\n", buffer);
    printf("\tsize: %zu\n", size);
    printf("\toffset: %d\n", offset);

    char *noext_path = remove_extension(path);
    if (!noext_path) return -ENOMEM;
    
    struct tokens *toks = tokenize_path(noext_path);
    
    int append = (offset == 0) ? 0 : 1;
    int result = update_attribute_value(toks, buffer, size, append);

    free(toks->table);
    free(toks->record);
    free(toks->attribute);
    free(toks);
    free(noext_path);

    // What? FIX
    return result == 0 ? size : result;
}

/**
 * VFS2DB Create
 * @todo Implement the function to insert a new record in the database
 * 
 * @brief Creates a new file in the VFS2DB filesystem, which corresponds to inserting a new record in the database.
 * 
 * @param[in] path The file path to create
 * @param[in] mode The file mode (permissions)
 * @param[in] fi   Pointer to fuse_file_info structure (not used in this case
 * 
 * @return 0 on success, negative error code on failure
 * 
 */
int vfs2db_create(const char* path, mode_t mode, struct fuse_file_info *fi) {
    // if (insert_record(path, mode) == -1)
    //     return -1;
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
 * 
 */
int vfs2db_readlink(const char* path, char* buffer, size_t size) {
    printf("readlink\n");
    char *noext_path = remove_extension(path);
    if (!noext_path) return -ENOMEM;
    struct tokens *toks = tokenize_path(noext_path);

    // Get schema
    Schema* table;
    for (int i=0; i<db_schema->n_tables; i++) {
        if (strncmp(toks->table, db_schema->tables[i]->name, strlen(toks->table)) == 0) {
            table = db_schema->tables[i];
            break;
        }
    }

    // Get fk
    Fk* fk;
    for (int i=0; i<table->n_fks; i++) {
        if (strncmp(toks->attribute, table->fks[i]->from, strlen(toks->attribute)) == 0) {
            fk = table->fks[i];
            break;
        }
    }

    // Get all fks with the same 'table' value
    Fk* fks[table->n_fks];
    int n_same_fks = 0;
    for (int i=0; i<table->n_fks; i++) {
        if (strncmp(fk->table, table->fks[i]->table, strlen(fk->table)) == 0) {
            fks[n_same_fks++] = table->fks[i];
        }
    }

    // Get values of the fks
    char* fk_values[n_same_fks];
    for (int i=0; i<n_same_fks; i++) {
        char* value = NULL;
        struct tokens fk_toks = {
            .table = toks->table,
            .record = toks->record,
            .attribute = fks[i]->from
        };
        if (get_attribute_bytes(&fk_toks, &value) == STATUS_DB_ERROR) {
            return STATUS_DB_ERROR;
        }
        fk_values[i] = value;
    }

    int row_id;
    if (get_rowid_from_pks(fk->table, fks, fk_values, n_same_fks, &row_id) == STATUS_DB_ERROR) {
        return STATUS_DB_ERROR;
    }

    printf("row_id: %d\n", row_id);

    // 6. creare il path del record -> ../../ftable/row_id/fattribute.vfs2db
    snprintf(buffer, size, "../../%s/%d/%s.vfs2db", fk->table, row_id, fk->to);

    free(toks->table);
    free(toks->record);
    free(toks->attribute);
    free(toks);

    return 0;
}