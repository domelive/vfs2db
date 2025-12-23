#include "syscall_handler.h"

extern sqlite3 *db;

static inline struct tokens* tokenize_path(const char* path) {
    // path := /table/record/attribute
    // path := table/record/attribute
    if (!path) return NULL;

    struct tokens *toks = malloc(sizeof(struct tokens));

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

static inline char *remove_extension(const char *path) {
    int noext_path_length = strlen(path) - 7;
    if (noext_path_length <= 0) return NULL;

    char *noext_path = malloc(noext_path_length + 1);
    strncpy(noext_path, path, noext_path_length);
    noext_path[noext_path_length] = 0;

    return noext_path;
}

static inline int check_symlink(struct tokens* toks) {
    printf("check_symlink\n");
    sqlite3_stmt *pstmt;
    get_table_fks  (&pstmt, toks->table);

    printf("\tattribute: %s\n", toks->attribute);
    int rc;
    while ((rc = sqlite3_step(pstmt)) == SQLITE_ROW) {
        const char *attr_name = (const char*)sqlite3_column_text(pstmt, 3);
        printf("\tfk: %s\n", attr_name);
        if (toks->attribute && strncmp(attr_name, toks->attribute, strlen(toks->attribute)) == 0) {
            printf("\tfk found: %s\n", toks->attribute);
            return 1;
        }
    }
    return 0;
}

void *vfs2db_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    printf("init\n");

    // Get all the tables
    DbSchema db_schema;
    init_db_schema(&db_schema);
    printf("\tNumber of tables: %d\n", db_schema.n_tables);

    // For each table, get all the info
    for (int i=0; i<db_schema.n_tables; i++) {
        init_schema(db_schema.tables[i]);
    }

    // Testing
    for (int i=0; i<db_schema.n_tables; i++) {
        // Print table name
        printf("Table: %s\n", db_schema.tables[i]->name);
        // Print pks
        for (int j=0; j<db_schema.tables[i]->n_pk; j++) {
            printf("\tPK: %s\n", db_schema.tables[i]->pk[j]);
        }
        // Print fks
        for (int j=0; j<db_schema.tables[i]->n_fks; j++) {
            printf("\tFK: %s -> %s(%s)\n", db_schema.tables[i]->fks[j]->from,
                                           db_schema.tables[i]->fks[j]->table,
                                           db_schema.tables[i]->fks[j]->to);
        }
        // Print attributes
        for (int j=0; j<db_schema.tables[i]->n_attr; j++) {
            printf("\tATT: %s\n", db_schema.tables[i]->attr[j]);
        }
    }

    return NULL;
}

void vfs2db_destroy(void *private_data) {
    struct fuse_args *args = (struct fuse_args*) private_data;
    if (db) {
        sqlite3_close(db);
        printf("sqlite3_close executed correctly.\n");
    }

    fuse_opt_free_args(args);
    printf("fuse_opt_free_args executed correctly.\n");
}

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

        size_t att_size = get_attribute_size(toks);
        if (att_size < 0) return -1;

        st->st_size = att_size;

        free(toks->table);
        free(toks->record);
        free(toks->attribute);
        free(toks);
        free(noext_path);

        printf("\tcontent size: %d\n", att_size);
    }

    return 0;
}

int vfs2db_getxattr(const char *path, const char *name, char *value, size_t size) {
    if (strcmp(name, "user.type") != 0) return -ENODATA;

    char *noext_path = remove_extension(path);
    if (!noext_path) return -ENOMEM;
    
    struct tokens *toks = tokenize_path(noext_path);

    const char* t_str;
    switch (get_attribute_type(toks)) {
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

    sqlite3_stmt* pstmt;
    int slash_count = COUNT_CHAR(path_copy, '/');
    switch(slash_count) {
        case 0: // SE SEI NELLA ROOT DEVI FARE UNA QUERY CON LA SQLITE_MASTER
            make_root_select(&pstmt);
            break;
        case 1: // SE SEI DENTRO UNA TABELLA DEVI FARE UNA SELECT SUL ROWID
            make_table_select(&pstmt, toks->table);
            break;
        case 2: // SE SEI DENTRO UN RECORD BASTA FARE UNA SELECT SUL PRAGMA PER OTTENERE I NOMI DEI CAMPI DELLA TABELLA
            make_record_select(&pstmt, toks->table);
            break;
        default:
            fprintf(stderr, "\tHow the fuck did you end up here?");
            return -1;
    }
     
    const char *pattern = slash_count == 2 ? "%s.vfs2db" : "%s";

    int rc;
    while ((rc = sqlite3_step(pstmt)) == SQLITE_ROW) {
        const char *attr_name = (const char*)sqlite3_column_text(pstmt, 0);

        char file[1024];
        snprintf(file, 1024, pattern, attr_name);

        printf("\tfile: %s\n", file);
        if (*file != '\0') {
            filler(buffer, file, NULL, 0, FUSE_FILL_DIR_DEFAULTS);
        }
    }

    sqlite3_finalize(pstmt);

    free(path_copy);
    free(toks->table);
    free(toks->record);
    free(toks->attribute);
    free(toks);

    return 0;
}

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

    if (get_attribute_value(toks, &content.bytes, &content.size) == -1) {
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

    return result == 0 ? size : result;
}

int vfs2db_create(const char* path, mode_t mode, struct fuse_file_info *fi) {
    // if (insert_record(path, mode) == -1)
    //     return -1;
    return 0;
}

int vfs2db_readlink(const char* path, char* buffer, size_t size) {
    printf("readlink\n");
    char *noext_path = remove_extension(path);
    if (!noext_path) return -ENOMEM;
    struct tokens *toks = tokenize_path(noext_path);

    // 1. dalla path capire la tabella esterna del record
    char *ftable; char* fattribute;
    get_foreign_table_attribute_name(toks, &ftable, &fattribute);

    // abbiamo il nome della tabella cui campo (toks->attribute) e' riferito
    // ma attenzione! per ricavare il rowid, abbiamo bisogno di tutte le chiavi primarie
    // della tabella riferita --> dobbiamo prendere eventuali altri chiavi esterne della tabella riferita

    // lo facciamo accoppiando tutte le chiavi esterne di toks->table che fanno riferimento
    // a ftable
    int num = get_all_fkpk_relationships_length(toks->table, ftable);
    struct pkfk_relation *pkfk = malloc(num * sizeof(struct pkfk_relation));
    if (!pkfk) return -1;
    get_all_fkpk_relationships(toks->table, ftable, pkfk);
    
    // una volta ottenute queste coppie, devo conoscere i valori delle chiavi esterne
    fill_fk_values(toks->table, toks->record, pkfk, num);

    // quindi mi basta fare una query del tipo SELECT rowid FROM ftable WHERE pk1=vfk1, pk2=vfk2, ...
    int row_id = get_rowid_from_pks(ftable, pkfk, num);

    // 6. creare il path del record -> ../../ftable/row_id/fattribute.vfs2db
    snprintf(buffer, size, "../../%s/%d/%s.vfs2db", ftable, row_id, fattribute);

    free(toks->table);
    free(toks->record);
    free(toks->attribute);
    free(toks);
    free(ftable);
    free(fattribute);

    return 0;
}