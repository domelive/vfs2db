#include "db_handler.h"

int init_db_schema(DbSchema *db_schema) {
    printf("init_db_schema\n");
    sqlite3_stmt *pstmt;

    int rc = sqlite3_prepare_v2(db, qm_get_query_str(QUERY_GET_TABLES_NAME), -1, &pstmt, NULL);

    if (rc != SQLITE_OK) {
        printf("\t%s\n", sqlite3_errmsg(db));
        return -1;
    }

    int i = 0;
    while ((rc = sqlite3_step(pstmt)) == SQLITE_ROW) {
        const char *name = (const char*)sqlite3_column_text(pstmt, 0);
        db_schema->tables[i] = malloc(sizeof(Schema));
        db_schema->tables[i]->name = strdup(name);
        i++;
    }

    db_schema->n_tables = i;
    return 0;
}

/**
 * Initialize Schema Structure
 * @todo Handle error cases properly
 * 
 * @brief Initializes the Schema structure by retrieving table information
 *        from the database using PRAGMA statements.
 * 
 * @param schema Pointer to Schema structure to initialize
 * 
 * @return 0 on success, -1 on failure
 */
int init_schema(Schema *schema) {
    printf("init_schema\n");
    sqlite3_stmt *pstmt;

    schema->n_pk = 0;
    schema->n_attr = 0;
    schema->n_fks = 0;

    // This query gets: column_name, is_pk, fk_table, fk_column_name
    char query[1024];
    snprintf(query, sizeof(query), qm_get_query_str(QUERY_GET_TABLE_INFO), schema->name, schema->name);
    int rc = sqlite3_prepare_v2(db, query, -1, &pstmt, NULL);

    if (rc != SQLITE_OK) {
        printf("\t%s\n", sqlite3_errmsg(db));
        return -1;
    }

    while ((rc = sqlite3_step(pstmt)) == SQLITE_ROW) {
        const char *column_name = sqlite3_column_text(pstmt, 0);
        const bool is_pk = sqlite3_column_int(pstmt, 1);
        const char *fk_table = sqlite3_column_text(pstmt, 2);
        const char *fk_column_name = sqlite3_column_text(pstmt, 3);

        // Check if primary key
        if (is_pk) {
            // Add to schema pk field
            schema->pk[schema->n_pk] = strdup(column_name);
            schema->n_pk++;
        }
        // Check if foreign key
        else if (fk_table != NULL) {
            // Populate the schema fks field with the foreign key structure
            Fk *fk = malloc(sizeof(Fk));
            fk->from = strdup(column_name);
            fk->table = strdup(fk_table);
            fk->to = strdup(fk_column_name);

            // Add fk to schema
            schema->fks[schema->n_fks] = fk;
            schema->n_fks++;
        }
        // Normal attribute
        else {
            // Add to schema attr field
            schema->attr[schema->n_attr] = strdup(column_name);
            schema->n_attr++;
        }
    }
}

int get_attribute_size(struct tokens* toks) {
    printf("get_attribute_size\n");
    sqlite3_stmt *pstmt;

    printf("\ttoks:\n");
    printf("\t\tattribute: %s\n", toks->attribute);
    printf("\t\ttable: %s\n", toks->table);
    printf("\t\trecord: %s\n", toks->record);

    char *query_fmt = "SELECT %s FROM %s WHERE rowid = ?";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, toks->attribute, toks->table);
    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db,
          (const char*) query_str,
          -1, &pstmt, NULL);

    if (rc != SQLITE_OK) return -1;
    rc = sqlite3_bind_text(pstmt, 1, toks->record, -1, SQLITE_TRANSIENT);
    
    if (rc != SQLITE_OK) { sqlite3_finalize(pstmt); return -1; }
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

    char *query_fmt = "SELECT %s FROM %s WHERE rowid = ?";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, toks->attribute, toks->table);
    printf("\tquery: %s\n", query_str);
    
    int rc = sqlite3_prepare_v2(db, query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) return -1;

    rc = sqlite3_bind_text(pstmt, 1, toks->record, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { sqlite3_finalize(pstmt); return -1; }

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

int get_attribute_type(struct tokens *toks) {
    printf("get_attribute_type\n");

    sqlite3_stmt *pstmt;

    char *query_fmt = "SELECT %s FROM %s WHERE rowid = ?";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, toks->attribute, toks->table);
    printf("\tquery: %s\n", query_str);
    
    int rc = sqlite3_prepare_v2(db, query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) return -1;

    rc = sqlite3_bind_text(pstmt, 1, toks->record, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { sqlite3_finalize(pstmt); return -1; }

    rc = sqlite3_step(pstmt);
    if (rc != SQLITE_ROW) {
        printf("\tNo record matching query\n");
        sqlite3_finalize(pstmt);
        return -1;
    }

    int type = sqlite3_column_type(pstmt, 0);
    
    sqlite3_finalize(pstmt);
    return type;
}

int update_attribute_value(struct tokens* toks, const char* buffer, size_t size, int append) {
    printf("update_attribute_value\n");
    sqlite3_stmt *pstmt;

    const char* query = (append == 0) 
        ? "UPDATE %s SET %s = ? WHERE rowid = ?"
        : "UPDATE %s SET %s = \"%s\" || ? WHERE rowid = ?"; 
    
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query, toks->table, toks->attribute, toks->attribute);

    printf("\tquery_str: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) return -1;

    rc = sqlite3_bind_text(pstmt, 1, buffer, (int)size, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { sqlite3_finalize(pstmt); return -1; }

    rc = sqlite3_bind_text(pstmt, 2, toks->record, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { sqlite3_finalize(pstmt); return -1; }

    rc = sqlite3_step(pstmt);
    sqlite3_finalize(pstmt);

    return (rc == SQLITE_DONE) ? 0 : -1; 
}

void make_root_select(sqlite3_stmt **pstmt) {
    int rc = sqlite3_prepare_v2(db,
          "SELECT name FROM sqlite_schema WHERE type = 'table' AND name NOT LIKE 'sqlite_%';",
          -1, pstmt, NULL);
    if (rc != SQLITE_OK) printf("Not okay...\n");
}

void make_table_select(sqlite3_stmt **pstmt, const char *table) {
    char *query_fmt = "SELECT rowid from %s";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, table);
    int rc = sqlite3_prepare_v2(db,
             (const char*)query_str,
             -1, pstmt, NULL);
}

void make_record_select(sqlite3_stmt **pstmt, const char *table) {
    char *query_fmt = "SELECT name FROM pragma_table_info(\"%s\");";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, table);
    int rc = sqlite3_prepare_v2(db,
          (const char*)query_str,
          -1, pstmt, NULL);
}

void get_table_fks(sqlite3_stmt **pstmt, const char *table) {
    printf("get_table_fks\n");

    char* query_fmt = "SELECT * FROM pragma_foreign_key_list('%s')";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, table);
    
    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, pstmt, NULL);
}

