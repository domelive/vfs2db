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

// Portability macro to calculate the block offset for a given file offset, ensuring that it is
// aligned to the block size used in the VFS2DB filesystem.
#define BLOCK_OFFSET(offset) ((off_t)((offset / BLOCK_SIZE) * BLOCK_SIZE))

static __thread Arena* arena = NULL; /**< Thread-local memory arena for efficient allocations */

/**
 * Ensure Arena Init
 *
 * @brief Retrieves the thread-local memory arena, creating it if it doesn't exist.
 *
 * @return Pointer to the thread-local Arena
 */
static inline void ensure_arena_init() {
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
 * Parse SQLite Type
 *
 * @brief Parses a SQLite column type string and determines the corresponding SQLite type affinity.
 *
 * SQLite uses type affinity to determine how to store and compare values in a column, based on the
 * declared type of the column. This function analyzes the type string and returns the appropriate
 * SQLite type affinity constant (e.g., SQLITE_INTEGER, SQLITE_TEXT, SQLITE_BLOB, SQLITE_FLOAT)
 * based on the presence of certain keywords in the type string.
 *
 * @return The SQLite type affinity constant corresponding to the given type string
 */
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

/**
 * Get Cache Key from Tokens
 *
 * @brief Constructs a CacheKey structure from the given path tokens and block offset. The CacheKey
 * is used to identify a specific block of data in the cache based on the file path and offset.
 *
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute information
 * @param[in] block_offset The block-aligned offset for which to create the cache key
 * @param[out] key Pointer to a CacheKey pointer where the resulting CacheKey will be stored
 *
 * @return STATUS_OK on success, STATUS_ALLOC_ERROR on memory allocation failure
 */
static inline status_t get_cache_key_from_toks(struct tokens* toks, off_t block_offset,
                                               CacheKey** key) {
    status_t status = STATUS_OK;

    // Construct the file path for the cache key based on the table, record, and attribute
    // information from the tokens.
    char path[MAX_SIZE];

    if (toks->table && toks->record && toks->attribute) {
        snprintf(path, MAX_SIZE, "/%s/%s/%s", toks->table, toks->record, toks->attribute);
    } else if (toks->table && toks->record) {
        snprintf(path, MAX_SIZE, "/%s//", toks->table, toks->record);
    } else {
        LOG_ERROR("Invalid tokens for cache key: table='%s', record='%s', attribute='%s'",
                  toks->table, toks->record, toks->attribute);
        return STATUS_DB_ERROR;
    }

    LOG_TRACE("Getting attribute bytes: path='%s', block_offset=%ld", path, block_offset);

    // Allocate a new CacheKey structure to store the cache key information for the specified path
    // and block offset. The CacheKey will be used to identify the corresponding block of data in
    // the cache for read and write operations on the attribute file in the VFS2DB filesystem.
    TRY_NOT_NULL(*key = calloc(1, sizeof(CacheKey)), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate CacheKey for path '%s'", path);

    strncpy((*key)->query, path, strlen(path) + 1);
    (*key)->offset = block_offset;

cleanup:
    return status;
}

status_t init_db_schema(DbSchema* db_schema) {
    status_t status = STATUS_OK;

    LOG_DEBUG("Initializing database schema...");

    db_schema->tables_head = NULL;

    // Execute the SQL query to retrieve the names of all tables in the database.
    sqlite3_stmt* pstmt = qm_get_static_query_statement(QUERY_SELECT_TABLES_NAME);

    // Iterate over the query results and populate the DbSchema structure with the table names.
    while (sqlite3_step(pstmt) == SQLITE_ROW) {
        // Get the table name from the query result.
        const char* name = (const char*)sqlite3_column_text(pstmt, 0);

        Schema* new_schema;

        // Allocate a new Schema structure for the table.
        TRY_NOT_NULL(new_schema = calloc(1, sizeof(Schema)), cleanup, STATUS_ALLOC_ERROR,
                     "Failed to allocate Schema for table '%s'", name);

        // Allocate and copy the table name into the Schema structure.
        TRY_NOT_NULL(new_schema->name = strdup(name), cleanup, STATUS_ALLOC_ERROR,
                     "Failed to duplicate table name '%s' for schema", name);

        // Add the new Schema to the DbSchema's hash map of tables, using the table name as the key.
        add_schema(db_schema, new_schema);

        LOG_TRACE("Found table: %s", name);
    }

    LOG_INFO("Database schema initialized: %d tables found.", count_schemas(db_schema));

cleanup:
    return status;
}

status_t init_schema(Schema* schema) {
    status_t status = STATUS_OK;

    LOG_DEBUG("Initializing schema for table: %s", schema->name);

    sqlite3_stmt* stmt;

    // Build the dynamic query statement to retrieve column information for the specified table
    // using the QUERY_TPL_SELECT_TABLE_INFO template.
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_INFO,
                                                         schema->name, schema->name),
                 cleanup, STATUS_DB_ERROR, "Failed to build query statement for table info: '%s'",
                 schema->name);

    int pk_count = 0, fk_count = 0, attr_count = 0;

    // Iterate over the query results to populate the Schema structure with information about the
    // table's columns, primary keys, and foreign keys.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Extract column information from the query result.
        const char* column_name     = sqlite3_column_text(stmt, 0);
        const char* column_type_str = sqlite3_column_text(stmt, 1);
        const bool  is_pk           = sqlite3_column_int(stmt, 2);
        const char* fk_table        = sqlite3_column_text(stmt, 3);
        const char* fk_column_name  = sqlite3_column_text(stmt, 4);

        LOG_TRACE("Column: %s, Type: %s, PK: %d, FK Table: %s, FK Column: %s", column_name,
                  column_type_str, is_pk, fk_table ? fk_table : "NULL",
                  fk_column_name ? fk_column_name : "NULL");

        // Check if primary key
        if (is_pk) {
            LOG_TRACE("Column '%s' is a primary key", column_name);

            // Add pk to schema
            Pk* pk;

            // Allocate a new Pk structure for the primary key column and populate it with the
            // column name and SQLite type affinity parsed from the column type string.
            TRY_NOT_NULL(pk = malloc(sizeof(Pk)), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to allocate PK for column '%s' in table '%s'", column_name,
                         schema->name);

            // Duplicate the column name for the primary key and store it in the Pk structure.
            TRY_NOT_NULL(pk->name = strdup(column_name), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to duplicate column name '%s' for PK in table '%s'", column_name,
                         schema->name);

            // Parse the SQLite type affinity for the primary key column based on its declared type
            // and store it in the Pk structure.
            pk->sqlite_type = parse_sqlite_type(column_type_str);
            LOG_TRACE("Parsed SQLite type for PK '%s': %d", column_name, pk->sqlite_type);

            // Add the primary key to the Schema's hash map of primary keys.
            add_pk_to_schema(schema, pk);

            LOG_TRACE("  PK: %s", column_name);

            // Increment the primary key count for logging purposes.
            pk_count++;
        }
        // Check if foreign key
        else if (fk_table != NULL) {
            LOG_TRACE("Column '%s' is a foreign key referencing '%s(%s)'", column_name, fk_table,
                      fk_column_name);
            // Add fk to schema
            Fk* fk;

            // Allocate a new Fk structure for the foreign key column.
            TRY_NOT_NULL(fk = malloc(sizeof(Fk)), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to allocate FK for column '%s' in table '%s'", column_name,
                         schema->name);

            // Duplicate the column from name for the foreign key and store it in the Fk structure.
            TRY_NOT_NULL(fk->from = strdup(column_name), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to duplicate column name '%s' for FK in table '%s'", column_name,
                         schema->name);

            // Duplicate the target table name for the foreign key and store it in the Fk structure.
            TRY_NOT_NULL(fk->table = strdup(fk_table), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to duplicate FK table name '%s' for FK in table '%s'", fk_table,
                         schema->name);

            // Duplicate the target column name for the foreign key and store it in the Fk
            // structure.
            TRY_NOT_NULL(fk->to = strdup(fk_column_name), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to duplicate FK column name '%s' for FK in table '%s'",
                         fk_column_name, schema->name);

            // Parse the SQLite type affinity for the foreign key column based on its declared type
            // and store it in the Fk structure.
            fk->sqlite_type = parse_sqlite_type(column_type_str);

            // Add the foreign key to the Schema's hash map of foreign keys.
            add_fk_to_schema(schema, fk);

            LOG_TRACE("  FK: %s -> %s(%s)", column_name, fk_table, fk_column_name);

            // Increment the foreign key count for logging purposes.
            fk_count++;
        }
        // Normal attribute
        else {
            LOG_TRACE("Column '%s' is a normal attribute", column_name);

            // Add to schema attr field
            Attr* attr;

            // Allocate a new Attr structure for the normal attribute column.
            TRY_NOT_NULL(attr = malloc(sizeof(Attr)), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to allocate Attr for column '%s' in table '%s'", column_name,
                         schema->name);

            // Duplicate the column name for the attribute and store it in the Attr structure.
            TRY_NOT_NULL(attr->name = strdup(column_name), cleanup, STATUS_ALLOC_ERROR,
                         "Failed to duplicate column name '%s' for Attr in table '%s'", column_name,
                         schema->name);

            // Parse the SQLite type affinity for the normal attribute column based on its declared
            // type and store it in the Attr structure.
            attr->sqlite_type = parse_sqlite_type(column_type_str);

            // Add the normal attribute to the Schema's hash map of attributes.
            add_attribute_to_schema(schema, attr);

            LOG_TRACE("  Attr: %s", column_name);

            // Increment the attribute count for logging purposes.
            attr_count++;
        }
    }

    LOG_DEBUG("Schema '%s' initialized: %d PKs, %d FKs, %d attrs", schema->name, pk_count, fk_count,
              attr_count);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t record_exists(struct tokens* toks) {
    LOG_TRACE("Checking if record exists: %s/%s", toks->table, toks->record);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt;

    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ROWID, toks->table),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for record existence check: '%s/%s'", toks->table,
                 toks->record);

    // Bind the record value to the query
    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for existence check query: '%s/%s'", toks->table,
               toks->record);

    // Execute the query and check if a row is returned, which indicates that the record exists.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute record existence check query for '%s/%s'", toks->table,
               toks->record);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

