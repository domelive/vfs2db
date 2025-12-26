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
    - [ ] db_schema/schema hashmap
    - [ ] (logging & debugging)
    - [ ] ns-programming
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