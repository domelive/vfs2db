#ifndef TYPES_H
#define TYPES_H

#include "const.h"

// =============================================================
// Metadata Structures
// =============================================================

/**
 * Foreign Key Structure
 * 
 * from:  attribute in the current table
 * table: referenced table name
 * to:    referenced attribute in the referenced table
 */
typedef struct Fk {
    char *from;
    char *table;
    char *to;
} Fk;

/**
 * Schema Structure
 *
 * name:    table name
 * pk:      primary key's attributes' names
 * attr:    attributes' names
 * fks:     foreign keys' structures
 * n_pk:    primary key's attributes' number
 * n_attr:  attributes' number
 * n_fks:   foreign keys' attributes' number
 */
typedef struct Schema {
    char *name;

    char *pk[MAX_SIZE];
    char *attr[MAX_SIZE];
    Fk   *fks[MAX_SIZE];
    
    int   n_pk;
    int   n_attr;
    int   n_fks;
} Schema; 

// =============================================================
// Global Structures
// =============================================================

/**
 * Global Database Schema Structure
 * 
 * tables:   tables schemas
 * n_tables: number of tables schemas
 */
typedef struct DbSchema {
    Schema *tables[MAX_SIZE];
    int     n_tables;
} DbSchema;

// =============================================================
// Other Structures
// =============================================================

// FIX: make it a type, like Tokens
struct tokens {
    char *table;
    char *record;    char *attribute;
};


// FIX: destroy this
struct pkfk_relation {
    char *fk_name;
    char *pk_name;
    char *value;
};

#endif // TYPES_H