static inline status_t get_attribute_all_bytes(struct tokens* toks, char** bytes, size_t* size) {
    ensure_arena_init();

    LOG_TRACE("Getting all attribute bytes: %s/%s/%s", toks->table, toks->record, toks->attribute);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt;

    // Build the statement
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE,
                                                         toks->attribute, toks->table),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for getting all attribute bytes: '%s/%s/%s'",
                 toks->table, toks->record, toks->attribute);

    // Bind the record value to the query
    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for getting all attribute bytes: '%s/%s/%s'",
               toks->table, toks->record, toks->attribute);

    // Execute the query and check if a row is returned
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute query for getting all attribute bytes for '%s/%s/%s'",
               toks->table, toks->record, toks->attribute);

    // Retrieve the attribute data from the query result and calculate its size in bytes.
    TRY_NOT_NULL(*bytes = (char*)sqlite3_column_blob(stmt, 0), cleanup, STATUS_ISNULL,
                 "Failed to retrieve attribute data for '%s/%s/%s'", toks->table, toks->record,
                 toks->attribute);

    // Arena_strdup the bytes to ensure they are stored in the thread-local arena for efficient
    // memory management.
    char* blob_data;
    *size = (size_t)sqlite3_column_bytes(stmt, 0);

    TRY_NOT_NULL(blob_data = arena_alloc(arena, *size + 1), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to duplicate attribute data for '%s/%s/%s'", toks->table, toks->record,
                 toks->attribute);

    memcpy(blob_data, *bytes, *size);
    blob_data[*size] = '\0'; // Null-terminate the data for safe string operations
    *bytes           = blob_data;

    LOG_TRACE("Data retrieved of size %ld", *size);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t get_attribute_size(struct tokens* toks, size_t* size) {
    ensure_arena_init();

    LOG_TRACE("Getting attribute size: %s/%s/%s", toks->table, toks->record, toks->attribute);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt;

    // Build the statement
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE_SIZE,
                                                         toks->attribute, toks->table),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for attribute size: '%s/%s/%s'", toks->table,
                 toks->record, toks->attribute);

    // Bind the record value to the query
    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for attribute size query: '%s/%s/%s'", toks->table,
               toks->record, toks->attribute);

    // Execute the query and check if a row is returned
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute attribute size query for '%s/%s/%s'", toks->table, toks->record,
               toks->attribute);

    // Calculate the bytes of the attribute
    *size = sqlite3_column_int(stmt, 0);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t get_attribute_chunk_bytes(struct tokens* toks, off_t offset, char** bytes) {
    ensure_arena_init();

    LOG_TRACE("Getting attribute chunk bytes: %s/%s/%s, offset=%ld", toks->table, toks->record,
              toks->attribute, offset);

    status_t      status          = STATUS_OK;
    CacheKey*     key             = NULL;
    CacheBlock*   blk             = NULL;
    sqlite3_stmt* stmt            = NULL;
    char*         data            = NULL;
    size_t        data_size       = 0;
    size_t        relative_offset = 0;

    if (cache_enabled) {
        LOG_TRACE("Cache enabled, checking for cache hit...");
        cache_view();
        // Attempt to construct a cache key for the specified attribute and block offset to check
        // for a cache hit before querying the database.
        TRY(get_cache_key_from_toks(toks, BLOCK_OFFSET(offset), &key), cleanup,
            "Failed to get cache key from tokens for '%s/%s/%s'", toks->table, toks->record,
            toks->attribute);

        // Cache HIT
        if ((blk = cache_get(key)) != NULL) {
            LOG_DEBUG("Cache hit for '%s' at offset %ld", key->query, key->offset);

            // NOTE: we could get rid of this strdup, by just assigning `(char*) blk->data` to
            // `*bytes`... we strdup because of possible threads evicting the block...
            *bytes = arena_calloc(arena, 1, blk->actual_size + 1);
            if (!*bytes) {
                LOG_ERROR("Failed to allocate memory for cached attribute chunk");
                return STATUS_ALLOC_ERROR;
            }
            memcpy(*bytes, blk->data, blk->actual_size);
            relative_offset = offset - BLOCK_OFFSET(offset);
            *bytes += relative_offset;

            return status;
        }

        // Cache MISS
        LOG_DEBUG("Cache miss for '%s' at offset %ld, fetching from DB", key->query, key->offset);
    }

    // Build the statement
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_CHUNK_ATTRIBUTE,
                                                         toks->attribute, BLOCK_OFFSET(offset),
                                                         BLOCK_SIZE, toks->table),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for attribute chunk bytes: '%s/%s/%s'",
                 toks->table, toks->record, toks->attribute);

    LOG_TRACE("Dynamic query built");

    // Bind the record value to the query
    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for chunk query: '%s/%s/%s'", toks->table, toks->record,
               toks->attribute);

    LOG_TRACE("Dynamic query binded");

    // Execute the query and check if a row is returned
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute chunk query for '%s/%s/%s'", toks->table, toks->record,
               toks->attribute);

    LOG_TRACE("Dynamic query executed");

    // Retrieve the attribute chunk data from the query result and calculate its size in bytes.
    TRY_NOT_NULL(data = (char*)sqlite3_column_blob(stmt, 0), cleanup, STATUS_DB_ERROR,
                 "Failed to retrieve attribute chunk data for '%s/%s/%s'", toks->table,
                 toks->record, toks->attribute);

    data_size = (size_t)sqlite3_column_bytes(stmt, 0);

    LOG_TRACE("Data: %s", data);

    LOG_TRACE("Data retrieved of size %ld", data_size);

    TRY_NOT_NULL(*bytes = arena_calloc(arena, 1, data_size + 1), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to duplicate attribute chunk data for '%s/%s/%s'", toks->table,
                 toks->record, toks->attribute);

    memcpy(*bytes, data, data_size);

    LOG_TRACE("Data copied to bytes: %s", *bytes);

    // Calculate the relative offset within the block for the requested attribute chunk and set the
    // output bytes pointer to the correct position within the retrieved data. This allows for
    // correct retrieval of the attribute data even when the requested offset is not aligned.
    relative_offset = offset - BLOCK_OFFSET(offset);
    *bytes += relative_offset;

    if (cache_enabled) {
        LOG_TRACE("Inserting new cache block");

        // Create a new CacheBlock for the retrieved attribute chunk data.
        TRY_NOT_NULL(blk = calloc(1, sizeof(CacheBlock)), cleanup, STATUS_ALLOC_ERROR,
                     "Failed to allocate CacheBlock for '%s/%s/%s'", toks->table, toks->record,
                     toks->attribute);

        // Populate the CacheBlock with the cache key, retrieved data, and actual size of the data.
        blk->key  = *key;
        blk->data = calloc(1, data_size);
        if (!blk->data) {
            LOG_ERROR("Failed to allocate memory for cache block data");
            return STATUS_ALLOC_ERROR;
        }
        memcpy(blk->data, data, data_size);
        blk->actual_size = data_size;

        // Add the CacheBlock to the cache to store the retrieved attribute chunk for future access.
        cache_add_block(blk);
        LOG_DEBUG("Fetched %zu bytes from DB and cached", blk->actual_size);

        key = NULL; // Ownership transferred to cache block, avoid double free

        cache_view();
    }

