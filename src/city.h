#ifndef CIVPSP_CITY_H
#define CIVPSP_CITY_H

#include <stdint.h>

#include "data.h"
#include "map.h"
#include "unit.h"

#define CITY_MAX            32
#define CITY_MIN_DISTANCE   3
#define CITY_SIGHT          2
#define CITY_WORK_RADIUS    2
#define CITY_WORK_DIAMETER  5
#define CITY_WORK_TILES     25
#define CITY_CENTER_INDEX   12
#define CITY_INITIAL_RADIUS 1
#define CITY_INITIAL_TILES  9
#define CITY_MAX_QUEUE      5
#define CITY_MAX_POP        20
#define CITY_FOOD_PER_POP   3
#define CITY_GROWTH_BASE    10
#define CITY_GROWTH_PER_POP 5
#define CITY_BORDER_BASE    10
#define CITY_BORDER_STEP    5
#define CITY_TILE_BASE_COST 15
#define CITY_TILE_COST_STEP 5
#define CITY_CENTER_FOOD    2
#define CITY_CENTER_PROD    2
#define CITY_CENTER_GOLD    1
#define CITY_CENTER_SCIENCE 1
#define CITY_HP_MAX          100
#define CITY_DEFENSE_BASE    100
#define CITY_WALL_DEFENSE   150
#define CITY_HP_REGEN         10
#define CITY_REPAIR_BASE      25
#define CITY_REPAIR_WALL      15

typedef struct {
    uint8_t owner;
    uint8_t name_index;
    uint8_t population;
    int8_t x;
    int8_t y;
    uint16_t buildings;       /* bit per BuildingType */
    uint8_t queue[CITY_MAX_QUEUE];
    uint8_t queue_count;
    int16_t food_stored;
    int16_t production_stored;
    int16_t influence;
    uint32_t owned_mask;      /* 5x5 tile mask; bit 12 is the city center */
    uint32_t worked_mask;
    uint32_t locked_mask;
    uint8_t is_capital;       /* original capital; survives capture */
    /* V0.2 city combat.  Defense absorbs damage before HP; HP reaches zero
     * only after the defense pool is exhausted. */
    uint16_t hp;
    uint16_t defense;
    uint16_t defense_max;
    uint16_t last_hit_turn;
} City;

typedef struct {
    City cities[CITY_MAX];
    int count;
} CityPool;

typedef struct {
    int food;
    int production;
    int gold;
    int science;
    int influence;
    int maintenance;
    int defense;
} CityYield;

typedef struct {
    uint8_t grew;
    uint8_t shrank;
    uint8_t border_grew;
    uint8_t production_completed;
    uint8_t unit_held;
} CityTurnEvents;

typedef enum {
    CITY_FOUND_OK = 0,
    CITY_FOUND_INVALID_TILE,
    CITY_FOUND_TOO_CLOSE,
    CITY_FOUND_LIMIT
} CityFoundResult;

typedef enum {
    CITY_QUEUE_OK = 0,
    CITY_QUEUE_FULL = -1,
    CITY_QUEUE_INVALID = -2,
    CITY_QUEUE_ALREADY = -3
} CityQueueResult;

typedef enum {
    CITY_ACTION_OK = 0,
    CITY_ACTION_NO_QUEUE = -1,
    CITY_ACTION_NO_GOLD = -2,
    CITY_ACTION_NO_SPACE = -3,
    CITY_ACTION_INVALID = -4,
    CITY_ACTION_NO_IDLE = -5
} CityActionResult;

typedef enum {
    CITY_TILE_OK = 0,
    CITY_TILE_INVALID = -1,
    CITY_TILE_OWNED = -2,
    CITY_TILE_NOT_ADJACENT = -3,
    CITY_TILE_NO_GOLD = -4
} CityTileResult;

void CityPool_Init(CityPool *pool);
CityFoundResult City_CanFound(const CityPool *pool, const Map *map,
                              int x, int y);
