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
#include "cache_manager.h"
#include "errors.h"
#include "helpers.h"
#include "logger.h"
#include "query_manager.h"
#include "types.h"

extern sqlite3*  db;
extern DbSchema* db_schema;

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
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t init_db_schema(DbSchema* db_schema);

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
 * - `PRAGMA foreign_key_list(table_name)`: foreign key informations `| id | seq | table | from | to
 * | on_update | on_delete | match |`
 *
 * @param[out] schema Pointer to Schema structure to initialize
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t init_schema(Schema* schema);

/**
 * Record Exists
 *
 * @brief Checks if a record exists in the database based on the provided tokens.
 *
 * This function executes a SQL query to check for the existence of a record in the specified table
 * that matches the given record identifier. It returns STATUS_OK if the record exists, or an
 * appropriate error status if it does not exist or if there is a database error.
 *
 * @param[in] toks Pointer to tokens structure containing table and record information
 *
 * @return STATUS_OK if the record exists, STATUS_DB_ERROR if there is a database error, or
 * STATUS_DB_NOTFOUND if the record does not exist
 */
status_t record_exists(struct tokens* toks);

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
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t get_attribute_chunk_bytes(struct tokens* toks, off_t offset, char** bytes);

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
status_t get_attribute_all_bytes(struct tokens* toks, char** bytes, size_t* size);

/**
 * Get Attribute Type
 * @todo Handle error cases properly
 *
 * @brief Retrieves the SQLite data type of a specific attribute value
 *        for a given record in a table.
 *
 * This function executes a SQL query to fetch the attribute value
 * and determines its SQLite data type (e.g., INTEGER, TEXT, BLOB).
 *
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute information
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t get_attribute_size(struct tokens* toks, size_t* size);

/**
 * Update Attribute Value
 * @todo Handle error cases properly
 *
 * @brief Updates a specific attribute value for a given record in a table.
 *
 * This function performs the following steps:
 * 1. Evicts the corresponding cache block if it exists.
 * 2. Reads the existing attribute value from the database.
 * 3. Patches the existing value with the new data at the specified offset.
 * 4. Writes the updated value back to the database.
 *
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute information
 * @param[in] buffer Pointer to the new data to write
 * @param[in] size Size of the new data in bytes
 * @param[in] offset Byte offset within the attribute value where the update should begin
 * @param[in] attr_size Total size of the attribute value in bytes (before update)
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t get_attribute_type(struct tokens* toks, int* type);

/**
 * Get Table Row IDs
 * @todo Handle error cases properly
 *
 * @brief Retrieves the row IDs of all records in a specified table.
 *
 * This function executes a SQL query to fetch the row IDs of all records
 * in the specified table and returns them as an array of strings.
 *
 * @param[in] table Name of the table to query
 * @param[out] records Array of strings where the row IDs will be stored
 * @param[out] n_records Pointer to an integer where the number of records will be stored
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t update_attribute_value(struct tokens* toks, const char* buffer, size_t size, off_t offset);

/**
 * Set Attribute to NULL
 * @todo Handle error cases properly
 *
 * @brief Sets a specific attribute value to NULL for a given record in a table.
 *
 * This function executes a SQL query to update the specified attribute value
 * to NULL in the database. It also evicts the corresponding cache block if it exists.
 *
 * @param[in] toks Pointer to tokens structure containing table, record, and attribute information
 */
status_t set_attribute_empty(struct tokens* toks);

/**
 * Get Row ID from Primary Keys
 * @todo Handle error cases properly
 *
 * @brief Retrieves the row ID of a record in a table based on the provided primary key values.
 * This function constructs and executes a SQL query to find the row ID of a record
 * that matches the specified primary key values.
 *
 * @param[in] table Name of the table to query
 * @param[in] fks Array of pointers to Fk structures containing primary key names and values
 * @param[in] fks_values Array of strings containing the corresponding primary key values
 * @param[in] num_fks Number of primary keys provided in the fks array
 * @param[out] row_id Pointer to an integer where the retrieved row ID will be stored if found
 *
 * @return STATUS_OK on success, STATUS_DB_ERROR on failure
 */
status_t get_table_rowids(const char* table, char* records[], int* n_records);

/**
 * Get Row ID from Primary Keys
 * @todo Handle error cases properly
 *
 * @brief Retrieves the row ID of a record in a table based on the provided primary key values.
 *
 * This function constructs and executes a SQL query to find the row ID of a record
 * that matches the specified primary key values.
 *
 * @param[in] table Name of the table to query
 * @param[in] fks Array of pointers to Fk structures containing primary key names and values
 * @param[in] fks_values Array of strings containing the corresponding primary key values
 * @param[in] num_fks Number of primary keys provided in the fks array
 * @param[out] row_id Pointer to an integer where the retrieved row ID will be stored if found
 */
status_t get_rowid_from_pks(const char* table, Fk* fks[], char* fks_values[], int num_fks,
                            int* row_id);

#endif // DB_HANDLER_H