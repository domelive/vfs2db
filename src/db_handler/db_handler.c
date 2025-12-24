/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (...)
 * @brief  Database Handler Source File
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

#include "db_handler.h"

/**
 * Initialize Database Schema
 * @todo Handle error cases properly
 * 
 * @brief Initializes the DbSchema structure by retrieving table names
 *        from the database.
 * 
 * This function populates the provided DbSchema structure with
 * the names of all tables present in the database.
 * 
 * Uses the following SQL query:
 * 
 * - `SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';`
 * 
 * Where `sqlite_master` is a special table that has the following columns: `| type | name | tbl_name | rootpage | sql |`
 * 
 * @param[out] db_schema Pointer to DbSchema structure to initialize
 * 
 * @return 0 on success, -1 on failure
 * 
 */
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
 *        from the database using `PRAGMA` statements.
 * 
 * This function populates the provided Schema structure with
 * information about the table's columns, primary keys, and foreign keys.
 * 
 * Uses the following `PRAGMA` statements:
 * 
 * - `PRAGMA table_info(table_name)`: column informations `| cid | name | type | notnull | dflt_value | pk |`
 *  
 * - `PRAGMA foreign_key_list(table_name)`: foreign key informations `| id | seq | table | from | to | on_update | on_delete | match |`
 * 
 * @param[out] schema Pointer to Schema structure to initialize
 * 
 * @return 0 on success, -1 on failure
 * 
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
    
    // FIX: finalize the statement?
    return 0;
}

/**
 * Get Attribute Size
 * @todo Handle error cases properly
 * 
 * @brief Retrieves the size (in bytes) of a specific attribute value
 *        for a given record in a table.
 * 
 * This function executes a SQL query to fetch the attribute value
 * and calculates its size in bytes.
 * 
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute information
 * 
 * @return Size of the attribute in bytes on success, -1 on failure
 *  
 */
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

/**
 * Get Attribute Value
 * @todo Handle error cases properly
 * 
 * @brief Retrieves the value of a specific attribute for a given record in a table.
 * 
 * This function executes a SQL query to fetch the attribute value
 * and returns it as a dynamically allocated string.
 * 
 * @param[in]  toks  Pointer to tokens structure containing table, record, and attribute information
 * @param[out] bytes Pointer to a char pointer where the attribute value will be stored
 * @param[out] size  Pointer to a size_t variable where the size of the attribute value will be stored
 * 
 * @return 0 on success, -1 on failure
 * 
 */
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

/**
 * Get Attribute Type
 * @todo Handle error cases properly
 * 
 * @brief Retrieves the data type of a specific attribute for a given record in a table.
 * 
 * This function executes a SQL query to fetch the attribute value
 * and determines its data type.
 * 
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute information
 * 
 * @return SQLite data type constant (e.g., SQLITE_INTEGER, SQLITE_TEXT) on success, -1 on failure
 * 
 */
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

/**
 * Update Attribute Value
 * @todo Handle error cases properly
 * 
 * @brief Updates the value of a specific attribute for a given record in a table.
 * 
 * This function executes a SQL `UPDATE` statement to modify the attribute value.
 * It supports both overwriting the existing value and appending to it.
 * 
 * @param[in] toks   Pointer to tokens structure containing table, record, and attribute information
 * @param[in] buffer Pointer to the new value to set
 * @param[in] size   Size of the new value
 * @param[in] append If non-zero, appends the new value to the existing value; otherwise, overwrites it
 * 
 * @return 0 on success, -1 on failure
 * 
 */
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

/**
 * Make Root Select Statement
 * 
 * @brief Prepares a SQL statement to select all table names from the database.
 * 
 * This function prepares a SQL statement that retrieves the names of all tables
 * in the database, excluding SQLite's internal tables.
 * 
 * @param[out] pstmt Pointer to the SQLite statement to be prepared
 * 
 * @deprecated Use `qm_get_query_str(QUERY_GET_TABLES_NAME)` instead.
 * 
 */
