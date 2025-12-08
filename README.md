# vfs2db
FUSE driver to navigate an SQL database just like a filesystem.
# Motivations
+ Legacy and compatibility: Do you have an old application that only reads from text files? With this driver, you can read data from a modern db without rewriting a single line of code.
+ Easy to use: This driver let's you navigate a modern db without learning SQL.
+ Script power: You can use bash, python, grep, awk or sed on a modern db. For example if you have to search for a string in all records you can do `$ grep -r "error..." /mnt/db/logs`.
+ Adaptability: This driver let's you have any type of tables in a hierarchical filesystem-like view.
