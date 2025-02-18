#ifndef _STDIO_H
#define _STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
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

size_t fread(void *restrict ptr, size_t nmemb, size_t size, FILE *restrict stream);
size_t fwrite(const void *restrict ptr, size_t nmemb, size_t size, FILE *restrict stream);

int fputc(int c, FILE *restrict stream);
int fputs(const char *restrict s, FILE *restrict stream);
int puts(const char *restrict s);

int fgetc(FILE *restrict stream);
char *fgets(char *restrict s, int size, FILE *restrict stream);

int getc(FILE *restrict stream);
int ungetc(int c, FILE *stream);

int fflush(FILE *stream);
int feof(FILE *stream);

int ferror(FILE *stream);
void clearerr(FILE *stream);

int printf(const char *restrict format, ...);
int vprintf(const char *restrict format, va_list ap);

int fprintf(FILE *restrict stream, const char *restrict format, ...);
int vfprintf(FILE *restrict stream, const char *restrict format, va_list ap);

int dprintf(int fd, const char *restrict format, ...);
int vdprintf(int fd, const char *restrict format, va_list ap);

int sprintf(char *restrict str, const char *restrict format, ...);
int vsprintf(char *restrict str, const char *restrict format, va_list ap);

int snprintf(char *restrict str, size_t size, const char *restrict format, ...);
int vsnprintf(char *restrict str, size_t size, const char *restrict format, va_list ap);

int scanf(const char *restrict format, ...);
int vscanf(const char *restrict format, va_list ap);

int fscanf(FILE *restrict stream, const char *restrict format, ...);
int vfscanf(FILE *restrict stream, const char *restrict format, va_list ap);

int sscanf(const char *restrict str, const char *restrict format, ...);
int vsscanf(const char *restrict str, const char *restrict format, va_list ap);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H */

