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
    status_t status = STATUS_OK;

    // Rebuild the path from toks
    char path[MAX_SIZE];
    snprintf(path, MAX_SIZE, "/%s/%s/%s", toks->table, toks->record, toks->attribute);

    LOG_TRACE("Getting attribute bytes: path='%s', block_offset=%ld", path,
              block_offset);

    TRY_NOT_NULL(*key = arena_calloc(arena, 1, sizeof(CacheKey)), key_alloc_error, STATUS_ALLOC_ERROR, "Failed to allocate CacheKey for path '%s'", path);

    strncpy((*key)->query, path, strlen(path) + 1);
    (*key)->offset = block_offset;

key_alloc_error:
    return status;
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
    ensure_arena_init();

    status_t status = STATUS_OK;

    LOG_DEBUG("Initializing database schema...");

    db_schema->tables_head = NULL;

    sqlite3_stmt* pstmt = qm_get_static_query_statement(QUERY_SELECT_TABLES_NAME);
    while (sqlite3_step(pstmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(pstmt, 0);

        Schema* new_schema;
        TRY_NOT_NULL(new_schema = arena_calloc(arena, 1, sizeof(Schema)), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate Schema for table '%s'", name);

        TRY_NOT_NULL(new_schema->name = arena_strdup(arena, name), cleanup, STATUS_ALLOC_ERROR, "Failed to duplicate table name '%s' for schema", name);
        add_schema(db_schema, new_schema);

        LOG_TRACE("Found table: %s", name);
    }

    LOG_INFO("Database schema initialized: %d tables found.", count_schemas(db_schema));

cleanup:
    return status;
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
    ensure_arena_init();
    
    status_t status = STATUS_OK;

    LOG_DEBUG("Initializing schema for table: %s", schema->name);

    // This query gets: column_name, is_pk, fk_table, fk_column_name
    sqlite3_stmt* stmt;
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_INFO, schema->name, schema->name),
                cleanup, STATUS_DB_ERROR, "Failed to build query statement for table info: '%s'", schema->name);

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
            Pk* pk;
            TRY_NOT_NULL(pk = arena_alloc(arena, sizeof(Pk)), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate PK for column '%s' in table '%s'", column_name, schema->name);

            TRY_NOT_NULL(pk->name = arena_strdup(arena, column_name), cleanup, STATUS_ALLOC_ERROR, "Failed to duplicate column name '%s' for PK in table '%s'", column_name, schema->name);

            pk->sqlite_type = parse_sqlite_type(column_type_str);
            add_pk_to_schema(schema, pk);
            
            LOG_TRACE("  PK: %s", column_name);
            pk_count++;
        }
        // Check if foreign key
        else if (fk_table != NULL) {
            // Add fk to schema
            Fk* fk;
            TRY_NOT_NULL(fk = arena_alloc(arena, sizeof(Fk)), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate FK for column '%s' in table '%s'", column_name, schema->name);

            TRY_NOT_NULL(fk->from = arena_strdup(arena, column_name), cleanup, STATUS_ALLOC_ERROR, "Failed to duplicate column name '%s' for FK in table '%s'", column_name, schema->name);
            TRY_NOT_NULL(fk->table = arena_strdup(arena, fk_table), cleanup, STATUS_ALLOC_ERROR, "Failed to duplicate FK table name '%s' for FK in table '%s'", fk_table, schema->name);
            TRY_NOT_NULL(fk->to = arena_strdup(arena, fk_column_name), cleanup, STATUS_ALLOC_ERROR,  "Failed to duplicate FK column name '%s' for FK in table '%s'", fk_column_name, schema->name);

            fk->sqlite_type = parse_sqlite_type(column_type_str);
            add_fk_to_schema(schema, fk);

            LOG_TRACE("  FK: %s -> %s(%s)", column_name, fk_table, fk_column_name);
            fk_count++;    
        }
        // Normal attribute
        else {
            // Add to schema attr field
            Attr* attr;
            TRY_NOT_NULL(attr = arena_alloc(arena, sizeof(Attr)), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate Attr for column '%s' in table '%s'", column_name, schema->name);

            TRY_NOT_NULL(attr->name = arena_strdup(arena, column_name), cleanup, STATUS_ALLOC_ERROR, "Failed to duplicate column name '%s' for Attr in table '%s'", column_name, schema->name);
            
            attr->sqlite_type = parse_sqlite_type(column_type_str);
            add_attribute_to_schema(schema, attr);

            LOG_TRACE("  Attr: %s", column_name);
            attr_count++;
        }
    }

    LOG_DEBUG("Schema '%s' initialized: %d PKs, %d FKs, %d attrs", schema->name, pk_count, fk_count,
              attr_count);

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    return status;
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
    ensure_arena_init();

    LOG_TRACE("Getting attribute size: %s/%s/%s", toks->table, toks->record, toks->attribute);
    
    status_t status = STATUS_OK;
    sqlite3_stmt* stmt;

    // Build the statement
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE, toks->attribute, toks->table),
                cleanup, STATUS_DB_ERROR, "Failed to build query statement for attribute size: '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    // Bind the record value to the query
    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for attribute size query: '%s/%s/%s'", toks->table, toks->record, toks->attribute);
    
    // If there's no record matching the query (should not be possible)
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute attribute size query for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    // Calculate the bytes of the attribute
    *size = sqlite3_column_bytes(stmt, 0);
    LOG_TRACE("Attribute size: %zu bytes", *size);
    if (*size < 0) {
        LOG_SQLITE_ERROR(db);
        status = STATUS_DB_ERROR;
        goto cleanup;
    }

    sqlite3_finalize(stmt);

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    return status;
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
    ensure_arena_init();

    LOG_TRACE("Getting attribute chunk bytes: %s/%s/%s, offset=%ld", toks->table, toks->record,
              toks->attribute, offset);

    status_t status = STATUS_OK;
    CacheKey* key = NULL;
    CacheBlock* blk = NULL;
    sqlite3_stmt* stmt = NULL;
    char* data = NULL;
    size_t data_size = 0;
    size_t relative_offset = 0;

    if (cache_enabled) {
        TRY(get_cache_key_from_toks(toks, BLOCK_OFFSET(offset), &key), cleanup, "Failed to get cache key from tokens for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        // CACHE HIT
        if ((blk = cache_get(key)) != NULL) {
            LOG_DEBUG("Cache hit for '%s' at offset %ld", key->query, key->offset);
            *bytes = (char*)blk->data;
            return status;
        }

        // CACHE MISS
        LOG_DEBUG("Cache miss for '%s' at offset %ld, fetching from DB", key->query, key->offset);
    }

    // Build the statement
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_CHUNK_ATTRIBUTE, toks->attribute,
                                         BLOCK_OFFSET(offset), BLOCK_SIZE, toks->table), cleanup,
                STATUS_DB_ERROR, "Failed to build query statement for attribute chunk bytes: '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for chunk query: '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute chunk query for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    TRY_NOT_NULL(data = arena_strdup(arena, (char*)sqlite3_column_text(stmt, 0)), cleanup, STATUS_ALLOC_ERROR, "Failed to retrieve attribute chunk data for '%s/%s/%s'", toks->table, toks->record, toks->attribute);
    data_size = (size_t)sqlite3_column_bytes(stmt, 0);

    // Calculate relative offset
    relative_offset = offset - BLOCK_OFFSET(offset);
    *bytes                 = data + relative_offset;

    if (cache_enabled) {
        TRY_NOT_NULL(blk = arena_alloc(arena, sizeof(CacheBlock)), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate CacheBlock for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        blk->key         = *key;
        blk->data        = data;
        blk->actual_size = data_size;

        cache_add_block(blk);
        LOG_DEBUG("Fetched %zu bytes from DB and cached", blk->actual_size);

        cache_view();
    }

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    return status;
}

