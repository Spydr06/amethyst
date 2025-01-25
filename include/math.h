#ifndef _AMETHYST_LIBK_MATH_H
#define _AMETHYST_LIBK_MATH_H

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define ROUND_DOWN(v, n) ((v) - ((v) % (n)))
#define ROUND_UP(v, n) ROUND_DOWN((v) + (n) - 1, n)

#define ROUND_UP_DIV(n, d) (((n) + (d) - 1) / (d))

double pow10(int x);

#endif /* _AMETHYST_LIBK_MATH_H */

