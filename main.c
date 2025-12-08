#define FUSE_USE_VERSION 30

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>

#define COUNT_CHAR(str, ch) ({ \
    int count = 0; \
    for (int i = 0; str[i] != '\0'; i++) { \
        if (str[i] == ch) count++; \
    } \
    count; \
})

static sqlite3 *db = NULL;

struct tokens {
    char *table;
    char *record;
    char *attribute;
};

static inline void tokenize_path(const char* path, struct tokens* toks) {
    // path := /table/record/attribute
    // path := table/record/attribute
    if (!path || !toks) return;

    char* path_copy = strdup(path); 
    if (!path_copy) return;

    char *cursor = path_copy;
    if (cursor[0] == '/') cursor++;

    char* t = strtok(cursor, "/");
    if (t) {
        toks->table = strdup(t); 
    } else {
        toks->table = NULL;
    }

    t = strtok(NULL, "/");
    if (t) {
        toks->record = strdup(t);
    } else {
        toks->record = NULL;
    }

    t = strtok(NULL, "/");
    if (t) {
        toks->attribute = strdup(t);
    } else {
        toks->attribute = NULL;
    }

    free(path_copy);
}

static int get_attribute_size(struct tokens* toks) {
    printf("get_attribute_size\n");
    sqlite3_stmt *pstmt;

    char query_str[1024];
    snprintf(query_str, 1024, "SELECT %s FROM %s WHERE rowid = ?1;", toks->attribute, toks->table);

    int rc = sqlite3_prepare_v2(db,
          (const char*) query_str,
          -1, &pstmt, NULL);

    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(pstmt, 1, toks->record, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(pstmt);

    // If there's no record matching the query (should not be possible)
    if (rc != SQLITE_ROW) {
        printf("\tNo record matching query\n");
        sqlite3_finalize(pstmt);
        return -1;
    }

    // Calculate the bytes of the attribute
    size_t att_size = sqlite3_column_bytes(pstmt, 0);
    if (att_size < 0) return -1;

    return att_size; 
}

static void make_root_select(sqlite3_stmt **pstmt) {
    int rc = sqlite3_prepare_v2(db,
          "SELECT name FROM sqlite_schema WHERE type = 'table' AND name NOT LIKE 'sqlite_%';",
          -1, pstmt, NULL);
    if (rc != SQLITE_OK) printf("Not okay...\n");
}

static void make_table_select(sqlite3_stmt **pstmt, const char *table) {
    char query_str[1024];
    snprintf(query_str, 1024, "SELECT rowid FROM %s;", table);
    int rc = sqlite3_prepare_v2(db,
             (const char*)query_str,
             -1, pstmt, NULL);
}

static void make_record_select(sqlite3_stmt **pstmt, const char *table) {
    char query_str[1024];
    snprintf(query_str, 1024, "SELECT name FROM pragma_table_info('%s');", table);
    int rc = sqlite3_prepare_v2(db,
          (const char*)query_str,
          -1, pstmt, NULL);
}

static void vfs2db_destroy(void *private_data) {
    struct fuse_args *args = (struct fuse_args*) private_data;
    if (db) {
        sqlite3_close(db);
        printf("sqlite3_close executed correctly.\n");
    }

    fuse_opt_free_args(args);
    printf("fuse_opt_free_args executed correctly.\n");
}

static int vfs2db_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
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
        st->st_mode = S_IFREG | 0644;
        st->st_uid = getuid();
        st->st_gid = getgid();
        st->st_atime = st->st_mtime = time(NULL);
        st->st_nlink = 1;

        // Get rid of the extension .vfs2db
        char* noext_path = (const char*)malloc(sizeof(path) - 7);
        strncpy(noext_path, path, strlen(path) - 7);
        noext_path[strlen(path) - 7] = 0;
        printf("\tNo extension path: %s\n", noext_path);

        struct tokens toks;
        tokenize_path(noext_path, &toks);
        printf("\t\tTable: %s\n", toks.table);
        printf("\t\tRecord: %s\n", toks.record);
        printf("\t\tAttribute: %s\n", toks.attribute);

        size_t att_size = get_attribute_size(&toks);
        if (att_size < 0) return -1;

        st->st_size = att_size;

        free(toks.table);
        free(toks.record);
        free(toks.attribute);
        free(noext_path);

        printf("\tcontent size: %d\n", att_size);
    }

    return 0;
} 

static int vfs2db_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
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
    
    struct tokens toks;
    tokenize_path(path_copy, &toks);
    printf("\t\tTable: %s\n", toks.table);
    printf("\t\tRecord: %s\n", toks.record);
    printf("\t\tAttribute: %s\n", toks.attribute);

    sqlite3_stmt* pstmt;
    int slash_count = COUNT_CHAR(path_copy, '/');
    switch(slash_count) {
        case 0: // SE SEI NELLA ROOT DEVI FARE UNA QUERY CON LA SQLITE_MASTER
            make_root_select(&pstmt);
            break;
        case 1: // SE SEI DENTRO UNA TABELLA DEVI FARE UNA SELECT SUL ROWID
            make_table_select(&pstmt, toks.table);
            break;
        case 2: // SE SEI DENTRO UN RECORD BASTA FARE UNA SELECT SUL PRAGMA PER OTTENERE I NOMI DEI CAMPI DELLA TABELLA
            make_record_select(&pstmt, toks.table);
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
    return 0;
}

static const struct fuse_operations vfs2db_oper = {
	.getattr        = vfs2db_getattr,
	.readdir        = vfs2db_readdir,
	// .read           = vfs2db_read,
    // .write          = vfs2db_write,
    // .create         = vfs2db_create,
    .destroy        = vfs2db_destroy,
};

struct options {
    const char *db_path;
};

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