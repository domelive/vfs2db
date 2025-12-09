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

struct tokens {
    char *table;
    char *record;
    char *attribute;
};

extern sqlite3 *db;

int  get_attribute_size(struct tokens* toks);
int  get_attribute_value(struct tokens* toks, char **bytes, size_t *size);
void make_root_select(sqlite3_stmt **pstmt);
void make_table_select(sqlite3_stmt **pstmt, const char *table);
void make_record_select(sqlite3_stmt **pstmt, const char *table);

#endif // DB_HANDLER_H