#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>

#define BUFSIZ 1024

#define EOF (-1)

#define FOPEN_MAX 1024
#define FILENAME_MAX 4096

/* TODO: NaN is not supported */
#define _PRINTF_NAN_LEN_MAX 0

#define L_tmpnam 20

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;

FILE* fopen(const char *restrict filename, const char *restrict mode);
int fclose(FILE* stream);

int fflush(FILE* stream);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H */

