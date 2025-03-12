#ifndef _SHARD_SHELL_STMT_H
#define _SHARD_SHELL_STMT_H

enum shell_stmt_type {
    SH_STMT_LIST,
};

struct shell_stmt {
    enum shell_stmt_type type;
};



#endif /* _SHARD_SHELL_STMT_H */
