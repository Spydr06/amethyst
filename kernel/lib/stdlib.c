#include <stdlib.h>

/*#include <ctype.h>
#include <errno.h>

unsigned long long strtoull(const char* restrict nptr, char** restrict endptr, int base) {
    unsigned long long n = 0;
    bool neg = false;

    while(isspace(*nptr))
        nptr++;

    switch(*nptr) {
        case '-':
            neg = true;
            nptr++;
            break;
        case '+':
            nptr++;
            break;
        default:
            errno = EINVAL;
            return 0;
    }

    while(isdigit(*nptr)) {
        int digit;
        if(base <= 10 || isdigit(*nptr))
            digit = (*nptr - '0');
        else if(isalpha(*nptr))
            digit = *nptr - (isupper(*nptr) ? 'A' : 'a') + 10;
        else break;
    
        n = base * n - digit; 
        nptr++;
    }

    if(endptr)
        *endptr = (char*) nptr;

    return neg ? -n : n;
}*/