void get_foreign_table_attribute_name(struct tokens *toks, char **ftable, char **fattribute) {
    printf("get_foreign_table_attribute_name\n");
    sqlite3_stmt *pstmt;

    char* query_fmt = "SELECT \"table\", \"to\" FROM pragma_foreign_key_list('%s') WHERE \"from\" = '%s'";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, toks->table, toks->attribute);
    
    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return;
    }

    rc = sqlite3_step(pstmt);

    *ftable = strdup(sqlite3_column_text(pstmt, 0));
    *fattribute = strdup(sqlite3_column_text(pstmt, 1));

    sqlite3_finalize(pstmt);
}

int get_all_fkpk_relationships_length(const char *src_table, const char *dst_table) {
    printf("get_all_fk_pk_relationships_length\n");
    sqlite3_stmt *pstmt;

    char* query_fmt = "SELECT COUNT(*) FROM pragma_foreign_key_list('%s') WHERE \"table\" = '%s'";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, src_table, dst_table);

    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return -1;
    }

    rc = sqlite3_step(pstmt);

    int length = sqlite3_column_int(pstmt, 0);

    sqlite3_finalize(pstmt);

    return length;
}

void get_all_fkpk_relationships(const char *src_table, const char *dst_table, struct pkfk_relation *pkfk) {
    printf("get_all_fk_pk_relationships\n");
    sqlite3_stmt *pstmt;

    char* query_fmt = "SELECT \"from\", \"to\" FROM pragma_foreign_key_list('%s') WHERE \"table\" = '%s'";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, src_table, dst_table);

    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return;
    }

    int i = 0;
    while ((rc = sqlite3_step(pstmt)) == SQLITE_ROW) {
        const char *fk_name = (const char*)sqlite3_column_text(pstmt, 0);
        const char *pk_name = (const char*)sqlite3_column_text(pstmt, 1);

        printf("\tfk_name: %s\n", fk_name);
        printf("\tpk_name: %s\n", pk_name);

        pkfk[i].fk_name = strdup(fk_name);
        pkfk[i].pk_name = strdup(pk_name);

        i++;
    }
}

void fill_fk_values(const char *table, const char *record, struct pkfk_relation *pkfk, int pkfk_length) {
    printf("fill_fk_values\n");

    sqlite3_stmt *pstmt;
    
    int str_len = 0;
    char query_str[1024];
    str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "SELECT ");
    
    int i;
    for (i = 0; i < pkfk_length-1; i++) {
        str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "%s, ", pkfk[i].fk_name);
    }
    str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "%s FROM %s WHERE rowid = ?", pkfk[i].fk_name, table);

    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) {
        printf("\t1 not okay...\n"); 
        sqlite3_finalize(pstmt); 
        return;
    } 
    
    rc = sqlite3_bind_text(pstmt, 1, record, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        printf("\t2 not okay...\n"); 
        sqlite3_finalize(pstmt); 
        return;
    } 

    rc = sqlite3_step(pstmt);

    for (int i=0; i<pkfk_length; i++) {
        pkfk[i].value = strdup(sqlite3_column_text(pstmt, i));
    }

    sqlite3_finalize(pstmt);
}

int get_rowid_from_pks(const char *table, struct pkfk_relation *pkfk, int pkfk_length) {
    printf("get_rowid_from_pks\n");
    sqlite3_stmt *pstmt;
    
    int str_len = 0;
    char query_str[1024];
    str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "SELECT rowid FROM %s WHERE ", table);
    
    for (int i = 0; i < pkfk_length; i++) {
        if (i > 0)
            str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, " AND ");
        str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "%s = '%s'", pkfk[i].pk_name, pkfk[i].value);
    }

    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        return -1;
    }

    rc = sqlite3_step(pstmt);

    int row_id = sqlite3_column_int(pstmt, 0);
    sqlite3_finalize(pstmt);
    return row_id;
}