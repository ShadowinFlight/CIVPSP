#include "rng.h"

#include <psprtc.h>

static uint32_t rng_state = 0x9E3779B9u;

void Rng_Seed(uint32_t seed) {
    rng_state = seed ? seed : 0x9E3779B9u;
}

void Rng_SeedFromClock(void) {
    u64 tick = 0;
    sceRtcGetCurrentTick(&tick);
    Rng_Seed((uint32_t)(tick ^ (tick >> 32)));
}

uint32_t Rng_Next(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

uint32_t Rng_Range(uint32_t bound) {
    if (bound == 0) return 0;
    return Rng_Next() % bound;
}

uint32_t Rng_GetState(void) {
    return rng_state;
}

void Rng_SetState(uint32_t state) {
    rng_state = state ? state : 0x9E3779B9u;
}
