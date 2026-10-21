#ifndef CIVPSP_AI_H
#define CIVPSP_AI_H

#include "map.h"
#include "world.h"

/* Runs rival civilizations and barbarians after the player ends a turn.
 * The logic is deterministic and synchronous; combat and city capture use the
 * same rule functions as the player. */
void Ai_ProcessTurn(World *world, Map *map);

#endif
