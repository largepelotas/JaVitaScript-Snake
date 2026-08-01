/*
 * Deterministic RNG for the game core.
 *
 * PLAN.md 4.2: never calls time(); the shell supplies the seed. Determinism is
 * what makes the replay harness (PLAN.md 4.7) able to prove a refactor did not
 * change behavior.
 */
#ifndef RNG_H
#define RNG_H

#include "snake_types.h"

/* A zero seed is a fixed point of xorshift32 and would make the generator emit
 * zero forever, so it is remapped. Any other value is used as given. */
void rng_seed(Rng *r, uint32_t seed);

uint32_t rng_next(Rng *r);

/* Uniform in [0, bound). Returns 0 when bound is 0.
 *
 * Uses rejection sampling rather than a plain modulo: with a modulo the low
 * residues would be very slightly more likely, which would bias food placement
 * toward one corner of the board over a long game. */
uint32_t rng_below(Rng *r, uint32_t bound);

#endif /* RNG_H */