cleanup:
    if (key)
        free(key);
    if (stmt)
        sqlite3_finalize(stmt);

    return status;
}

status_t is_attribute_null(struct tokens* toks, bool* is_null) {
    ensure_arena_init();

    LOG_TRACE("Getting all attribute bytes: %s/%s/%s", toks->table, toks->record, toks->attribute);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    // Build the statement to check if the specified attribute value is NULL for the given record in
    // the specified table, using the QUERY_TPL_SELECT_ATTRIBUTE_IS_NULL template.
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_ATTRIBUTE_IS_NULL,
                                                         toks->attribute, toks->table),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for checking if attribute is NULL: '%s/%s/%s'",
                 toks->table, toks->record, toks->attribute);

    // Bind the record value to the query to specify which record's attribute value to retrieve.
    TRY_SQLITE(sqlite3_bind_text(stmt, 1, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for getting all attribute bytes: '%s/%s/%s'",
               toks->table, toks->record, toks->attribute);

    // Execute the query and check if a row is returned, indicating that the attribute value was
    // successfully retrieved.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_ROW, cleanup,
               "Failed to execute query for checking if attribute is NULL for '%s/%s/%s'",
               toks->table, toks->record, toks->attribute);

    *is_null = sqlite3_column_int(stmt, 0);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t get_attribute_type(struct tokens* toks, int* type) {
    LOG_TRACE("Getting attribute type: %s/%s/%s", toks->table, toks->record, toks->attribute);

    status_t status       = STATUS_OK;
    Schema*  table_schema = NULL;

    // Find the schema for the specified table in the database schema.
    TRY_NOT_NULL(table_schema = find_schema_by_name(db_schema, toks->table), cleanup,
                 STATUS_DB_ERROR, "Table '%s' not found in schema", toks->table);

    // Check if the specified attribute is a foreign key, primary key, or normal attribute.
    void* attribute;

    // First, check if it's a foreign key
    attribute = find_fk_by_name(table_schema, toks->attribute);
    if (attribute) {
        Fk* attr = (Fk*)attribute;
        *type    = attr->sqlite_type;
        return STATUS_OK;
    }

    // Check if it's a primary key
    attribute = find_pk_by_name(table_schema, toks->attribute);
    if (attribute) {
        Pk* attr = (Pk*)attribute;
        *type    = attr->sqlite_type;
        return STATUS_OK;
    }

    // Finally, check if it's a normal attribute
    attribute = find_attribute_by_name(table_schema, toks->attribute);
    if (attribute) {
        Attr* attr = (Attr*)attribute;
        *type      = attr->sqlite_type;
        LOG_TRACE("Attribute type for '%s/%s/%s': %d", toks->table, toks->record, toks->attribute,
                  *type);
        return STATUS_OK;
    }

    // If the attribute is not found in the schema, return an error status.
    status = STATUS_DB_ERROR;

cleanup:
    return status;
}

