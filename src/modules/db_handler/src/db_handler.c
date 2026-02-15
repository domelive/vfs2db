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

#define BLOCK_OFFSET(offset) ((off_t)((offset / BLOCK_SIZE) * BLOCK_SIZE))

static inline int parse_sqlite_type(const char* typestr) {
    LOG_TRACE("Parsing SQLITE type...");

    if (!typestr) {
        LOG_TRACE("Column has no type affinity, defaulting to TEXT");
        return SQLITE_TEXT;
    }

    if (strcasestr(typestr, "INT")) {
        LOG_TRACE("Column type '%s' has INTEGER affinity", typestr);
        return SQLITE_INTEGER;
    }

    if (strcasestr(typestr, "CHAR") || strcasestr(typestr, "CLOB") || strcasestr(typestr, "TEXT")) {
        LOG_TRACE("Column type '%s' has TEXT affinity", typestr);
        return SQLITE_TEXT;
    }

    if (strcasestr(typestr, "BLOB")) {
        LOG_TRACE("Column type '%s' has BLOB affinity", typestr);
        return SQLITE_BLOB;
    }

    if (strcasestr(typestr, "REAL") || strcasestr(typestr, "FLOA") || strcasestr(typestr, "DOUB")) {
        LOG_TRACE("Column type '%s' has FLOAT affinity", typestr);
        return SQLITE_FLOAT;
    }

    LOG_TRACE("Fallback to SQLITE_TEXT");

    return SQLITE_TEXT;
}

static inline status_t get_cache_key_from_toks(struct tokens* toks, off_t block_offset, CacheKey** key) {
    // Rebuild the path from toks
    char path[MAX_SIZE];
    snprintf(path, MAX_SIZE, "/%s/%s/%s", toks->table, toks->record, toks->attribute);

    LOG_TRACE("Getting attribute bytes: path='%s', block_offset=%ld", path,
              block_offset);

    *key = calloc(1, sizeof(CacheKey));
    if (!(*key)) {
        LOG_ERROR("Failed to allocate CacheKey");
        return STATUS_DB_ERROR;
    }

    strncpy((*key)->query, path, strlen(path) + 1);
    (*key)->offset = block_offset;

    return STATUS_OK;
}

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
 * Where `sqlite_master` is a special table that has the following columns: `| type | name |
 * tbl_name | rootpage | sql |`
 *
 * @param[out] db_schema Pointer to DbSchema structure to initialize
 *
 * @return 0 on success, -1 on failure
 */
