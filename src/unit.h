#ifndef CIVPSP_UNIT_H
#define CIVPSP_UNIT_H

#include <stdint.h>

#include "map.h"

#define UNIT_MAX 64
#define UNIT_HP_MAX 100

typedef enum {
    UNIT_SETTLER = 0,
    UNIT_WARRIOR,
    UNIT_SCOUT,
    UNIT_SWORDSMAN,
    UNIT_HORSEMAN,
    UNIT_WORKER,
    UNIT_TYPE_COUNT
} UnitType;

typedef struct {
    uint8_t type;
    uint8_t owner;
    uint8_t moves_left;
    int8_t x;
    int8_t y;
    /* Hit points and per-turn AI bookkeeping are appended so older five-field
     * initializers still compile; Unit_Spawn always fills both fields. */
    uint8_t hp;
    uint8_t acted;
} Unit;

typedef struct {
    Unit units[UNIT_MAX];
    int count;
} UnitPool;

typedef struct {
    uint8_t move_points;
    uint8_t sight;
    uint8_t strength;
    uint8_t can_found_city;
} UnitDef;

extern const UnitDef g_unit_defs[UNIT_TYPE_COUNT];

void UnitPool_Init(UnitPool *pool);
int Unit_Spawn(UnitPool *pool, int type, int owner, int x, int y);
int Unit_IndexOfAt(const UnitPool *pool, int x, int y);
const Unit *Unit_AtConst(const UnitPool *pool, int x, int y);
Unit *Unit_At(UnitPool *pool, int x, int y);
void Unit_RemoveAt(UnitPool *pool, int index);

/* Half-tile movement points: straight 2, diagonal 3, +2 entering forest or
 * hills.  Mountains and water are never passable. */
int Unit_PassableTile(const Map *map, int x, int y);
int Unit_StepCost(const Map *map, int from_x, int from_y, int to_x, int to_y);

/* First step of an A* path toward a target.  When the target tile is
 * occupied, a successful search stops on a tile adjacent to it (so AI units
 * can approach an enemy, then attack).  Returns 1 and writes the next tile,
 * 0 when the unit is already at/adjacent to the goal, or -1 when no path
 * exists. */
int Unit_PathNextStep(const UnitPool *pool, const Map *map, int unit_index,
                      int tx, int ty, int *next_x, int *next_y);

/* 8-way Dijkstra limited to the unit's remaining movement points.  Terrain
 * and friendly unit stacking are respected; the unit's own tile is allowed as
 * the destination (cost 0). */
int Unit_PathCost(const UnitPool *pool, const Map *map, const Unit *unit,
                  int tx, int ty, int max_points);
int Unit_MoveTo(UnitPool *pool, const Map *map, int index, int tx, int ty);

void Unit_ResetTurn(UnitPool *pool);
void Unit_GrantVision(const UnitPool *pool, Map *map);
void Unit_GrantVisionOwner(const UnitPool *pool, Map *map, int owner);

int Unit_TypeMovePoints(int type);
int Unit_TypeSight(int type);

#endif