static inline status_t bind_attribute_value(sqlite3_stmt* stmt, char* value, int type) {
    switch (type) {
    // For integer types, convert the string value to a 64-bit integer and bind it to the statement
    // using sqlite3_bind_int64.
    case SQLITE_INTEGER: {
        sqlite3_int64 val = strtoll(value, NULL, 10);
        if (sqlite3_bind_int64(stmt, 1, val) != SQLITE_OK) {
            LOG_SQLITE_ERROR(db);
            return STATUS_DB_ERROR;
        }
        break;
    }
    // For floating-point types, convert the string value to a double and bind it to the statement
    // using sqlite3_bind_double.
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
        // case SQLITE_BLOB:
        // default: {
        //     if (sqlite3_bind_blob(stmt, 1, value, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        //         LOG_SQLITE_ERROR(db);
        //         return STATUS_DB_ERROR;
        //     }
        //     break;
        // }
    }

    return STATUS_OK;
}

status_t update_attribute_value(struct tokens* toks, const char* buffer, size_t size,
                                off_t offset) {
    ensure_arena_init();

    LOG_TRACE("Updating attribute value: %s/%s/%s (size=%zu, offset=%ld)", toks->table,
              toks->record, toks->attribute, size, offset);

    status_t      status      = STATUS_OK;
    sqlite3_stmt* stmt        = NULL;
    sqlite3_blob* blob_handle = NULL;
    int           type;

    // Determine the SQLite data type of the attribute to decide how to perform the update.
    TRY(get_attribute_type(toks, &type), cleanup,
        "Failed to get attribute type for update of '%s/%s/%s'", toks->table, toks->record,
        toks->attribute);

    // If the attribute is of type BLOB, we can use the SQLite Blob I/O API to perform
    // in-place updates without needing to read the entire value into memory. This allows for
    // efficient updates of large BLOB attributes by directly writing to the database file
    // at the correct offset.
    if (type == SQLITE_BLOB) {
        // Get current attribute size
        size_t current_size;
        TRY(get_attribute_size(toks, &current_size), cleanup,
            "Failed to get attribute size for update of '%s/%s/%s'", toks->table, toks->record,
            toks->attribute);

        size_t end_of_write = offset + size;

        // If we need to expand, extend the blob in-place using SQL concatenation
        // with zeroblob --> no full copy into RAM
        if (end_of_write > current_size) {
            size_t expand_by = end_of_write - current_size;
            LOG_DEBUG("Expanding blob by %zu bytes (current=%zu, needed=%zu)", expand_by,
                      current_size, end_of_write);

            TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ZERO_BLOB,
                                                                 toks->table, toks->attribute,
                                                                 toks->attribute),
                         cleanup, STATUS_DB_ERROR,
                         "Failed to build query statement for blob expansion of '%s/%s/%s'",
                         toks->table, toks->record, toks->attribute);

            TRY_SQLITE(sqlite3_bind_zeroblob(stmt, 1, (int)expand_by), SQLITE_OK, cleanup,
                       "Failed to bind expand size for blob expansion of '%s/%s/%s'", toks->table,
                       toks->record, toks->attribute);

            TRY_SQLITE(sqlite3_bind_text(stmt, 2, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK,
                       cleanup, "Failed to bind record value for blob expansion of '%s/%s/%s'",
                       toks->table, toks->record, toks->attribute);

            TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
                       "Failed to execute blob expansion for '%s/%s/%s'", toks->table, toks->record,
                       toks->attribute);

            LOG_DEBUG("Blob expanded successfully to %zu bytes", end_of_write);
        }

        // Now write the data in-place using the Blob I/O API
        TRY_SQLITE(sqlite3_blob_open(db, "main", toks->table, toks->attribute, atoll(toks->record),
                                     1, &blob_handle),
                   SQLITE_OK, cleanup, "Failed to open blob handle for '%s/%s/%s': %s", toks->table,
                   toks->record, toks->attribute, sqlite3_errmsg(db));

        TRY_SQLITE(sqlite3_blob_write(blob_handle, buffer, (int)size, (int)offset), SQLITE_OK,
                   cleanup, "Failed to write to blob: %s", sqlite3_errmsg(db));

        LOG_DEBUG("Blob updated in-place via Blob I/O (offset=%ld, size=%zu)", offset, size);
    }
    // For other data types (e.g., TEXT, INTEGER, FLOAT), we need to read the entire attribute value
    // into memory, apply the update to the in-memory value, and then write the updated value back
    // to the database. This is necessary because non-BLOB types cannot be updated in-place and
    // require a full read-modify-write cycle.
    else {
        // Read all the attribute bytes
        char*    bytes;
        size_t   bytes_size;
        status_t read_status = get_attribute_all_bytes(toks, &bytes, &bytes_size);

        // If the attribute value is NULL, we treat it as an empty string for the purpose of the
        // update. This allows us to apply the update correctly even when the existing value is
        // NULL.
        if (read_status == STATUS_ISNULL) {
            LOG_DEBUG("Attribute value is NULL, treating as empty for update of '%s/%s/%s'",
                      toks->table, toks->record, toks->attribute);
            bytes      = arena_strdup(arena, "");
            bytes_size = 0;
        } else if (read_status != STATUS_OK) {
            LOG_ERROR("Failed to read existing attribute value for update of '%s/%s/%s'",
                      toks->table, toks->record, toks->attribute);
            goto cleanup;
        }

        // Calculate new size after update
        size_t new_bytes_size = (offset + size > bytes_size) ? offset + size : bytes_size;
        LOG_TRACE("Current attribute, new data size after update: %zu bytes", new_bytes_size);

        // Allocate new buffer for the updated attribute value
        char* new_bytes;
        TRY_NOT_NULL(new_bytes = arena_calloc(arena, 1, new_bytes_size + 1), cleanup,
                     STATUS_ALLOC_ERROR,
                     "Failed to allocate buffer for updated attribute value for '%s/%s/%s'",
                     toks->table, toks->record, toks->attribute);

        // Copy existing bytes and patch with new data
        memcpy(new_bytes, bytes, bytes_size);
        memcpy(new_bytes + offset, buffer, size);

        // Write the new attribute bytes
        TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE,
                                                             toks->table, toks->attribute),
                     cleanup, STATUS_DB_ERROR,
                     "Failed to build query statement for attribute update of '%s/%s/%s'",
                     toks->table, toks->record, toks->attribute);

        // value based on its SQLite type
        TRY(bind_attribute_value(stmt, new_bytes, type), cleanup,
            "Failed to bind new attribute value for update of '%s/%s/%s'", toks->table,
            toks->record, toks->attribute);

        // Bind the record ID for the WHERE clause
        TRY_SQLITE(sqlite3_bind_int64(stmt, 2, atoi(toks->record)), SQLITE_OK, cleanup,
                   "Failed to bind record value for attribute update of '%s/%s/%s'", toks->table,
                   toks->record, toks->attribute);

        // Execute the UPDATE statement
        TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
                   "Failed to execute UPDATE query for attribute '%s/%s/%s'", toks->table,
                   toks->record, toks->attribute);

        int changes = sqlite3_changes(db);
        LOG_DEBUG("Attribute updated successfully, %d rows affected", changes);
    }

    if (cache_enabled) {
        // Remove ALL blocks with key that has path = rebuilt path
        CacheKey* key = NULL;
        TRY(get_cache_key_from_toks(toks, 0, &key), cleanup,
            "Failed to get cache key from tokens for eviction of '%s/%s/%s'", toks->table,
            toks->record, toks->attribute);

        cache_evict_blocks_from_toks(toks);

        LOG_TRACE("Evicting cache blocks for updated attribute: path='%s'", key->query);
        free(key);
    }

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    if (blob_handle)
        sqlite3_blob_close(blob_handle);

    return status;
}

