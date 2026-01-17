# vfs2db
FUSE driver to navigate an SQL database just like a filesystem.
# Motivations
+ Legacy and compatibility: Do you have an old application that only reads from text files? With this driver, you can read data from a modern db without rewriting a single line of code.
+ Easy to use: This driver let's you navigate a modern db without learning SQL.
+ Script power: You can use bash, python, grep, awk or sed on a modern db. For example if you have to search for a string in all records you can do `$ grep -r "error..." /mnt/db/logs`.
+ Adaptability: This driver let's you have any type of tables in a hierarchical filesystem-like view.

## Todo
- [ ] refactoring
    - [x] pragma init
    - [x] errors
    - [x] query manager
    - [x] CMake building
    - [ ] Logger
        - Has different severity levels: DEBUG, INFO, WARNING, ERROR, FATAL
        - Has a timestamp when the event occurred.
        - Has a context in terms of the file, line and function where the event occurred.
        - Has a configurable output. (?)
        - Has the possibility to activate/deactivate at runtime.
    - [ ] ns-programming
    - [ ] general fixes.
        - Code styles are not defined.
        - Errors are handled inconsistently. Some functions return `-1` some `status_t`.
        - Memory leaks.
        - Defining constants for magic numbers.
        - Make the status_t enum more explicit by adding more types of errors.
- [ ] optimizations
    - [x] cache to save query result for multiple reads and writes of the same file
        - Make it limitless (for the rowid query)
        - Add metrics like hits, misses and evictions.
    - [ ] memory arena
        - Needs to be used for every FUSE operation.
            - We allocate small things and then we call reset to free the memory and leave it for a next operation.
        - Needs to be thread local because FUSE can be multi-threaded.
            - In order to be thread local we must add `__thread` to the type when declaring the arena.
        - API:
            - arena_create(size) --> returns an arena object.
            - arena_alloc(arena, size) --> returns the pointer to the newly allocated memory.
                - Be carefull with the alignment at the start.
            - arena_calloc(arena, count, size) --> just like calloc, the memory allocated is also zeroed.
            - arena_strdup(arena, string) --> duplicates a string inside the arena.
            - arena_reset(arena) --> deallocates the entire used memory inside the arena in O(1).
            - arena_destroy(arena) --> destroys the memory arena and frees the memory.
            - (HELPER) arena_used(arena) --> returns the amount of memory used.
            - (HELPER) arena_available(arena) --> returns the amount of available memory.
    - [x] schema hashmap
    - [x] fk hashmap
    - [x] pk & attr sets
- [ ] insert/create/alter table;
- [ ] delete (unlink, rmdir o remove);
- [ ] rowid;
- [ ] metadata;

### INSERT (insert new record)
> ""
> mkdir test/orders/4/

? INSERT INTO orders(rowid) VALUES (4);

> ls test/orders/4
id.vfs2db
price.vfs2db

> cat test/orders/4/id.vfs2db
4

> cat test/orders/4/price.vfs2db

### CREATE (insert new table)
> mkdir test/newtable/

? CREATE TABLE newtable(rowid)

> ls test/newtable

> mkdir test/newtable/1/

? INSERT INTO newtable(rowid) VALUES (1);

> ls test/newtable/1/
rowid.vfs2db

### ALTER TABLE ADD (insert new attribute)
> touch test/newtable/1/attr2.vfs2db

? ALTER TABLE newtable ADD attr2 TEXT;

> mkdir test/newtable/2/

? INSERT INTO newtable(rowid) VALUES (2);

> ls test/newtable/2/
rowid.vfs2db
attr2.vfs2db