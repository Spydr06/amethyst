#include <rand.h>

#include <stdint.h>

// taken from https://github.com/Spydr06/CSpydr/blob/main/src/std/random.csp

static unsigned init[32] = {
    0x00000000,0x5851f42d,0xc0b18ccf,0xcbb5f646,
    0xc7033129,0x30705b04,0x20fd5db4,0x9a8b7f78,
    0x502959d8,0xab894868,0x6c0356a7,0x88cdb7ff,
    0xb477d43f,0x70a3a52b,0xa8e4baf1,0xfd8341fc,
    0x8ae16fd9,0x742d2f7a,0x0d1f0796,0x76035e09,
    0x40f7702c,0x6fa72ca5,0xaaa84157,0x58a0df74,
    0xc74a0364,0xae533cc4,0x04185faf,0x6de3b115,
    0x0cab8628,0xf043bfa4,0x398150e9,0x37521657
};

static unsigned* x = init + 1, n = 31, i = 3, j = 0;

static inline unsigned lcg32(unsigned x) {
    return (1103515245 * x + 12345) & 0x7fffffff;
}

static inline uint64_t lcg64(uint64_t x) {
    return (1103515245ull * x + 12345ull) & 0x7fffffffull;
}

int rand(void) {
    uint64_t k = 0;

    if(!n)
        k = x[0] = lcg32(x[0]);
    else {
        x[i] += x[j];
        k = x[i] >> 1;
        if(++i == n)
            i = 0;
        if(++j == n)
            j = 0;
    }

    return (int) k;
}

void srand(unsigned seed) {
    uint64_t s = seed;

    if(!n) {
        x[0] = s;
        return;
    }

    i = n == 31 || n == 7 ? 3 : 1;
    j = 0;
    for(unsigned k = 0; k < n; k++) {
        s = lcg64(s);
        x[k] = s >> 32;
    }

    x[0] |= 1;
}

