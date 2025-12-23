#ifndef DB_HANDLER_H
#define DB_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include "query_manager.h"
#include "../utils/types.h"

extern sqlite3 *db;

int  init_db_schema(DbSchema *db_schema);
int  init_schema(Schema *schema);

int  get_attribute_size(struct tokens* toks);
int  get_attribute_value(struct tokens* toks, char **bytes, size_t *size);
int  get_attribute_type(struct tokens *toks);
int  update_attribute_value(struct tokens* toks, const char *buffer, size_t size, int append);
void make_root_select(sqlite3_stmt **pstmt);
void make_table_select(sqlite3_stmt **pstmt, const char *table);
void make_record_select(sqlite3_stmt **pstmt, const char *table);
void get_table_fks(sqlite3_stmt **pstmt, const char *table);
void get_foreign_table_attribute_name(struct tokens *toks, char **ftable, char **fattribute);
int  get_all_fkpk_relationships_length(const char *src_table, const char *dst_table);
void get_all_fkpk_relationships(const char *src_table, const char *dst_table, struct pkfk_relation *pkfk);
void fill_fk_values(const char *table, const char *record, struct pkfk_relation *pkfk, int pkfk_length);
int  get_rowid_from_pks(const char *table, struct pkfk_relation *pkfk, int pkfk_length);

#endif // DB_HANDLER_H