CityFoundResult City_TryFound(CityPool *pool, const Map *map, int owner,
                              int x, int y, int *out_index);
int City_IndexOfAt(const CityPool *pool, int x, int y);
const City *City_AtConst(const CityPool *pool, int x, int y);
City *City_At(CityPool *pool, int x, int y);
void City_GrantVision(const CityPool *pool, Map *map);
void City_GrantVisionOwner(const CityPool *pool, Map *map, int owner);

/* Tile mask helpers.  The 5x5 grid is row-major, with the city center at
 * index CITY_CENTER_INDEX. */
int City_TileIndex(int dx, int dy);
int City_TileCoord(const City *city, int index, int *x, int *y);
int City_IndexOfTile(const City *city, int x, int y);
int City_OwnsIndex(const City *city, int index);
int City_OwnsTile(const City *city, int x, int y);
int City_IsWorkedIndex(const City *city, int index);
int City_IsLockedIndex(const City *city, int index);
int City_OwnedCount(const City *city);
int City_WorkedCount(const City *city);
int City_BuildingCount(const City *city);
int City_HasBuilding(const City *city, BuildingType type);

void City_ComputeYields(const City *city, const Map *map, CityYield *out);
/* M8 tech-aware variants.  The original functions remain the base-yield
 * versions used by host tests and by callers that do not have a world. */
void City_ComputeYieldsWithTech(const City *city, const Map *map,
                                uint32_t researched, CityYield *out);
int City_FoodSurplus(const City *city, const Map *map);
int City_GrowthThreshold(const City *city);
int City_BorderThreshold(const City *city);
int City_Maintenance(const City *city);
int City_Defense(const City *city);
int City_DefenseWithTech(const City *city, uint32_t researched);

/* V0.2 city combat.  Walls raise the defense-pool cap; the pool does not
 * regenerate by itself, so a worker must repair it.  HP regenerates while the
 * city has not been hit during the current turn. */
int City_DefenseMax(const City *city);
void City_ResetCombat(City *city);
void City_UpdateCombat(City *city);
void City_RecoverHP(City *city, int current_turn);
int City_RepairDefense(City *city);

/* Citizen assignment.  Auto-assignment keeps locked tiles and fills the rest
 * with the best owned tiles. */
void City_AutoAssign(City *city, const Map *map);
int City_ToggleWork(City *city, const Map *map, int index);
int City_ToggleLock(City *city, const Map *map, int index);

/* Production queue.  Queue entries are ProductionType values. */
CityQueueResult City_QueueAdd(City *city, ProductionType type);
int City_QueueRemoveLast(City *city);
int City_QueueContains(const City *city, ProductionType type);
/* Add a one-off production bonus (for example a forest chop).  The city's
 * normal end-turn processing decides when the queued item completes. */
int City_AddProduction(City *city, int amount);
int City_ProductionRemaining(const City *city);
int City_TurnsLeft(const City *city, const Map *map);
int City_TurnsLeftWithTech(const City *city, const Map *map,
                           uint32_t researched);
int City_RushCost(const City *city);
CityActionResult City_RushProduction(City *city, const Map *map,
                                     UnitPool *units, int *treasury);

/* Border growth and tile purchase.  A purchased or grown tile must be
 * adjacent to the current territory. */
int City_TilePurchaseCost(const City *city);
CityTileResult City_CanBuyTile(const City *city, const Map *map, int index);
CityTileResult City_BuyTile(City *city, Map *map, int *treasury,
                            int index);
int City_GrowBorder(City *city, Map *map);

/* Process one city at the end of a turn.  `broke` halves production when the
 * treasury was negative at the start of the turn.  Gold/science/maintenance
 * are summed by World_ProcessCities. */
void City_ProcessTurn(City *city, Map *map, UnitPool *units, int broke,
                      CityTurnEvents *events);
void City_ProcessTurnWithTech(City *city, Map *map, UnitPool *units, int broke,
                              uint32_t researched, CityTurnEvents *events);

#endif