status_t set_attribute_empty(struct tokens* toks) {
    ensure_arena_init();

    LOG_TRACE("Setting attribute to empty: %s/%s/%s", toks->table, toks->record, toks->attribute);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    // Build the UPDATE statement to set the specified attribute to empty for the given table and
    // record using the QUERY_TPL_UPDATE_ATTRIBUTE template.
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_UPDATE_ATTRIBUTE,
                                                         toks->table, toks->attribute),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for setting attribute to empty for '%s/%s/%s'",
                 toks->table, toks->record, toks->attribute);

    // NOTE: it should NOT nullify the field, because it can have a 'NOT NULL' constraint
    // Get the attribute type to bind the empty value correctly based on its SQLite type
    int type;
    TRY(get_attribute_type(toks, &type), cleanup,
        "Failed to get attribute type for setting attribute to empty for '%s/%s/%s'", toks->table,
        toks->record, toks->attribute);

    if (type == SQLITE_BLOB) {
        TRY_SQLITE(sqlite3_bind_zeroblob(stmt, 1, 0), SQLITE_OK, cleanup,
                   "Failed to bind empty blob for setting attribute to empty for '%s/%s/%s'",
                   toks->table, toks->record, toks->attribute);
    } else {
        TRY_SQLITE(sqlite3_bind_text(stmt, 1, "", -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
                   "Failed to bind empty string for setting attribute to empty for '%s/%s/%s'",
                   toks->table, toks->record, toks->attribute);
    }

    // Bind the record ID for the WHERE clause to specify which record's attribute value should be
    // set to empty.
    TRY_SQLITE(sqlite3_bind_text(stmt, 2, toks->record, -1, SQLITE_TRANSIENT), SQLITE_OK, cleanup,
               "Failed to bind record value for setting attribute to empty for '%s/%s/%s'",
               toks->table, toks->record, toks->attribute);

    // Execute the UPDATE statement to set the attribute value to empty for the specified record.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
               "Failed to execute UPDATE query for setting attribute to empty for '%s/%s/%s'",
               toks->table, toks->record, toks->attribute);

    int changes = sqlite3_changes(db);
    LOG_DEBUG("Attribute set to empty successfully, %d rows affected", changes);

    if (cache_enabled) {
        // Evict the corresponding cache blocks, if exists
        CacheKey* key = NULL;
        TRY(get_cache_key_from_toks(toks, 0, &key), cleanup,
            "Failed to get cache key from tokens for eviction of '%s/%s/%s'", toks->table,
            toks->record, toks->attribute);
        LOG_TRACE("Evicting cache blocks for emptyified attribute: path='%s'", key->query);
        cache_evict_blocks_from_toks(toks);
        free(key);
    }

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t get_table_rowids(const char* table, char* records[], int* n_records) {
    ensure_arena_init();

    LOG_TRACE("Getting row IDs for table: %s", table);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt;

    // Build the dynamic query statement to select all row IDs from the specified table using the
    // QUERY_TPL_SELECT_TABLE_ROWIDS template.
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_SELECT_TABLE_ROWIDS, table),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for selecting rowids of table '%s'", table);

    int record_count = 0;

    // Iterate over the query results to retrieve the row IDs and store them in the provided
    // records.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* rowid = (const char*)sqlite3_column_text(stmt, 0);
        LOG_TRACE("Record count: %d", record_count);
        records[record_count++] = arena_strdup(arena, rowid);

        if (record_count >= MAX_SIZE) {
            LOG_WARN("Table '%s' has more than %d rows, truncating", table, MAX_SIZE);
            break;
        }
    }

    *n_records = record_count;
    LOG_DEBUG("Found %d rows in table '%s'", record_count, table);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

