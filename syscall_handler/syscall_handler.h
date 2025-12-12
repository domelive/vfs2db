#ifndef SYSCALL_HANDLER_H
#define SYSCALL_HANDLER_H

#define FUSE_USE_VERSION 30

#include <fuse3/fuse.h>

#include "../db_handler/db_handler.h"

#define COUNT_CHAR(str, ch) ({ \
    int count = 0; \
    for (int i = 0; str[i] != '\0'; i++) { \
        if (str[i] == ch) count++; \
    } \
    count; \
})

void vfs2db_destroy(void *private_data);

int  vfs2db_getattr(const char *path, struct stat *st, struct fuse_file_info *fi);
int  vfs2db_getxattr(const char *path, const char *name, char *value, size_t size);
int  vfs2db_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int  vfs2db_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
int  vfs2db_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
int  vfs2db_create(const char* path, mode_t mode, struct fuse_file_info *fi);

#endif // SYSCALL_HANDLER_H