void make_root_select(sqlite3_stmt **pstmt) {
    int rc = sqlite3_prepare_v2(db,
          "SELECT name FROM sqlite_schema WHERE type = 'table' AND name NOT LIKE 'sqlite_%';",
          -1, pstmt, NULL);
    if (rc != SQLITE_OK) printf("Not okay...\n");
}

/**
 * Make Table Select Statement
 * 
 * @brief Prepares a SQL statement to select all row IDs from a specific table.
 * 
 * @param[out] pstmt Pointer to the SQLite statement to be prepared
 * @param[in] table Name of the table to select from
 * 
 */
void make_table_select(sqlite3_stmt **pstmt, const char *table) {
    char *query_fmt = "SELECT rowid from %s";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, table);
    int rc = sqlite3_prepare_v2(db,
             (const char*)query_str,
             -1, pstmt, NULL);
}

/**
 * Make Record Select Statement
 * 
 * @brief Prepares a SQL statement to select all column names from a specific table.
 * 
 * @param[out] pstmt Pointer to the SQLite statement to be prepared
 * @param[in] table Name of the table to select from
 * 
 */
void make_record_select(sqlite3_stmt **pstmt, const char *table) {
    char *query_fmt = "SELECT name FROM pragma_table_info(\"%s\");";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, table);
    int rc = sqlite3_prepare_v2(db,
          (const char*)query_str,
          -1, pstmt, NULL);
}

/**
 * Get Table Foreign Keys
 * 
 * @brief Prepares a SQL statement to retrieve all foreign keys from a specific table.
 * 
 * @param[out] pstmt Pointer to the SQLite statement to be prepared
 * @param[in] table Name of the table to retrieve foreign keys from
 * 
 */
void get_table_fks(sqlite3_stmt **pstmt, const char *table) {
    printf("get_table_fks\n");

    char* query_fmt = "SELECT * FROM pragma_foreign_key_list('%s')";
    char query_str[1024];
    snprintf(query_str, sizeof(query_str), query_fmt, table);
    
    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, pstmt, NULL);
}

/**
 * Get Foreign Table and Attribute Name
 * 
 * @brief Retrieves the foreign table name and attribute name for a given foreign key.
 * 
 * @param[in]  toks       Pointer to tokens structure containing table, record, and attribute information
 * @param[out] ftable     Pointer to a char pointer where the foreign table name will be stored
 * @param[out] fattribute Pointer to a char pointer where the foreign attribute name will be stored
 * 
 */
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

/**
 * Get All Foreign Key to Primary Key Relationships Length
 * 
 * @brief Retrieves the count of all foreign key to primary key relationships between two tables.
 * 
 * @param[in] src_table Source table name
 * @param[in] dst_table Destination table name
 * 
 * @return The count of foreign key to primary key relationships on success, -1 on failure
 */
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

/**
 * Get All Foreign Key to Primary Key Relationships
 * 
 * @brief Retrieves all foreign key to primary key relationships between two tables.
 * 
 * @param[in]  src_table Source table name
 * @param[in]  dst_table Destination table name
 * @param[out] pkfk      Array of pkfk_relation structures to store the relationships
 * 
 */
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

/**
 * Fill Foreign Key Values
 * 
 * @brief Fills the values of foreign keys in the provided pkfk_relation array
 *        based on the specified table and record.
 * 
 * @param[in]  table     Name of the table containing the foreign keys
 * @param[in]  record    Record identifier (rowid) to fetch the foreign key values
 * @param[out] pkfk      Array of pkfk_relation structures to fill with foreign key values
 * @param[in]  pkfk_length Length of the pkfk_relation array
 * 
 */
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

/**
 * Get Row ID from Primary Keys
 * 
 * @brief Retrieves the row ID of a record in a table based on the provided primary key values.
 * 
 * @param[in] table     Name of the table to query
 * @param[in] pkfk      Array of pkfk_relation structures containing primary key names and values
 * @param[in] pkfk_length Length of the pkfk_relation array
 * 
 * @return The row ID of the matching record on success, -1 on failure
 * 
 */
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