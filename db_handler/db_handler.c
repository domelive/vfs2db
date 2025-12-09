#include "db_handler.h"

int get_attribute_size(struct tokens* toks) {
    printf("get_attribute_size\n");
    sqlite3_stmt *pstmt;

    printf("\ttoks:\n");
    printf("\t\tattribute: %s\n", toks->attribute);
    printf("\t\ttable: %s\n", toks->table);
    printf("\t\trecord: %s\n", toks->record);

    char query_str[1024];
    snprintf(query_str, 1024, "SELECT %s FROM %s WHERE rowid = %s;", toks->attribute, toks->table, toks->record);
    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db,
          (const char*) query_str,
          -1, &pstmt, NULL);

    if (rc != SQLITE_OK) return -1;

    rc = sqlite3_step(pstmt);

    // If there's no record matching the query (should not be possible)
    if (rc != SQLITE_ROW) {
        printf("\tNo record matching query\n");
        sqlite3_finalize(pstmt);
        return -1;
    }

    // Calculate the bytes of the attribute
    size_t att_size = sqlite3_column_bytes(pstmt, 0);
    printf("\tattribute_size: %ld", att_size);
    if (att_size < 0) return -1;

    sqlite3_finalize(pstmt);
    return att_size; 
}

int get_attribute_value(struct tokens* toks, char **bytes, size_t *size) {
    printf("get_attribute_value\n");

    sqlite3_stmt *pstmt;

    char query_str[1024];
    snprintf(query_str, 1024, "SELECT %s FROM %s WHERE rowid = %s;", toks->attribute, toks->table, toks->record);
    printf("\tquery: %s\n", query_str);
    
    int rc = sqlite3_prepare_v2(db, query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) return -1;

    rc = sqlite3_step(pstmt);
    if (rc != SQLITE_ROW) {
        printf("\tNo record matching query\n");
        sqlite3_finalize(pstmt);
        return -1;
    }

    *bytes = strdup(sqlite3_column_text(pstmt, 0));
    *size = (size_t)sqlite3_column_bytes(pstmt, 0);

    sqlite3_finalize(pstmt);
    return 0;
}

void make_root_select(sqlite3_stmt **pstmt) {
    int rc = sqlite3_prepare_v2(db,
          "SELECT name FROM sqlite_schema WHERE type = 'table' AND name NOT LIKE 'sqlite_%';",
          -1, pstmt, NULL);
    if (rc != SQLITE_OK) printf("Not okay...\n");
}

void make_table_select(sqlite3_stmt **pstmt, const char *table) {
    char query_str[1024];
    snprintf(query_str, 1024, "SELECT rowid FROM %s;", table);
    int rc = sqlite3_prepare_v2(db,
             (const char*)query_str,
             -1, pstmt, NULL);
}

void make_record_select(sqlite3_stmt **pstmt, const char *table) {
    char query_str[1024];
    snprintf(query_str, 1024, "SELECT name FROM pragma_table_info('%s');", table);
    int rc = sqlite3_prepare_v2(db,
          (const char*)query_str,
          -1, pstmt, NULL);
}