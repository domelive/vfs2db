/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.c
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
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
status_t init_db_schema(DbSchema *db_schema) {
    printf("init_db_schema\n");
    sqlite3_stmt *pstmt = qm_get_static_query_statement(QUERY_SELECT_TABLES_NAME);

    int i = 0;
    while (sqlite3_step(pstmt) == SQLITE_ROW) {
        const char *name = (const char*)sqlite3_column_text(pstmt, 0);
        db_schema->tables[i] = malloc(sizeof(Schema));
        db_schema->tables[i]->name = strdup(name);
        i++;
    }

    db_schema->n_tables = i;

    return STATUS_OK;
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
status_t init_schema(Schema *schema) {
    printf("init_schema\n");
    sqlite3_stmt *pstmt;

    schema->n_pk = 0;
    schema->n_attr = 0;
    schema->n_fks = 0;

    // This query gets: column_name, is_pk, fk_table, fk_column_name
    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_INFO, schema->name, schema->name);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *column_name = sqlite3_column_text(stmt, 0);
        const bool is_pk = sqlite3_column_int(stmt, 1);
        const char *fk_table = sqlite3_column_text(stmt, 2);
        const char *fk_column_name = sqlite3_column_text(stmt, 3);
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
    
    sqlite3_finalize(stmt);
    return STATUS_OK;
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
status_t get_attribute_size(struct tokens* toks, size_t *size) {
    printf("get_attribute_size\n");
    sqlite3_stmt *pstmt;

    printf("\ttoks:\n");
    printf("\t\tattribute: %s\n", toks->attribute);
    printf("\t\ttable: %s\n", toks->table);
    printf("\t\trecord: %s\n", toks->record);

    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE, toks->attribute, toks->table);
    
    if(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        printf("\t%s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }
    
    // If there's no record matching the query (should not be possible)
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        printf("\t%s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    // Calculate the bytes of the attribute
    *size = sqlite3_column_bytes(stmt, 0);
    
    if (*size < 0) {
        printf("\t%s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }
    printf("\tattribute_size: %ld\n", *size);
    
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

/**
 * Get Attribute Bytes
 * @todo Handle error cases properly
 * 
 * @brief Retrieves the bytes of a specific attribute for a given record in a table.
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
status_t get_attribute_bytes(struct tokens* toks, char **bytes) {
    printf("get_attribute_value\n");
    
    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE, toks->attribute, toks->table);

    if (sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT) != SQLITE_OK) { 
        printf("\t%s\n", sqlite3_errmsg(db));        
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR; 
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        printf("\t%s\n", sqlite3_errmsg(db));        
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    *bytes = strdup(sqlite3_column_text(stmt, 0));

    sqlite3_finalize(stmt);
    return STATUS_OK;
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
status_t get_attribute_type(struct tokens *toks, int* type) {
    printf("get_attribute_type\n");

    sqlite3_stmt *stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE, toks->attribute, toks->table);

    if (sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        printf("\t%s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        printf("\t%s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    *type = sqlite3_column_type(stmt, 0);
    
    sqlite3_finalize(stmt);
    return STATUS_OK;
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
status_t update_attribute_value(struct tokens* toks, const char* buffer, size_t size, int append) {
    printf("update_attribute_value\n");
    
    sqlite3_stmt *stmt = (append == 0)
        ? qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE, toks->attribute, toks->table)
        : qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE_APPEND, toks->attribute, toks->table);

    if (sqlite3_bind_text(stmt, 1, buffer, (int)size, SQLITE_TRANSIENT) != SQLITE_OK) { 
        sqlite3_finalize(stmt);
        printf("\t%s\n", sqlite3_errmsg(db));
        return STATUS_DB_ERROR; 
    }

    if (sqlite3_bind_text(stmt, 2, toks->record, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        printf("\t%s\n", sqlite3_errmsg(db));
        return STATUS_DB_ERROR;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        printf("\t%s\n", sqlite3_errmsg(db));
        return STATUS_DB_ERROR;
    }
    
    sqlite3_finalize(stmt);
    return STATUS_OK;
}

/**
 * Get Table Row IDs
 * 
 * @brief Prepares and executes a SQL statement to select all row IDs from a specified table.
 * 
 * @param[in]  table      Name of the table to query
 * @param[out] records    Array of strings to store the retrieved row IDs
 * @param[out] n_records  Pointer to an integer to store the number of retrieved records
 * 
 * @return 0 on success, -1 on failure
 * 
 */
status_t get_table_rowids(const char *table, char *records[], int *n_records) {
    sqlite3_stmt *stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_ROWIDS, table);

    if (stmt == NULL) {
        printf("Failed to prepare table select statement for table %s\n", table);
        return STATUS_DB_ERROR;
    }

    int record_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *rowid = sqlite3_column_text(stmt, 0);
        records[record_count++] = strdup(rowid); 
    }

    *n_records = record_count;

    sqlite3_finalize(stmt);
    return STATUS_OK;
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
status_t get_rowid_from_pks(const char *table, Fk* fks[], char* fks_values[], int num_fks, int* rowid) {
    // FIX: this function should be implemented better using the query manager prepared statements
    printf("get_rowid_from_pks\n");
    sqlite3_stmt *pstmt;
    
    int str_len = 0;
    char query_str[1024];
    str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "SELECT rowid FROM %s WHERE ", table);
    
    for (int i = 0; i < num_fks; i++) {
        if (i > 0)
            str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, " AND ");
        str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "%s = '%s'", fks[i]->to, fks_values[i]);
    }

    printf("\tquery: %s\n", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*) query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(pstmt);
        printf("\t%s\n", sqlite3_errmsg(db));
        return STATUS_DB_ERROR;
    }

    rc = sqlite3_step(pstmt);

    *rowid = sqlite3_column_int(pstmt, 0);
    
    sqlite3_finalize(pstmt);
    return STATUS_OK;
}