status_t init_db_schema(DbSchema* db_schema) {
    LOG_DEBUG("Initializing database schema...");

    db_schema->tables_head = NULL;

    sqlite3_stmt* pstmt = qm_get_static_query_statement(QUERY_SELECT_TABLES_NAME);
    while (sqlite3_step(pstmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(pstmt, 0);

        Schema* new_schema = calloc(1, sizeof(Schema));
        if (!new_schema) {
            LOG_ERROR("Failed to allocate memory for schema '%s'", name);
            return STATUS_DB_ERROR;
        }

        new_schema->name = strdup(name);
        add_schema(db_schema, new_schema);

        LOG_TRACE("Found table: %s", name);
    }

    LOG_INFO("Database schema initialized: %d tables found.", count_schemas(db_schema));

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
 * - `PRAGMA table_info(table_name)`: column informations `| cid | name | type | notnull |
 * dflt_value | pk |`
 *
 * - `PRAGMA foreign_key_list(table_name)`: foreign key informations `| id | seq | table | from
 * | to | on_update | on_delete | match |`
 *
 * @param[out] schema Pointer to Schema structure to initialize
 *
 * @return 0 on success, -1 on failure
 */
status_t init_schema(Schema* schema) {
    LOG_DEBUG("Initializing schema for table: %s", schema->name);

    // This query gets: column_name, is_pk, fk_table, fk_column_name
    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_INFO,
                                                          schema->name, schema->name);
    if (!stmt) {
        LOG_ERROR("Failed to build query statement for table info: '%s'", schema->name);
        return STATUS_DB_ERROR;
    }

    int pk_count = 0, fk_count = 0, attr_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* column_name     = sqlite3_column_text(stmt, 0);
        const char* column_type_str = sqlite3_column_text(stmt, 1);
        const bool  is_pk           = sqlite3_column_int(stmt, 2);
        const char* fk_table        = sqlite3_column_text(stmt, 3);
        const char* fk_column_name  = sqlite3_column_text(stmt, 4);
        // Check if primary key
        if (is_pk) {
            // Add to schema pk field
            Pk* pk = malloc(sizeof(Pk));
            if (!pk) {
                LOG_ERROR("Failed to allocate memory for primary key '%s' in table '%s'",
                          column_name, schema->name);
                sqlite3_finalize(stmt);
                return STATUS_DB_ERROR;
            }

            pk->name = strdup(column_name);
            add_pk_to_schema(schema, pk);

            LOG_TRACE("  PK: %s", column_name);
            pk_count++;
        }
        // Check if foreign key
        else if (fk_table != NULL) {
            // Add fk to schema
            Fk* fk = malloc(sizeof(Fk));
            if (!fk) {
                LOG_ERROR("Failed to allocate FK for column '%s'", column_name);
                sqlite3_finalize(stmt);
                return STATUS_DB_ERROR;
            }

            fk->from  = strdup(column_name);
            fk->table = strdup(fk_table);
            fk->to    = strdup(fk_column_name);
            add_fk_to_schema(schema, fk);

            LOG_TRACE("  FK: %s -> %s(%s)", column_name, fk_table, fk_column_name);
            fk_count++;
        }
        // Normal attribute
        else {
            // Add to schema attr field
            Attr* attr = malloc(sizeof(Attr));
            if (!attr) {
                LOG_ERROR("Failed to allocate Attr for column '%s'", column_name);
                sqlite3_finalize(stmt);
                return STATUS_DB_ERROR;
            }
            attr->name        = strdup(column_name);
            attr->sqlite_type = parse_sqlite_type(column_type_str);
            add_attribute_to_schema(schema, attr);

            LOG_TRACE("  Attr: %s", column_name);
            attr_count++;
        }
    }

    LOG_DEBUG("Schema '%s' initialized: %d PKs, %d FKs, %d attrs", schema->name, pk_count, fk_count,
              attr_count);

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
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute
 * information
 *
 * @return Size of the attribute in bytes on success, -1 on failure
 */
status_t get_attribute_size(struct tokens* toks, size_t* size) {
    LOG_TRACE("Getting attribute size: %s/%s/%s", toks->table, toks->record, toks->attribute);

    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE,
                                                          toks->attribute, toks->table);
    if (!stmt) {
        LOG_ERROR("Failed to build SELECT query for attribute size");
        return STATUS_DB_ERROR;
    }

    if (sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        LOG_SQLITE_ERROR(db);
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    // If there's no record matching the query (should not be possible)
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        LOG_ERROR("No row found for %s/%s/%s", toks->table, toks->record, toks->attribute);
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    // Calculate the bytes of the attribute
    *size = sqlite3_column_bytes(stmt, 0);

    LOG_TRACE("Attribute size: %zu bytes", *size);

    if (*size < 0) {
        LOG_SQLITE_ERROR(db);
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

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
 * @param[in]  toks  Pointer to tokens structure containing table, record, and attribute
 * information
 * @param[out] bytes Pointer to a char pointer where the attribute value will be stored
 * @param[out] size  Pointer to a size_t variable where the size of the attribute value will be
 * stored
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t get_attribute_chunk_bytes(struct tokens* toks, off_t offset, char** bytes) {
    CacheKey* key = NULL;
    if (cache_enabled) {
        if (get_cache_key_from_toks(toks, BLOCK_OFFSET(offset), &key) != STATUS_OK) {
            LOG_ERROR("Failed to get cache key from tokens");
            return STATUS_CACHE_ERROR;
        }

        CacheBlock* blk;
        // CACHE HIT
        if ((blk = cache_get(key)) != NULL) {
            LOG_DEBUG("Cache hit for '%s' at offset %ld", key->query, key->offset);
            *bytes = (char*)blk->data;
            return STATUS_OK;
        }

        // CACHE MISS
        LOG_DEBUG("Cache miss for '%s' at offset %ld, fetching from DB", key->query, key->offset);
    }

    // Build the statement
    sqlite3_stmt* stmt =
        qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_CHUNK_ATTRIBUTE, toks->attribute,
                                         BLOCK_OFFSET(offset), BLOCK_SIZE, toks->table);
    if (!stmt) {
        LOG_ERROR("Failed to build chunk query");
        free(key);
        return STATUS_DB_ERROR;
    }

    if (sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        LOG_SQLITE_ERROR(db);
        sqlite3_finalize(stmt);
        free(key);
        return STATUS_DB_ERROR;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        LOG_ERROR("No data found for chunk query");
        sqlite3_finalize(stmt);
        free(key);
        return STATUS_DB_ERROR;
    }

    const char* data = strdup((char*)sqlite3_column_text(stmt, 0));
    const size_t data_size = (size_t)sqlite3_column_bytes(stmt, 0);

    // Calculate relative offset
    size_t relative_offset = offset - BLOCK_OFFSET(offset);
    *bytes                 = data + relative_offset;

    if (cache_enabled) {
        CacheBlock* blk = malloc(sizeof(CacheBlock));
        if (!blk) {
            LOG_ERROR("Failed to allocate CacheBlock");
            sqlite3_finalize(stmt);
            free(key);
            return STATUS_DB_ERROR;
        }
        blk->key         = *key;
        blk->data        = data;
        blk->actual_size = data_size;

        cache_add_block(blk);
        LOG_DEBUG("Fetched %zu bytes from DB and cached", blk->actual_size);

        free(key);  // Content copied to blk->key
        cache_view();
    }

    sqlite3_finalize(stmt);
    return STATUS_OK;
}

status_t get_attribute_all_bytes(struct tokens* toks, char** bytes, size_t* size) {
    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE,
                                                          toks->attribute, toks->table);
    if (!stmt) {
        LOG_ERROR("Failed to build SELECT query for update");
        return STATUS_DB_ERROR;
    }

    if (sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        LOG_SQLITE_ERROR(db);
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        LOG_ERROR("No row found for update query");
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    *bytes = strdup((char*)sqlite3_column_text(stmt, 0));
    *size = (size_t) sqlite3_column_bytes(stmt, 0);

    if (!bytes) {
        LOG_ERROR("Failed to retrieve existing attribute value for update");
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

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
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute
 * information
 *
 * @return SQLite data type constant (e.g., SQLITE_INTEGER, SQLITE_TEXT) on success, -1 on
 * failure
 */
status_t get_attribute_type(struct tokens* toks, int* type) {
    LOG_TRACE("Getting attribute type: %s/%s/%s", toks->table, toks->record, toks->attribute);

    Schema* table_schema = find_schema_by_name(db_schema, toks->table);
    if (!table_schema) {
        LOG_ERROR("Table '%s' not found in schema", toks->table);
        return STATUS_DB_ERROR;
    }

    Attr* attr = NULL;
    HASH_FIND_STR(table_schema->attr_head, toks->attribute, attr);
    if (!attr) {
        LOG_ERROR("Attribute '%s' not found in table '%s'", toks->attribute, toks->table);
        return STATUS_DB_ERROR;
    }

    *type = attr->sqlite_type;
    LOG_TRACE("Attribute type for '%s/%s/%s': %d", toks->table, toks->record, toks->attribute,
              *type);

    return STATUS_OK;
}

static inline status_t bind_attribute_value(sqlite3_stmt* stmt, char* value, int type) {
    switch (type) {
    case SQLITE_INTEGER: {
        sqlite3_int64 val = strtoll(value, NULL, 10);
        if (sqlite3_bind_int64(stmt, 1, val) != SQLITE_OK) {
            LOG_SQLITE_ERROR(db);
            return STATUS_DB_ERROR;
        }
        break;
    }
    case SQLITE_FLOAT: {
        double val = strtod(value, NULL);
        if (sqlite3_bind_double(stmt, 1, val) != SQLITE_OK) {
            LOG_SQLITE_ERROR(db);
            return STATUS_DB_ERROR;
        }
        break;
    }
    case SQLITE_TEXT: {
        if (sqlite3_bind_text(stmt, 1, value, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            LOG_SQLITE_ERROR(db);
            return STATUS_DB_ERROR;
        }
        break;
    }
    case SQLITE_BLOB:
    default: {
        if (sqlite3_bind_blob(stmt, 1, value, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            LOG_SQLITE_ERROR(db);
            return STATUS_DB_ERROR;
        }
        break;
    }
    }
}

/**
 * Update Attribute Value
 */
status_t update_attribute_value(struct tokens* toks, const char* buffer, size_t size, off_t offset) {
    LOG_DEBUG("Updating attribute: %s/%s/%s (size=%zu)", toks->table, toks->record, toks->attribute,
              size);
    
    // Check type
    int type;
    if (get_attribute_type(toks, &type) != STATUS_OK) {
        LOG_ERROR("Failed to get attribute type for update");
        return STATUS_DB_ERROR;
    }

    if (type == SQLITE_TEXT || type == SQLITE_BLOB) {
        // Get current attribute size
        size_t current_size;
        if (get_attribute_size(toks, &current_size) != STATUS_OK) {
            LOG_ERROR("Failed to get attribute size for blob/text update");
            return STATUS_DB_ERROR;
        }

        size_t end_of_write = offset + size;

        // If we need to expand, extend the blob in-place using SQL concatenation
        // with zeroblob --> no full copy into RAM
        if (end_of_write > current_size) {
            size_t expand_by = end_of_write - current_size;
            LOG_DEBUG("Expanding blob by %zu bytes (current=%zu, needed=%zu)",
                      expand_by, current_size, end_of_write);

            sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ZERO_BLOB,
                                                                  toks->table, toks->attribute,
                                                                  toks->attribute);

            if (sqlite3_bind_int(stmt, 1, (int)expand_by) != SQLITE_OK) {
                LOG_SQLITE_ERROR(db);
                sqlite3_finalize(stmt);
                return STATUS_DB_ERROR;
            }

            if (sqlite3_bind_int64(stmt, 2, atoi(toks->record)) != SQLITE_OK) {
                LOG_SQLITE_ERROR(db);
                sqlite3_finalize(stmt);
                return STATUS_DB_ERROR;
            }

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                LOG_ERROR("Failed to expand blob: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                return STATUS_DB_ERROR;
            }

            sqlite3_finalize(stmt);
            LOG_DEBUG("Blob expanded successfully to %zu bytes", end_of_write);
        }

        // Now write the data in-place using the Blob I/O API
        sqlite3_blob* blob_handle = NULL;
        if (sqlite3_blob_open(db, "main", toks->table, toks->attribute, atoll(toks->record), 1, &blob_handle) != SQLITE_OK) {
            LOG_ERROR("Failed to open blob handle: %s", sqlite3_errmsg(db));
            return STATUS_DB_ERROR;
        }

        if (sqlite3_blob_write(blob_handle, buffer, (int)size, (int)offset) != SQLITE_OK) {
            LOG_ERROR("Failed to write to blob: %s", sqlite3_errmsg(db));
            sqlite3_blob_close(blob_handle);
            return STATUS_DB_ERROR;
        }

        LOG_DEBUG("Blob updated in-place via Blob I/O (offset=%ld, size=%zu)", offset, size);
        sqlite3_blob_close(blob_handle);
    } else {
        // Read all the attribute bytes
        char* bytes;
        size_t bytes_size;
        if (get_attribute_all_bytes(toks, &bytes, &bytes_size) != STATUS_OK) {
            LOG_ERROR("Failed to get all bytes for attribute update");
            return STATUS_DB_ERROR;
        }

        // Calculate new size after update
        size_t new_bytes_size = (offset + size > bytes_size) ? offset + size : bytes_size;
        LOG_TRACE("Current attribute, new data size after update: %zu bytes", new_bytes_size);

        // Allocate new buffer for the updated attribute value
        char* new_bytes = calloc(1, new_bytes_size);
        if (!new_bytes) {
            LOG_ERROR("Failed to allocate memory for new attribute bytes");
            free(bytes);
            return STATUS_DB_ERROR;
        }
        
        // Copy existing bytes and patch with new data
        memcpy(new_bytes, bytes, bytes_size);
        memcpy(new_bytes + offset, buffer, size);

        // Write the new attribute bytes
        sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE, toks->table,
                                                toks->attribute);
        if (!stmt) {
            LOG_ERROR("Failed to build UPDATE query for attribute");
            free(new_bytes);
            return STATUS_DB_ERROR;
        }

        // Bind the new value based on its SQLite type
        if (bind_attribute_value(stmt, new_bytes, type) != STATUS_OK) {
            LOG_ERROR("Failed to bind new attribute value for update");
            sqlite3_finalize(stmt);
            free(new_bytes);
            return STATUS_DB_ERROR;
        }

        // Bind the record ID for the WHERE clause
        if (sqlite3_bind_int64(stmt, 2, atoi(toks->record)) != SQLITE_OK) {
            LOG_SQLITE_ERROR(db);
            sqlite3_finalize(stmt);
            free(new_bytes);
            return STATUS_DB_ERROR;
        }

        // Execute the UPDATE statement
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            LOG_ERROR("Failed to execute UPDATE query for attribute");
            sqlite3_finalize(stmt);
            free(new_bytes);
            return STATUS_DB_ERROR;
        }

        int changes = sqlite3_changes(db);
        LOG_DEBUG("Attribute updated successfully, %d rows affected", changes);
        sqlite3_finalize(stmt);
    }

    if (cache_enabled) {
        // Remove ALL blocks with key that has path = rebuilt path
        CacheKey* key = NULL;
        if (get_cache_key_from_toks(toks, 0, &key) != STATUS_OK) {
            LOG_ERROR("Failed to get cache key from tokens for eviction");
            return STATUS_CACHE_ERROR;
        }
        LOG_TRACE("Evicting cache blocks for updated attribute: path='%s'", key->query);
        cache_evict_blocks_by_path(key->query);
        free(key);
    }

    return STATUS_OK;
}

/**
 * Set Attribute to NULL
 * @todo Handle error cases properly
 *
 * @brief Sets the value of a specific attribute to NULL for a given record in a table.
 *
 * This function executes a SQL `UPDATE` statement to set the attribute value to NULL.
 *
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute
 * information
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t set_attribute_null(struct tokens* toks) {
    LOG_DEBUG("Setting attribute to NULL: %s/%s/%s", toks->table, toks->record, toks->attribute);

    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE,
                                                          toks->table, toks->attribute);
    if (!stmt) {
        LOG_ERROR("Failed to build UPDATE query for setting attribute to NULL");
        return STATUS_DB_ERROR;
    }

    // NOTE: it should NOT nullify the field, because it can have a 'NOT NULL' constraint
    if (sqlite3_bind_null(stmt, 1) != SQLITE_OK) {
        LOG_SQLITE_ERROR(db);
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    if (sqlite3_bind_int64(stmt, 2, atoi(toks->record)) != SQLITE_OK) {
        LOG_SQLITE_ERROR(db);
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LOG_ERROR("Failed to execute UPDATE query for setting attribute to NULL");
        sqlite3_finalize(stmt);
        return STATUS_DB_ERROR;
    }

    int changes = sqlite3_changes(db);
    LOG_DEBUG("Attribute set to NULL successfully, %d rows affected", changes);
    sqlite3_finalize(stmt);

    if (cache_enabled) {
        // Evict the corresponding cache blocks, if exists
        CacheKey* key = NULL;
        if (get_cache_key_from_toks(toks, 0, &key) != STATUS_OK) {
            LOG_ERROR("Failed to get cache key from tokens for eviction");
            return STATUS_CACHE_ERROR;
        }
        LOG_TRACE("Evicting cache blocks for NULLified attribute: path='%s'", key->query);
        cache_evict_blocks_by_path(key->query);
        free(key);
    }

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
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t get_table_rowids(const char* table, char* records[], int* n_records) {
    LOG_TRACE("Getting row IDs for table: %s", table);

    CacheKey* key = NULL;
    if (cache_enabled) {
        struct tokens toks = {
            .table = table,
            .attribute = "",
            .record = ""
        };

        // VERY BIG ASSUMPTION ON BLOCK_OFFSET=0
        if (get_cache_key_from_toks(&toks, 0, &key) != STATUS_OK) {
            LOG_ERROR("Failed to get cache key from tokens for rowids");
            return STATUS_CACHE_ERROR;
        }


        // CACHE HIT
        CacheBlock* blk;
        if ((blk = cache_get(key)) != NULL) {
            LOG_DEBUG("Cache hit for table rowids: %s", table);
            *n_records = blk->actual_size;

            char** cached_records = (char**)blk->data;
            for (int i = 0; i < *n_records; i++) {
                records[i] = cached_records[i];
            }

            return STATUS_OK;
        }

        // CACHE MISS
        LOG_DEBUG("Cache miss for table rowids: %s", table);
    }

    sqlite3_stmt* stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_ROWIDS, table);

    if (!stmt) {
        LOG_ERROR("Failed to build rowids query for table: %s", table);
        free(key);
        return STATUS_DB_ERROR;
    }

    int record_count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* rowid       = (const char*)sqlite3_column_text(stmt, 0);
        records[record_count++] = strdup(rowid);

        if (record_count >= MAX_SIZE) {
            LOG_WARN("Table '%s' has more than %d rows, truncating", table, MAX_SIZE);
            break;
        }
    }

    *n_records = record_count;
    LOG_DEBUG("Found %d rows in table '%s'", record_count, table);

    // populate cache
    if (cache_enabled) {
        CacheBlock* blk = malloc(sizeof(CacheBlock));
        if (!blk) {
            LOG_WARN("Failed to allocate cache block for rowids, continuing without cache");
            free(key);
            sqlite3_finalize(stmt);
            return STATUS_OK; // Non-fatal, we have the data
        }

        char** records_copy = malloc(record_count * sizeof(char*));
        if (!records_copy) {
            LOG_WARN("Failed to allocate records copy for cache");
            free(blk);
            free(key);
            sqlite3_finalize(stmt);
            return STATUS_OK;
        }

        for (int i = 0; i < record_count; i++) {
            records_copy[i] = strdup(records[i]);
        }

        blk->key         = *key;
        blk->data        = records_copy;
        blk->actual_size = record_count;

        free(key);  // Content copied to blk->key
        cache_add_block(blk);

        cache_view();
    }

    sqlite3_finalize(stmt);
    return STATUS_OK;
}

/**
 * Get Row ID from Primary Keys
 *
 * @brief Retrieves the row ID of a record in a table based on the provided primary key values.
 *
 * @param[in] table     Name of the table to query
 * @param[in] pkfk      Array of pkfk_relation structures containing primary key names and
 * values
 * @param[in] pkfk_length Length of the pkfk_relation array
 *
 * @return The row ID of the matching record on success, -1 on failure
 */
status_t get_rowid_from_pks(const char* table, Fk* fks[], char* fks_values[], int num_fks,
                            int* rowid) {
    LOG_TRACE("Getting rowid from PKs for table: %s (num_fks=%d)", table, num_fks);

    // FIX: this function should be implemented better using the query manager prepared
    // statements
    sqlite3_stmt* pstmt;

    int  str_len = 0;
    char query_str[1024];
    str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len,
                        "SELECT rowid FROM %s WHERE ", table);

    for (int i = 0; i < num_fks; i++) {
        if (i > 0)
            str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, " AND ");
        str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "%s = '%s'",
                            fks[i]->to, fks_values[i]);
    }

    LOG_TRACE("Built query: %s", query_str);

    int rc = sqlite3_prepare_v2(db, (const char*)query_str, -1, &pstmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_SQLITE_ERROR(db);
        sqlite3_finalize(pstmt);
        return STATUS_DB_ERROR;
    }

    if (sqlite3_step(pstmt) != SQLITE_ROW) {
        LOG_ERROR("No matching row found for FK lookup in table '%s'", table);
        sqlite3_finalize(pstmt);
        return STATUS_DB_ERROR;
    }

    *rowid = sqlite3_column_int(pstmt, 0);
    LOG_DEBUG("Found rowid=%d for FK lookup in table '%s'", *rowid, table);

    sqlite3_finalize(pstmt);
    return STATUS_OK;
}
