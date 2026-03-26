/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * @file   db_handler.h
 * @author Domenico Livera (domenico.livera@gmail.com)
 * @author Nicola Travaglini (nicola1.travaglini@gmail.com)
 * @brief  Database Handler Header File
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

#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arena.h"
#include "errors.h"
#include "helpers.h"
#include "logger.h"
#include "query_manager.h"
#include "types.h"

/**
 * Set SQLite PRAGMA
 *
 * @brief Sets a SQLite PRAGMA option to the specified value.
 *
 * This function constructs and executes a SQL query to set the given PRAGMA option
 * to the specified value in the SQLite database. It is used to configure various aspects of the
 * database behavior, such as enabling foreign key constraints or adjusting performance settings.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] pragma The name of the PRAGMA option to set (e.g., "foreign_keys")
 * @param[in] value The value to set for the PRAGMA option (e.g., "ON" or "OFF")
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t set_sqlite_pragma(Vfs2DbContext* ctx, const char* pragma, const char* value);

/**
 * Initialize Database Schema
 *
 * @brief Initializes the database schema by retrieving the names of all tables in the database
 * and populating the DbSchema structure with this information.
 *
 * This function executes a SQL query to fetch the table names and creates a Schema structure for
 * each table, which is then added to the DbSchema's hash map of tables.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t init_db_schema(Vfs2DbContext* ctx);

/**
 * Initialize Schema
 *
 * @brief Initializes the schema for a specific table by retrieving column information and
 * populating the Schema structure with details about primary keys, attributes, and foreign keys.
 *
 * This function executes a SQL query to fetch column information for the specified table and
 * processes the results to fill the Schema structure accordingly.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 * @param[in,out] schema Pointer to the Schema structure to be initialized with column information
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t init_schema(Vfs2DbContext* ctx, Schema* schema);

/**
 * Record Exists
 *
 * @brief Checks if a specific record exists in a given table by executing a SQL query that
 * searches for the record based on its row ID.
 *
 * This function returns STATUS_OK if the record exists, or an appropriate error status if it does
 * not exist or if there is a database error.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to a tokens structure containing the table name and record identifier
 *
 * @return STATUS_OK if the record exists, or an appropriate error status on failure
 */
status_t record_exists(Vfs2DbContext* ctx, struct tokens* toks);

