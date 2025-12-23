#ifndef QUERY_MANAGER_H
#define QUERY_MANAGER_H

#include <stdio.h>
#include <sqlite3.h>

typedef enum {
    QUERY_GET_TABLES_NAME,
    QUERY_GET_TABLE_INFO,
    QUERY_COUNT
} QueryID;

int  qm_init(sqlite3 *db);
void qm_cleanup();

char         *qm_get_query_str(QueryID qid);
sqlite3_stmt *qm_get_static(QueryID qid);

// TODO: variadic function to build a dynamic query

#endif // QUERY_MANAGER_H