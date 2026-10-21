#ifndef CIVPSP_IMPROVEMENT_H
#define CIVPSP_IMPROVEMENT_H

#include <stdint.h>

#include "city.h"
#include "map.h"
#include "unit.h"

/* M4.1 worker improvements.  A worker may improve a land tile owned by a city
 * of its own owner, provided the improvement's technology is known and the
 * terrain/resource fits.  Building is instant and consumes the worker's
 * remaining movement for the turn. */
typedef enum {
    IMPROVEMENT_OK = 0,
    IMPROVEMENT_ERR_INVALID = -1,
    IMPROVEMENT_ERR_NOT_WORKER = -2,
    IMPROVEMENT_ERR_NO_MOVES = -3,
    IMPROVEMENT_ERR_LOCKED = -4,
    IMPROVEMENT_ERR_NOT_OWNED = -5,
    IMPROVEMENT_ERR_ALREADY = -6,
    IMPROVEMENT_ERR_TERRAIN = -7,
    IMPROVEMENT_ERR_CITY = -8
} ImprovementResult;

int Improvement_IsWorkerUnit(int unit_type);

/* Terrain/resource rule only; ignores ownership and technology. */
int Improvement_AllowedOnTile(const Map *map, int x, int y, int improvement);

int Improvement_CanBuild(const Map *map, const CityPool *cities,
                         const Unit *unit, uint32_t researched,
                         int improvement);
int Improvement_Build(Map *map, const CityPool *cities, Unit *unit,
                      uint32_t researched, int improvement);

#endif
