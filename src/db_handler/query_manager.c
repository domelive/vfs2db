#include "query_manager.h"

static const char* sql_store[] = {
    [QUERY_GET_TABLES_NAME] = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';",
    [QUERY_GET_TABLE_INFO]  = "SELECT " 
                                    "ti.name AS column_name,"
                                    "ti.pk AS is_pk,"
                                    "fk.\"table\" AS fk_table,"
                                    "fk.\"to\" AS fk_column_name "
                              "FROM "
                                    "pragma_table_info('%s') ti "
                                    "LEFT JOIN "
                                    "pragma_foreign_key_list('%s') fk "
                              "ON ti.name = fk.\"from\";",
};

char *qm_get_query_str(QueryID qid) {
    if (qid < 0 || qid >= QUERY_COUNT) return NULL;
    return sql_store[qid];
}