status_t get_attribute_all_bytes(struct tokens* toks, char** bytes, size_t* size) {
    ensure_arena_init();
    
    status_t status = STATUS_OK;
    sqlite3_stmt* stmt = NULL;
    
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE, toks->attribute, toks->table), cleanup,
                STATUS_DB_ERROR, "Failed to build query statement for getting all attribute bytes: '%s/%s/%s'", toks->table, toks->record, toks->attribute);
    
    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for getting all attribute bytes: '%s/%s/%s'", toks->table, toks->record, toks->attribute);
    
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute query for getting all attribute bytes for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    TRY_NOT_NULL(*bytes = arena_strdup(arena, (char*)sqlite3_column_text(stmt, 0)), cleanup, STATUS_ALLOC_ERROR, "Failed to retrieve attribute bytes for '%s/%s/%s'", toks->table, toks->record, toks->attribute);
    *size = (size_t) sqlite3_column_bytes(stmt, 0);

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    return status;
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
    ensure_arena_init();
    status_t status = STATUS_OK;

    LOG_TRACE("Getting attribute type: %s/%s/%s", toks->table, toks->record, toks->attribute);

    Schema* table_schema = NULL;
    TRY_NOT_NULL(table_schema = find_schema_by_name(db_schema, toks->table), cleanup, STATUS_DB_ERROR, "Table '%s' not found in schema", toks->table);

    void* attribute;
    
    attribute = find_fk_by_name(table_schema, toks->attribute);
    if (attribute) {
        Fk* attr = (Fk*) attribute;
        *type = attr->sqlite_type;
        return STATUS_OK;
    }

    attribute = find_pk_by_name(table_schema, toks->attribute);
    if (attribute) {
        Pk* attr = (Pk*) attribute;
        *type = attr->sqlite_type;
        return STATUS_OK;
    }

    attribute = find_attribute_by_name(table_schema, toks->attribute);
    if (attribute) {
        Attr* attr = (Attr*) attribute;
        *type = attr->sqlite_type;
        LOG_TRACE("Attribute type for '%s/%s/%s': %d", toks->table, toks->record, toks->attribute,
                *type);
        return STATUS_OK;
    }

    status = STATUS_DB_ERROR;