/**
 * Get Attribute All Bytes
 *
 * @brief Retrieves the entire byte content of a specific attribute value for a given record in a
 * table.
 *
 * This function executes a SQL query to fetch the attribute value as a blob and returns the
 * data along with its size in bytes. The retrieved data is duplicated into a thread-local arena for
 * efficient memory management.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name
 * @param[out] bytes Pointer to a char pointer where the retrieved attribute bytes will be stored
 * @param[out] size Pointer to a size_t variable where the size of the retrieved attribute data in
 * bytes will be stored
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t get_attribute_all_bytes(Vfs2DbContext* ctx, struct tokens* toks, char** bytes,
                                 size_t* size);

/**
 * Get Attribute Size
 *
 * @brief Retrieves the size in bytes of a specific attribute value for a given record in a table.
 *
 * This function executes a SQL query to calculate the size of the attribute value and returns it
 * through the `size` output parameter.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name
 * @param[in] offset Byte offset within the attribute value from which to start retrieving data
 * (used for chunked reads)
 * @param[out] bytes Pointer to a char pointer where the retrieved attribute bytes will be stored
 * (used for chunked reads)
 * @param[in] size Pointer to a size_t variable where the size of the attribute data in bytes will
 * be stored
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t get_attribute_chunk_bytes(Vfs2DbContext* ctx, struct tokens* toks, off_t offset,
                                   char** bytes, size_t size);

/**
 * Is Attribute NULL
 *
 * @brief Checks if a specific attribute value for a given record in a table is NULL.
 *
 * This function executes a SQL query to determine if the attribute value is NULL and returns the
 * result through the `is_null` output parameter.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name
 * @param[out] is_null Pointer to a boolean variable where the result will be stored (true if the
 * attribute value is NULL, false otherwise)
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t is_attribute_null(Vfs2DbContext* ctx, struct tokens* toks, bool* is_null);

/**
 * Get Attribute Type
 *
 * @brief Retrieves the SQLite data type of a specific attribute for a given record in a table.
 *
 * This function checks the database schema to determine if the attribute is a foreign key, primary
 * key, or normal attribute, and returns the corresponding SQLite data type through the `type`
 * output parameter.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name
 * @param[out] size Pointer to an integer variable where the SQLite data type of the attribute will
 * be stored
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t get_attribute_size(Vfs2DbContext* ctx, struct tokens* toks, size_t* size);

/**
 * Get Attribute Type
 *
 * @brief Retrieves the SQLite data type of a specific attribute for a given record in a table.
 *
 * This function checks the database schema to determine if the attribute is a foreign key, primary
 * key, or normal attribute, and returns the corresponding SQLite data type through the `type`
 * output parameter.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name
 * @param[out] type Pointer to an integer variable where the SQLite data type of the attribute will
 * be stored
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t get_attribute_type(Vfs2DbContext* ctx, struct tokens* toks, int* type);

/**
 * Bind Attribute Value
 *
 * @brief Binds a specific attribute value to a prepared SQLite statement based on the attribute's
 * SQLite data type.
 *
 * This function converts the string representation of the attribute value to the
 * appropriate data type (e.g., integer, float, text) and binds it to the provided SQLite statement
 * using the correct sqlite3_bind_* function.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name
 * @param[in] buffer Pointer to the buffer containing the new attribute value to be bound to the
 * statement
 * @param[in] size Size of the buffer in bytes
 * @param[in] offset Byte offset within the attribute value from which to start binding data (
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t update_attribute_value(Vfs2DbContext* ctx, struct tokens* toks, const char* buffer,
                                size_t size, off_t offset);

/**
 * Update Foreign Key Value
 *
 * @brief Updates the value of a foreign key attribute for a given record in a table.
 *
 * This function constructs and executes a SQL query to update the foreign key value based on the
 * provided link path tokens and the new foreign key record value.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks_linkpath Pointer to a tokens structure containing the table name, record
 * identifier, and foreign key attribute name for the link path
 * @param[in] toks_target Pointer to a tokens structure containing the table name, record
 * identifier, and foreign key attribute name for the target

 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t update_fk_value(Vfs2DbContext* ctx, struct tokens* toks_linkpath,
                         struct tokens* toks_target);

/**
 * Set Attribute Empty
 *
 * @brief Sets the value of a specific attribute for a given record in a table to an empty value.
 *
 * This function constructs and executes a SQL query to update the attribute value to an empty
 * string or zero value based on the attribute's SQLite data type.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name for which the value should be set to empty
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t set_attribute_empty(Vfs2DbContext* ctx, struct tokens* toks);

/**
 * Get Table Row IDs
 *
 * @brief Retrieves the row IDs of all records in a specified table.
 *
 * This function executes a SQL query to fetch the row IDs of all records in the given table and
 * returns them through the `records` output parameter, along with the total number of records
 * through the `n_records` output parameter.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] table Name of the table for which to retrieve the row IDs
 * @param[out] records Pointer to an array of char pointers where the retrieved row IDs will be
 * stored
 * @param[out] n_records Pointer to an integer variable where the total number of records in the
 * table will be stored
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t get_table_rowids(Vfs2DbContext* ctx, const char* table, char* records[], int* n_records);

/**
 * Get Row ID from Primary Keys
 *
 * @brief Retrieves the row ID of a specific record in a table based on its primary key values.
 *
 * This function constructs and executes a SQL query to find the row ID of a record in the specified
 * table that matches the provided primary key values. The primary key values are passed as an array
 * of Fk structures and their corresponding string values.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] table Name of the table to search for the record
 * @param[in] fks Array of Fk structures representing the primary key columns of the table
 * @param[in] fks_values Array of string values corresponding to the primary key columns
 * @param[in] num_fks Number of primary key columns (size of the fks and fks_values arrays)
 * @param[out] row_id Pointer to an integer variable where the retrieved row ID will be stored if a
 * matching record is found
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t get_rowid_from_pks(Vfs2DbContext* ctx, const char* table, Fk* fks[], char* fks_values[],
                            int num_fks, int* row_id);

/**
 * Insert Record into Table
 *
 * @brief Inserts a new record into the specified table with the given row ID.
 *
 * This function constructs and executes a SQL query to insert a new record
 * with the specified row ID into the given table.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to tokens structure containing table and record information
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t insert_record_into_table(Vfs2DbContext* ctx, struct tokens* toks);

/**
 * Create Empty Table
 *
 * @brief Creates a new empty table in the database with the specified name. The new table will
 * have a single column named `rowid` which is an INTEGER PRIMARY KEY that auto-increments with
 * each new record inserted.
 *
 * This function constructs and executes a SQL query to create the new table if it does not
 * already exist in the database.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] table Name of the table to create
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t create_empty_table(Vfs2DbContext* ctx, const char* table);

/**
 * Drop Table
 *
 * @brief Drops a specified table from the database, removing all its data and schema definition.
 * This function constructs and executes a SQL query to drop the table if it exists in the database.

 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] table Name of the table to drop
 */