// FIX: this function should be implemented better using the query manager prepared statements
status_t get_rowid_from_pks(const char* table, Fk* fks[], char* fks_values[], int num_fks,
                            int* rowid) {
    ensure_arena_init();

    LOG_TRACE("Getting rowid from PKs for table: %s (num_fks=%d)", table, num_fks);

    status_t      status = STATUS_OK;
    sqlite3_stmt* pstmt;
    int           str_len = 0;
    char          query_str[1024];

    // Build the SQL query string to select the row ID from the specified table based on the
    // provided primary key values.
    str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len,
                        "SELECT rowid FROM %s WHERE ", table);

    // Iterate over the provided primary key values and append the corresponding conditions to the
    // SQL query string to filter the results based on the primary key values.
    for (int i = 0; i < num_fks; i++) {
        if (i > 0)
            str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, " AND ");

        str_len += snprintf(query_str + str_len, sizeof(query_str) - str_len, "%s = '%s'",
                            fks[i]->to, fks_values[i]);
    }

    LOG_TRACE("Built query: %s", query_str);

    // Prepare the SQL statement using sqlite3_prepare_v2 to compile the query string into a SQLite
    // statement object that can be executed.
    TRY_SQLITE(sqlite3_prepare_v2(db, query_str, -1, &pstmt, NULL), SQLITE_OK, cleanup,
               "Failed to prepare statement for getting rowid from PKs for table '%s'", table);

    // Execute the prepared statement using sqlite3_step and check if a row is returned, indicating
    // that a matching record was found based on the provided primary key values.
    TRY_SQLITE(
        sqlite3_step(pstmt), SQLITE_ROW, cleanup,
        "Failed to step through prepared statement for getting rowid from PKs for table '%s'",
        table);

    *rowid = sqlite3_column_int(pstmt, 0);
    LOG_DEBUG("Found rowid=%d for FK lookup in table '%s'", *rowid, table);

cleanup:
    if (pstmt)
        sqlite3_finalize(pstmt);
    return status;
}

