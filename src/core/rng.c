#include "rng.h"

/* Marsaglia xorshift32. Period 2^32-1, and it passes the only property this
 * game needs: uniform low bits after the final shift. */

void rng_seed(Rng *r, uint32_t seed)
{
    r->s = (seed == 0u) ? 0x9E3779B9u : seed;
}

uint32_t rng_next(Rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

uint32_t rng_below(Rng *r, uint32_t bound)
{
    uint32_t limit, v;

    if (bound == 0u) {
        return 0u;
    }

    /* Largest multiple of bound that fits in 32 bits; draws at or above it are
     * rejected so every residue is equally likely. */
    limit = (uint32_t)(0xFFFFFFFFu - (0xFFFFFFFFu % bound));

    do {
        v = rng_next(r);
    } while (v >= limit);

    return v % bound;
}