cleanup:
    return status;
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
    ensure_arena_init();
    status_t status = STATUS_OK;

    sqlite3_stmt* stmt = NULL;
    sqlite3_blob* blob_handle = NULL;

    LOG_DEBUG("Updating attribute: %s/%s/%s (size=%zu)", toks->table, toks->record, toks->attribute,
              size);
    
    // Check type
    int type;
    TRY(get_attribute_type(toks, &type), cleanup, "Failed to get attribute type for update of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    if (type == SQLITE_TEXT || type == SQLITE_BLOB) {
        // Get current attribute size
        size_t current_size;
        TRY(get_attribute_size(toks, &current_size), cleanup, "Failed to get attribute size for update of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        size_t end_of_write = offset + size;

        // If we need to expand, extend the blob in-place using SQL concatenation
        // with zeroblob --> no full copy into RAM
        if (end_of_write > current_size) {
            size_t expand_by = end_of_write - current_size;
            LOG_DEBUG("Expanding blob by %zu bytes (current=%zu, needed=%zu)", expand_by, current_size, end_of_write);

            TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ZERO_BLOB, toks->table, toks->attribute), cleanup, STATUS_DB_ERROR, "Failed to build query statement for blob expansion of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

            TRY_SQLITE(sqlite3_bind_int(stmt, 1, (int)expand_by), SQLITE_OK, cleanup, "Failed to bind expand size for blob expansion of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

            TRY_SQLITE(sqlite3_bind_text(stmt, 2, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup, "Failed to bind record value for blob expansion of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

            TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup, "Failed to execute blob expansion for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

            sqlite3_finalize(stmt);
            LOG_DEBUG("Blob expanded successfully to %zu bytes", end_of_write);
        }

        // Now write the data in-place using the Blob I/O API
        TRY_SQLITE(sqlite3_blob_open(db, "main", toks->table, toks->attribute, atoll(toks->record), 1, &blob_handle), SQLITE_OK, cleanup, "Failed to open blob handle for '%s/%s/%s': %s", toks->table, toks->record, toks->attribute, sqlite3_errmsg(db));

        TRY_SQLITE(sqlite3_blob_write(blob_handle, buffer, (int)size, (int)offset), SQLITE_OK, cleanup, "Failed to write to blob: %s", sqlite3_errmsg(db));

        LOG_DEBUG("Blob updated in-place via Blob I/O (offset=%ld, size=%zu)", offset, size);
        sqlite3_blob_close(blob_handle);
    } else {
        // Read all the attribute bytes
        char* bytes;
        size_t bytes_size;
        TRY(get_attribute_all_bytes(toks, &bytes, &bytes_size), cleanup, "Failed to get all attribute bytes for update of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        // Calculate new size after update
        size_t new_bytes_size = (offset + size > bytes_size) ? offset + size : bytes_size;
        LOG_TRACE("Current attribute, new data size after update: %zu bytes", new_bytes_size);

        // Allocate new buffer for the updated attribute value
        char* new_bytes;
        TRY_NOT_NULL(new_bytes = arena_alloc(arena, new_bytes_size), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate buffer for updated attribute value for '%s/%s/%s'", toks->table, toks->record, toks->attribute);
        
        // Copy existing bytes and patch with new data
        memcpy(new_bytes, bytes, bytes_size);
        memcpy(new_bytes + offset, buffer, size);

        // Write the new a;ttribute bytes
        TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE, toks->table, toks->attribute), cleanup, STATUS_DB_ERROR, "Failed to build query statement for attribute update of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        // value based on its SQLite type
        TRY(bind_attribute_value(stmt, new_bytes, type), cleanup, "Failed to bind new attribute value for update of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        // Bind the record ID for the WHERE clause
        TRY_SQLITE(sqlite3_bind_int64(stmt, 2, atoi(toks->record)), SQLITE_OK, cleanup, "Failed to bind record value for attribute update of '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        // Execute the UPDATE statement
        TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup, "Failed to execute UPDATE query for attribute '%s/%s/%s'", toks->table, toks->record, toks->attribute);

        int changes = sqlite3_changes(db);
        LOG_DEBUG("Attribute updated successfully, %d rows affected", changes);
        sqlite3_finalize(stmt);
    }

    if (cache_enabled) {
        // Remove ALL blocks with key that has path = rebuilt path
        CacheKey* key = NULL;
        TRY(get_cache_key_from_toks(toks, 0, &key), cleanup, "Failed to get cache key from tokens for eviction of '%s/%s/%s'", toks->table, toks->record, toks->attribute);
        
        LOG_TRACE("Evicting cache blocks for updated attribute: path='%s'", key->query);
        free(key);
    }

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    if (blob_handle) sqlite3_blob_close(blob_handle);

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
    ensure_arena_init();
    status_t status = STATUS_OK;

    LOG_DEBUG("Setting attribute to NULL: %s/%s/%s", toks->table, toks->record, toks->attribute);

    sqlite3_stmt* stmt = NULL;
    
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE, toks->table, toks->attribute), cleanup, STATUS_DB_ERROR, "Failed to build query statement for setting attribute to NULL for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    // NOTE: it should NOT nullify the field, because it can have a 'NOT NULL' constraint
    TRY_SQLITE(sqlite3_bind_null(stmt, 1), SQLITE_OK, cleanup, "Failed to bind NULL value for setting attribute to NULL for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    TRY_SQLITE(sqlite3_bind_text(stmt, 2, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup, "Failed to bind record value for setting attribute to NULL for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup, "Failed to execute UPDATE query for setting attribute to NULL for '%s/%s/%s'", toks->table, toks->record, toks->attribute);

    int changes = sqlite3_changes(db);
    LOG_DEBUG("Attribute set to NULL successfully, %d rows affected", changes);
    sqlite3_finalize(stmt);

    if (cache_enabled) {
        // Evict the corresponding cache blocks, if exists
        CacheKey* key = NULL;
        TRY(get_cache_key_from_toks(toks, 0, &key), cleanup, "Failed to get cache key from tokens for eviction of '%s/%s/%s'", toks->table, toks->record, toks->attribute);
        LOG_TRACE("Evicting cache blocks for NULLified attribute: path='%s'", key->query);
        cache_evict_blocks_by_path(key->query);
    }

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    return status;
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
    ensure_arena_init();
    status_t status = STATUS_OK;

    LOG_TRACE("Getting row IDs for table: %s", table);

    CacheKey* key = NULL;
    if (cache_enabled) {
        struct tokens toks = {
            .table = table,
            .attribute = "",
            .record = ""
        };

        // VERY BIG ASSUMPTION ON BLOCK_OFFSET=0
        TRY(get_cache_key_from_toks(&toks, 0, &key), cleanup, "Failed to get cache key from tokens for rowids of table '%s'", table);

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

    sqlite3_stmt* stmt;
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_ROWIDS, table), cleanup, STATUS_DB_ERROR, "Failed to build query statement for selecting rowids of table '%s'", table);

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
        CacheBlock* blk;
        TRY_NOT_NULL(blk = arena_alloc(arena, sizeof(CacheBlock)), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate CacheBlock for rowids of table '%s'", table);

        char** records_copy;

        TRY_NOT_NULL(records_copy = arena_alloc(arena, record_count * sizeof(char*)), cleanup, STATUS_ALLOC_ERROR, "Failed to allocate records copy for cache of table '%s'", table);

        if (!records_copy) {
            LOG_WARN("Failed to allocate records copy for cache");
            free(blk);
            free(key);
            sqlite3_finalize(stmt);
            return STATUS_OK;
        }

        for (int i = 0; i < record_count; i++) {
            TRY_NOT_NULL(records_copy[i] = arena_strdup(arena, records[i]), cleanup, STATUS_ALLOC_ERROR, "Failed to duplicate record '%s' for cache of table '%s'", records[i], table);
        }

        blk->key         = *key;
        blk->data        = records_copy;
        blk->actual_size = record_count;

        cache_add_block(blk);
        cache_view();
    }

cleanup:
    if (stmt) sqlite3_finalize(stmt);
    return status;
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
    ensure_arena_init();
    status_t status = STATUS_OK;

    LOG_TRACE("Getting rowid from PKs for table: %s (num_fks=%d)", table, num_fks);

    // FIX: this function should be implemented better using the query manager prepared statements
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

    TRY_SQLITE(sqlite3_prepare_v2(db, query_str, -1, &pstmt, NULL), SQLITE_OK, cleanup, "Failed to prepare statement for getting rowid from PKs for table '%s'", table);
    
    TRY_SQLITE(sqlite3_step(pstmt), SQLITE_ROW, cleanup, "Failed to step through prepared statement for getting rowid from PKs for table '%s'", table);

    *rowid = sqlite3_column_int(pstmt, 0);
    LOG_DEBUG("Found rowid=%d for FK lookup in table '%s'", *rowid, table);

cleanup:
    sqlite3_finalize(pstmt);
    return STATUS_OK;
}