status_t insert_record_into_table(struct tokens* toks) {
    LOG_TRACE("Inserting record into table: %s/%s", toks->table, toks->record);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    // Build the dynamic query statement to insert a new record into the specified table using the
    // QUERY_TPL_INSERT_RECORD_INTO_TABLE template.
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_INSERT_RECORD_INTO_TABLE,
                                                         toks->table, toks->record),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for inserting record into table: '%s/%s'",
                 toks->table, toks->record);

    // Execute the INSERT statement to add the new record to the database.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
               "Failed to execute query for inserting record into table: '%s/%s'", toks->table,
               toks->record);

    // Check the number of rows affected by the INSERT operation to confirm that the record was
    // successfully inserted into the database.
    int changes = sqlite3_changes(db);
    LOG_DEBUG("Record inserted successfully, %d rows affected", changes);

    // If caching is enabled, evict any corresponding cache blocks that may exist for the newly
    // inserted record to ensure that subsequent queries for this record will fetch the updated data
    // from the database rather than stale data from the cache.
    if (cache_enabled) {
        // Evict the corresponding cache blocks, if exists
        CacheKey* key = NULL;
        TRY(get_cache_key_from_toks(toks, 0, &key), cleanup,
            "Failed to get cache key from tokens for eviction of '%s/%s'", toks->table,
            toks->record);
        LOG_TRACE("Evicting cache blocks for new record insertion: path='%s'", key->query);

        // We cannot evict blocks using toks directly, because we want to evict all blocks related
        // to the record (all attributes), so we create a new toks with empty attribute for
        // eviction.
        // TODO: make a separate function that takes the path. Maybe it is easier like that.
        struct tokens toks_for_eviction = {
            .table     = toks->table,
            .record    = toks->record,
            .attribute = "", // Evict all attributes for the record
        };

        cache_evict_blocks_from_toks(&toks_for_eviction);

        free(key);
    }

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t create_empty_table(const char* table) {
    LOG_TRACE("Creating empty table: %s", table);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    // Build the dynamic query statement to create a new empty table with the specified name using
    // the QUERY_TPL_CREATE_EMPTY_TABLE template.
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_CREATE_EMPTY_TABLE, table),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for creating empty table '%s'", table);

    // Execute the CREATE TABLE statement to add the new empty table to the database.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
               "Failed to execute query for creating empty table '%s'", table);

    LOG_DEBUG("Empty table created successfully: %s", table);

    // After creating the new empty table in the database, we need to update our in-memory schema
    // representation to include the new table.
    Schema* new_schema;
    TRY_NOT_NULL(new_schema = malloc(sizeof(Schema)), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate schema for new table '%s'", table);

    // Initialize the new schema for the created table with its name and empty lists for primary
    // keys, attributes, and foreign keys.
    TRY_NOT_NULL(new_schema->name = strdup(table), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate name for new schema of table '%s'", table);
    new_schema->pk_head   = NULL;
    new_schema->attr_head = NULL;
    new_schema->fks_head  = NULL;

    // Since every table must have a primary key, we create a default primary key named "rowid" of
    // type INTEGER for the new table.
    Pk* new_pk;
    TRY_NOT_NULL(new_pk = malloc(sizeof(Pk)), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate PK for new table '%s'", table);
    TRY_NOT_NULL(new_pk->name = strdup("rowid"), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate PK name for new table '%s'", table);
    new_pk->sqlite_type = SQLITE_INTEGER;

    // Add the new primary key to the new schema and then add the new schema to the database schema
    // to ensure that our in-memory representation of the database schema is consistent with the
    // actual state of the database after creating the new table.
    add_pk_to_schema(new_schema, new_pk);
    add_schema(db_schema, new_schema);
    LOG_DEBUG("mkdir: schema for table '%s' created with PK 'rowid'", table);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t delete_schema_column(struct tokens* toks) {
    ensure_arena_init();
    LOG_TRACE("Deleting schema column: %s/%s/%s", toks->table, toks->record, toks->attribute);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    DotSchemaTokens* ds_toks;
    TRY(tokenize_dot_schema_column(arena, toks->attribute, &ds_toks), cleanup,
        "Failed to parse dot schema tokens for attribute '%s'", toks->attribute);

    // Build the dynamic query statement to delete a column from the specified table using the
    // QUERY_TPL_DELETE_SCHEMA_COLUMN template.
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_DROP_SCHEMA_COLUMN,
                                                         toks->table, ds_toks->column_name),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for deleting schema column '%s/%s/%s'",
                 toks->table, toks->record, ds_toks->column_name);

    // Execute the ALTER TABLE statement to delete the specified column from the database table.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
               "Failed to execute query for deleting schema column '%s/%s/%s'", toks->table,
               toks->record, ds_toks->column_name);

    LOG_DEBUG("Schema column deleted successfully: %s/%s/%s", toks->table, toks->record,
              ds_toks->column_name);

    // After deleting the column from the database table, we need to update our in-memory schema
    // representation to reflect the change by removing the corresponding attribute from the schema
    // of the affected table.
    Schema* table_schema;
    TRY_NOT_NULL(table_schema = find_schema_by_name(db_schema, toks->table), cleanup,
                 STATUS_DB_ERROR, "Table '%s' not found in schema for deleting column",
                 toks->table);
    remove_attribute_from_schema(table_schema, ds_toks->column_name);
    remove_pk_from_schema(table_schema, ds_toks->column_name);
    remove_fk_from_schema(table_schema, ds_toks->column_name);
    LOG_DEBUG("Schema updated to remove attribute '%s' from table '%s'", ds_toks->column_name,
              toks->table);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t add_pk_to_table(const char* table, const char* pk_name, const char* pk_type) {
    ensure_arena_init();
    LOG_TRACE("Adding primary key %s to table %s", pk_name, table);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    // Build the dynamic query statement to add a primary key column to the specified table using
    // ALTER TABLE new_table ADD id INTEGER PRIMARY KEY
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_ADD_PRIMARY_KEY_COLUMN,
                                                         table, pk_name, pk_type),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for adding PK column '%s' to table '%s'", pk_name,
                 table);

    // Execute the ALTER TABLE statement to add the new primary key column to the database table.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
               "Failed to execute query for adding PK column '%s' to table '%s'", pk_name, table);

    // Update the schema in-memory representation to include the new primary key for the specified
    // table
    Schema* table_schema;
    TRY_NOT_NULL(table_schema = find_schema_by_name(db_schema, table), cleanup, STATUS_DB_ERROR,
                 "Table '%s' not found in schema for adding PK", table);

    Pk* new_pk;
    TRY_NOT_NULL(new_pk = malloc(sizeof(Pk)), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate PK for new PK '%s' in table '%s'", pk_name, table);

    TRY_NOT_NULL(new_pk->name = strdup(pk_name), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate name for new PK '%s' in table '%s'", pk_name, table);

    new_pk->sqlite_type = parse_sqlite_type(pk_type);

    add_pk_to_schema(table_schema, new_pk);

    LOG_DEBUG("PK '%s' added to schema for table '%s'", pk_name, table);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t add_attribute_to_table(const char* table, const char* attr_name, const char* attr_type) {
    ensure_arena_init();
    LOG_TRACE("Adding attribute %s to table %s", attr_name, table);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    // Build the dynamic query statement to add a primary key column to the specified table using
    // ALTER TABLE new_table ADD name TEXT
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_ADD_ATTRIBUTE_COLUMN, table,
                                                         attr_name, attr_type),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for adding attribute '%s' to table '%s'",
                 attr_name, table);

    // Execute the ALTER TABLE statement to add the new primary key column to the database table.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
               "Failed to execute query for adding attribute '%s' to table '%s'", attr_name, table);

    // Update the schema in-memory representation to include the new primary key for the specified
    // table
    Schema* table_schema;
    TRY_NOT_NULL(table_schema = find_schema_by_name(db_schema, table), cleanup, STATUS_DB_ERROR,
                 "Table '%s' not found in schema for adding PK", table);

    Attr* new_attr;
    TRY_NOT_NULL(new_attr = malloc(sizeof(Attr)), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate attribute for new attribute '%s' in table '%s'", attr_name,
                 table);

    TRY_NOT_NULL(new_attr->name = strdup(attr_name), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate name for new attribute '%s' in table '%s'", attr_name, table);

    new_attr->sqlite_type = parse_sqlite_type(attr_type);

    add_attribute_to_schema(table_schema, new_attr);

    LOG_DEBUG("Attribute '%s' added to schema for table '%s'", attr_name, table);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}

