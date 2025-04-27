#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

long strtol(const char *restrict nptr, char **restrict endptr, int base) {
    long long ll = strtoll(nptr, endptr, base);
    if(errno && errno != ERANGE)
        return (long) ll;

    if(ll > LONG_MAX) {
        errno = ERANGE;
        return LONG_MAX; 
    }
    if(ll < LONG_MIN) {
        errno = ERANGE;
        return LONG_MIN;
    }

    return (long) ll;
}

long long strtoll(const char *restrict nptr, char **restrict endptr, int base) {
    register const char *p = nptr;

    while(isspace(*p)) p++;

    bool negate = *p == '-';
    if(negate || *p == '+') p++;

    unsigned long long ull = strtoull(nptr, endptr, base);
    if(errno && errno != ERANGE)
        return (long long) ull;

    if(ull > LLONG_MAX && !negate) {
        errno = ERANGE;
        return LLONG_MAX;
    }
    if(ull > 1ull + LLONG_MAX && negate) {
        errno = ERANGE;
        return LLONG_MIN;
    }

    return negate ? -(long long) ull : (long long) ull;
}

unsigned long strtoul(const char *restrict nptr, char **restrict endptr, int base) {
    unsigned long long ull = strtoull(nptr, endptr, base);
    if(errno && errno != ERANGE)
        return (unsigned long) ull;

    if(ull > ULONG_MAX) {
        errno = ERANGE;
        return LONG_MAX;
    }

    return (long) ull;
}

unsigned long long strtoull(const char *restrict nptr, char **restrict endptr, int base) {
    register const char *s = nptr;
	register unsigned long long acc;
	register int c;
	register unsigned long long cutoff = ULLONG_MAX / base;
	register int any, cutlim = ULLONG_MAX % base;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s++;
	} while(isspace(c));

	if((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if(base == 0)
		base = c == '0' ? 8 : 10;

	for(acc = 0, any = 0;; c = *s++) {
		if(isdigit(c))
			c -= '0';
		else if(isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;

		if(c >= base)
			break;

		if(any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if(any < 0) {
		acc = ULONG_MAX;
		errno = ERANGE;
	}

	if(endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return acc;
}

