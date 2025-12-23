#include "syscall_handler/syscall_handler.h"

sqlite3* db = NULL;

static const struct fuse_operations vfs2db_oper = {
	.getattr        = vfs2db_getattr,
    .getxattr       = vfs2db_getxattr,
	.readdir        = vfs2db_readdir,
	.read           = vfs2db_read,
    .write          = vfs2db_write,
    .create         = vfs2db_create,
    .readlink       = vfs2db_readlink,

    .init           = vfs2db_init,
    .destroy        = vfs2db_destroy,
};

struct options {
    const char *db_path;
};

#define OPTION(t, p) { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("db=%s", db_path),
    FUSE_OPT_END
};

int main(int argc, char *argv[]) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct options opt = { NULL };

    if (fuse_opt_parse(&args, &opt, option_spec, NULL) == -1) {
        return 1;
    }

    if (opt.db_path == NULL) {
        fprintf(stderr, "Errore: Devi specificare il path del database come primo argomento.\n");
        fuse_opt_free_args(&args);
        return 1;
    }

    int check = sqlite3_open_v2(opt.db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (check != SQLITE_OK) {
        fprintf(stderr, "sqlite3_open_v2 failed: %s\n", sqlite3_errmsg(db));
        free(opt.db_path);
        fuse_opt_free_args(&args);
        return 1;
    }

    int res = fuse_main(args.argc, args.argv, &vfs2db_oper, NULL);

    free(opt.db_path);
    fuse_opt_free_args(&args);
    return res;
}