status_t add_fk_to_table(const char* table, const char* fk_from, const char* fk_table,
                         const char* fk_to) {
    ensure_arena_init();
    LOG_TRACE("Adding FK %s to table %s referencing %s(%s)", fk_from, table, fk_table, fk_to);

    status_t      status = STATUS_OK;
    sqlite3_stmt* stmt   = NULL;

    // Build the dynamic query statement to add a primary key column to the specified table using
    // ALTER TABLE new_table ADD chart_id REFERENCES chart(id)
    TRY_NOT_NULL(stmt = qm_build_dynamic_query_statement(db, QUERY_TPL_ADD_FOREIGN_KEY_COLUMN,
                                                         table, fk_from, fk_table, fk_to),
                 cleanup, STATUS_DB_ERROR,
                 "Failed to build query statement for adding FK column '%s' to table '%s'", fk_from,
                 table);

    // Execute the ALTER TABLE statement to add the new primary key column to the database table.
    TRY_SQLITE(sqlite3_step(stmt), SQLITE_DONE, cleanup,
               "Failed to execute query for adding FK column '%s' to table '%s'", fk_from, table);

    // Update the schema in-memory representation to include the new primary key for the specified
    // table
    Schema* table_schema;
    TRY_NOT_NULL(table_schema = find_schema_by_name(db_schema, table), cleanup, STATUS_DB_ERROR,
                 "Table '%s' not found in schema for adding FK", table);

    Fk* new_fk;
    TRY_NOT_NULL(new_fk = malloc(sizeof(Fk)), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate FK for new FK '%s' in table '%s'", fk_from, table);

    TRY_NOT_NULL(new_fk->from = strdup(fk_from), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate name for new FK '%s' in table '%s'", fk_from, table);

    TRY_NOT_NULL(new_fk->table = strdup(fk_table), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate name for new FK '%s' in table '%s'", fk_table, table);

    TRY_NOT_NULL(new_fk->to = strdup(fk_to), cleanup, STATUS_ALLOC_ERROR,
                 "Failed to allocate name for new FK '%s' in table '%s'", fk_to, table);

    Schema* ref_table_schema;
    TRY_NOT_NULL(ref_table_schema = find_schema_by_name(db_schema, fk_table), cleanup,
                 STATUS_DB_ERROR,
                 "Referenced table '%s' not found in schema for adding FK '%s' to table '%s'",
                 fk_table, fk_from, table);

    Pk* ref_pk;
    TRY_NOT_NULL(ref_pk = find_pk_by_name(ref_table_schema, fk_to), cleanup, STATUS_DB_ERROR,
                 "Referenced PK '%s' not found in schema for adding FK '%s' to table '%s'", fk_to,
                 fk_from, table);

    new_fk->sqlite_type = ref_pk->sqlite_type;

    add_fk_to_schema(table_schema, new_fk);

    LOG_DEBUG("FK '%s' added to schema for table '%s'", fk_from, table);

cleanup:
    if (stmt)
        sqlite3_finalize(stmt);
    return status;
}
