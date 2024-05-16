#include <math.h>

double pow10(int x) {
    double result = 1.0;
    int i;

    if(x >= 0)
        for (i = 0; i < x; i++)
            result *= 10;
    else
        for (i = 0; i > x; i--)
            result /= 10;

    return result;
}
