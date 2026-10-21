#ifndef CIVPSP_RNG_H
#define CIVPSP_RNG_H

#include <stdint.h>

void Rng_Seed(uint32_t seed);
void Rng_SeedFromClock(void);
uint32_t Rng_Next(void);
uint32_t Rng_Range(uint32_t bound);
uint32_t Rng_GetState(void);
void Rng_SetState(uint32_t state);

#endif