status_t drop_table(Vfs2DbContext* ctx, const char* table);

/**
 * Delete Record from Table
 *
 * @brief Deletes a specific record from a table in the database.
 *
 * This function constructs and executes a SQL query to delete the record with the specified ID
 * from the given table.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection
 * @param[in] toks Pointer to tokens structure containing table and record information
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t delete_record_from_table(Vfs2DbContext* ctx, struct tokens* toks);

/**
 * Delete Schema Column
 *
 * @brief Deletes a specific column from a table in the database and updates the in-memory schema
 * representation accordingly.
 *
 * This function constructs and executes a SQL query to drop the specified column from the given
 * table. After successfully deleting the column from the database, it also updates the in-memory
 * schema representation by removing the corresponding attribute, primary key, and foreign key
 * entries from the schema of the affected table.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 * @param[in] toks Pointer to a tokens structure containing the table name, record identifier, and
 * attribute name of the column to be deleted
 *
 * @return STATUS_OK on success, or an appropriate error status on failure
 */
status_t delete_schema_column(Vfs2DbContext* ctx, struct tokens* toks);

/**
 * Add Primary Key to Table
 *
 * @brief Adds a new primary key column to a specified table in the database and updates the
 * in-memory schema representation accordingly.
 *
 * This function constructs and executes a SQL query to add a new primary key column with the given
 * name and type to the specified table. It also updates the database schema to reflect the addition
 * of the new primary key.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 * @param[in] table Name of the table to modify
 * @param[in] pk_name Name of the primary key column to add
 * @param[in] pk_type SQLite data type of the primary key column (e.g., INTEGER, TEXT)
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t add_pk_to_table(Vfs2DbContext* ctx, const char* table, const char* pk_name,
                         const char* pk_type);

/**
 * Add Attribute to Table
 *
 * @brief Adds a new attribute column to a specified table in the database and updates the
 * in-memory schema representation accordingly.
 *
 * This function constructs and executes a SQL query to add a new attribute column with the given
 * name and type to the specified table. It also updates the database schema to reflect the addition
 * of the new attribute.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 * @param[in] table Name of the table to modify
 * @param[in] attr_name Name of the attribute column to add
 * @param[in] attr_type SQLite data type of the attribute column (e.g., INTEGER, TEXT)
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t add_attribute_to_table(Vfs2DbContext* ctx, const char* table, const char* attr_name,
                                const char* attr_type);

/**
 * Add Foreign Key to Table
 *
 * @brief Adds a new foreign key column to a specified table in the database, referencing another
 * table, and updates the in-memory schema representation accordingly.
 *
 * This function constructs and executes a SQL query to add a new foreign key column with the given
 * name to the specified table, referencing the primary key of another table. It also updates the
 * database schema to reflect the addition of the new foreign key.
 *
 * @param[in] ctx Pointer to the Vfs2DbContext containing the database connection and schema
 * @param[in] table Name of the table to modify
 * @param[in] fk_from Name of the foreign key column to add
 * @param[in] fk_table Name of the referenced table that the foreign key will point to
 * @param[in] fk_to Name of the primary key column in the referenced table that the foreign key will
 * reference
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t add_fk_to_table(Vfs2DbContext* ctx, const char* table, const char* fk_from,
                         const char* fk_table, const char* fk_to);

#endif // DB_HANDLER_H
