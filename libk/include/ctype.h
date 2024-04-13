#ifndef _AMETHYST_LIBK_CTYPE_H
#define _AMETHYST_LIBK_CTYPE_H

int isalnum(int ch);
int isalpha(int ch);
int islower(int ch);
int isupper(int ch);
int isdigit(int ch);
int isxdigit(int ch);
int iscntrl(int ch);
int isgraph(int ch);
int isspace(int ch);
int isblank(int ch);
int isprint(int ch);
int ispunct(int ch);

int tolower(int ch);
int toupper(int ch);

#endif /* _AMETNYST_LIBK_CTYPE_H */
