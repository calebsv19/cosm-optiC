#ifndef FAST_RNG_H
#define FAST_RNG_H

#include <stdint.h>

typedef struct {
    uint64_t state;
    uint64_t inc;
} FastRNG;

static inline uint32_t FastRNGNextUInt(FastRNG* rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static inline void FastRNGSeed(FastRNG* rng, uint64_t initstate, uint64_t initseq) {
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    FastRNGNextUInt(rng);
    rng->state += initstate;
    FastRNGNextUInt(rng);
}

static inline float FastRNGNextFloat(FastRNG* rng) {
    return (FastRNGNextUInt(rng) >> 8) * (1.0f / 16777216.0f);
}

static inline double FastRNGNextDouble(FastRNG* rng) {
    return FastRNGNextFloat(rng);
}

#endif // FAST_RNG_H
