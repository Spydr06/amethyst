#include <stdio.h>

#include <internal/file.h>

static unsigned char stdin_buf[BUFSIZ + MAX_UNGET];
static unsigned char stdout_buf[BUFSIZ + MAX_UNGET];
static unsigned char stderr_buf[BUFSIZ + MAX_UNGET];

static FILE stdin_file = {
    .buf = stdin_buf + MAX_UNGET,
    .buf_size = BUFSIZ,
    .fd = 0,
    .flags = F_PERM | F_NOWR,
    .read = __file_read,
    .seek = __file_seek,
    .close = __file_close
};

static FILE stdout_file = {
    .buf = stdout_buf + MAX_UNGET,
    .buf_size = BUFSIZ,
    .fd = 1,
    .flags = F_PERM | F_NORD,
    .lbf = '\n',
    .write = __file_write,
    .seek = __file_seek,
    .close = __file_close
};

static FILE stderr_file = {
    .buf = stderr_buf + MAX_UNGET,
    .buf_size = BUFSIZ,
    .fd = 2,
    .flags = F_PERM | F_NORD,
    .lbf = EOF,
    .write = __file_write,
    .seek = __file_seek,
    .close = __file_close
};

FILE *const stdin = &stdin_file;
FILE *const stdout = &stdout_file;
FILE *const stderr = &stderr_file;

