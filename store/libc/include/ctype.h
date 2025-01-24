#ifndef _CTYPE_H
#define _CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

int isalnum(int c);
int isalpha(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int toupper(int c);
int tolower(int c);

#ifdef _XOPEN_SOURCE
    int isascii(int c);
#endif

#if defined(_ISOC99_SOURCE) || defined(_POSIX_C_SOURCE)
    int isblank(int c);
#endif

// TODO: _l localized functions

#ifdef __cplusplus
}
#endif

#endif /* _CTYPE_H */

