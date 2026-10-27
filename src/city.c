#include "city.h"

#include <string.h>

#define CITY_STORE_MAX 30000

static int popcount32(uint32_t value) {
    int count = 0;
    while (value) {
        count += (int)(value & 1u);
        value >>= 1;
    }
    return count;
}

int City_TileIndex(int dx, int dy) {
    if (dx < -CITY_WORK_RADIUS || dx > CITY_WORK_RADIUS) return -1;
    if (dy < -CITY_WORK_RADIUS || dy > CITY_WORK_RADIUS) return -1;
    return (dy + CITY_WORK_RADIUS) * CITY_WORK_DIAMETER
         + (dx + CITY_WORK_RADIUS);
}

int City_TileCoord(const City *city, int index, int *x, int *y) {
    if (!city || index < 0 || index >= CITY_WORK_TILES) return 0;
    if (x) *x = city->x + index % CITY_WORK_DIAMETER - CITY_WORK_RADIUS;
    if (y) *y = city->y + index / CITY_WORK_DIAMETER - CITY_WORK_RADIUS;
    return 1;
}

int City_IndexOfTile(const City *city, int x, int y) {
    if (!city) return -1;
    int index = City_TileIndex(x - city->x, y - city->y);
    return index;
}

int City_OwnsIndex(const City *city, int index) {
    if (!city || index < 0 || index >= CITY_WORK_TILES) return 0;
    return (city->owned_mask & (1u << index)) != 0;
}

int City_OwnsTile(const City *city, int x, int y) {
    int index = City_IndexOfTile(city, x, y);
    return index >= 0 && City_OwnsIndex(city, index);
}

int City_IsWorkedIndex(const City *city, int index) {
    if (!city || index < 0 || index >= CITY_WORK_TILES) return 0;
    return (city->worked_mask & (1u << index)) != 0;
}

int City_IsLockedIndex(const City *city, int index) {
    if (!city || index < 0 || index >= CITY_WORK_TILES) return 0;
    return (city->locked_mask & (1u << index)) != 0;
}

int City_OwnedCount(const City *city) {
    return city ? popcount32(city->owned_mask) : 0;
}

int City_WorkedCount(const City *city) {
    return city ? popcount32(city->worked_mask) : 0;
}

int City_BuildingCount(const City *city) {
    return city ? popcount32(city->buildings) : 0;
}

int City_HasBuilding(const City *city, BuildingType type) {
    if (!city || type < 0 || type >= BUILDING_COUNT) return 0;
    return (city->buildings & (1u << type)) != 0;
}

static int tile_score(const Map *map, int x, int y) {
    Yield yield;
    Data_TileYield(map, x, y, &yield);
    int score = yield.food * 5 + yield.production * 3 + yield.gold * 2;
    const MapTile *tile = (map && x >= 0 && x < MAP_WIDTH
                           && y >= 0 && y < MAP_HEIGHT)
                              ? &map->tiles[y][x] : NULL;
    if (tile && tile->resource != RESOURCE_NONE) score += 2;
    return score;
}

static int tile_adjacent_to_owned(const City *city, int index) {
    int x = 0;
    int y = 0;
    if (!City_TileCoord(city, index, &x, &y)) return 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int ni = City_TileIndex(x - city->x + dx, y - city->y + dy);
            if (ni >= 0 && City_OwnsIndex(city, ni)) return 1;
        }
    }
    return 0;
}

static void assign_best_tile(City *city, const Map *map) {
    if (!city || !map) return;
    if (City_WorkedCount(city) >= city->population) return;
    int best = -1;
    int best_score = -1;
    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (i == CITY_CENTER_INDEX) continue;
        if (!City_OwnsIndex(city, i)) continue;
        if (City_IsWorkedIndex(city, i)) continue;
        int x = 0;
        int y = 0;
        City_TileCoord(city, i, &x, &y);
        int score = tile_score(map, x, y);
        if (score > best_score) {
            best = i;
            best_score = score;
        }
    }
    if (best >= 0) city->worked_mask |= (1u << best);
}

static void unassign_worst_tile(City *city, const Map *map) {
    if (!city) return;
    int best = -1;
    int best_score = 0x7FFFFFFF;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < CITY_WORK_TILES; i++) {
            if (i == CITY_CENTER_INDEX) continue;
            if (!City_IsWorkedIndex(city, i)) continue;
            int locked = City_IsLockedIndex(city, i);
            if (pass == 0 && locked) continue;
            if (pass == 1 && !locked) continue;
            int x = 0;
            int y = 0;
            City_TileCoord(city, i, &x, &y);
            int score = tile_score(map, x, y);
            if (score < best_score) {
                best = i;
                best_score = score;
            }
        }
        if (best >= 0) break;
    }
    if (best >= 0) {
        city->worked_mask &= ~(1u << best);
        city->locked_mask &= ~(1u << best);
    }
}

void City_AutoAssign(City *city, const Map *map) {
    if (!city || !map) return;

    for (int i = 0; i < CITY_WORK_TILES; i++) {
        int owned = City_OwnsIndex(city, i);
        if (!owned || i == CITY_CENTER_INDEX) {
            city->worked_mask &= ~(1u << i);
            city->locked_mask &= ~(1u << i);
        }
    }

    int locked = popcount32(city->locked_mask);
    while (locked > city->population) {
        int worst = -1;
        int worst_score = 0x7FFFFFFF;
        for (int i = 0; i < CITY_WORK_TILES; i++) {
            if (!City_IsLockedIndex(city, i)) continue;
            int x = 0;
            int y = 0;
            City_TileCoord(city, i, &x, &y);
            int score = tile_score(map, x, y);
            if (score < worst_score) {
                worst = i;
                worst_score = score;
            }
        }
        if (worst < 0) break;
        city->locked_mask &= ~(1u << worst);
        locked--;
    }

    city->worked_mask = city->locked_mask;
    while (City_WorkedCount(city) < city->population) {
        int before = City_WorkedCount(city);
        assign_best_tile(city, map);
        if (City_WorkedCount(city) == before) break;
    }
}

int City_ToggleWork(City *city, const Map *map, int index) {
    (void)map;
    if (!city || index < 0 || index >= CITY_WORK_TILES
        || index == CITY_CENTER_INDEX || !City_OwnsIndex(city, index)) {
        return CITY_ACTION_INVALID;
    }
    if (City_IsWorkedIndex(city, index)) {
        city->worked_mask &= ~(1u << index);
        city->locked_mask &= ~(1u << index);
        return CITY_ACTION_OK;
    }
    if (City_WorkedCount(city) >= city->population) {
        return CITY_ACTION_NO_IDLE;
    }
    city->worked_mask |= (1u << index);
    return CITY_ACTION_OK;
}

int City_ToggleLock(City *city, const Map *map, int index) {
    (void)map;
    if (!city || index < 0 || index >= CITY_WORK_TILES
        || index == CITY_CENTER_INDEX || !City_OwnsIndex(city, index)) {
        return CITY_ACTION_INVALID;
    }
    if (City_IsLockedIndex(city, index)) {
        city->locked_mask &= ~(1u << index);
        return CITY_ACTION_OK;
    }
    if (!City_IsWorkedIndex(city, index)) {
        if (City_WorkedCount(city) >= city->population) {
            return CITY_ACTION_NO_IDLE;
        }
        city->worked_mask |= (1u << index);
    }
    city->locked_mask |= (1u << index);
    return CITY_ACTION_OK;
}

void City_ComputeYields(const City *city, const Map *map, CityYield *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!city || !map) return;

    out->food = CITY_CENTER_FOOD;
    out->production = CITY_CENTER_PROD;
    out->gold = CITY_CENTER_GOLD;
    out->science = CITY_CENTER_SCIENCE;

    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (i == CITY_CENTER_INDEX) continue;
        if (!City_IsWorkedIndex(city, i) || !City_OwnsIndex(city, i)) continue;
        int x = 0;
        int y = 0;
        City_TileCoord(city, i, &x, &y);
        Yield tile;
        Data_TileYield(map, x, y, &tile);
        out->food += tile.food;
        out->production += tile.production;
        out->gold += tile.gold;
        out->science += tile.science;
    }

    for (int i = 0; i < BUILDING_COUNT; i++) {
        if (!City_HasBuilding(city, (BuildingType)i)) continue;
        const ProductionDef *def = Data_BuildingDef((BuildingType)i);
        if (!def) continue;
        out->food += def->food;
        out->production += def->production;
        out->gold += def->gold;
        out->science += def->science;
        out->influence += def->influence;
        out->maintenance += def->maintenance;
        out->defense += def->defense;
    }
    out->influence += 1;
    out->defense += 1;
}

void City_ComputeYieldsWithTech(const City *city, const Map *map,
                                uint32_t researched, CityYield *out) {
    City_ComputeYields(city, map, out);
    if (!city || !map || !out) return;

    for (int tech = 1; tech < TECH_COUNT; tech++) {
        if (!Tech_IsKnown(researched, tech)) continue;
        int value = Tech_EffectValue(tech);
        if (value <= 0) continue;

        switch (Tech_Effect(tech)) {
            case TECH_EFFECT_FARM_FOOD:
                for (int i = 0; i < CITY_WORK_TILES; i++) {
                    if (i == CITY_CENTER_INDEX) continue;
                    if (!City_IsWorkedIndex(city, i)
                        || !City_OwnsIndex(city, i)) {
                        continue;
                    }
                    int x = 0;
                    int y = 0;
                    City_TileCoord(city, i, &x, &y);
                    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
                        continue;
                    }
                    if (map->tiles[y][x].improvement == IMPROVEMENT_FARM) {
                        out->food += value;
                    }
                }
                break;
            case TECH_EFFECT_LIBRARY_SCIENCE:
                if (City_HasBuilding(city, BUILDING_LIBRARY)) {
                    out->science += value;
                }
                break;
            case TECH_EFFECT_UNIVERSITY_SCIENCE:
                if (City_HasBuilding(city, BUILDING_UNIVERSITY)) {
                    out->science += value;
                }
                break;
            case TECH_EFFECT_MARKET_GOLD:
                if (City_HasBuilding(city, BUILDING_MARKET)) {
                    out->gold += value;
                }
                break;
            case TECH_EFFECT_WORKSHOP_PROD:
                if (City_HasBuilding(city, BUILDING_WORKSHOP)) {
                    out->production += value;
                }
                break;
            case TECH_EFFECT_BARRACKS_PROD:
                if (City_HasBuilding(city, BUILDING_BARRACKS)) {
                    out->production += value;
                }
                break;
            case TECH_EFFECT_WALL_DEFENSE:
                if (City_HasBuilding(city, BUILDING_WALLS)) {
                    out->defense += value;
                }
                break;
            case TECH_EFFECT_EMPIRE_GOLD:
                out->gold += value;
                break;
            case TECH_EFFECT_EMPIRE_SCIENCE:
                out->science += value;
                break;
            default:
                break;
        }
    }
}

int City_FoodSurplus(const City *city, const Map *map) {
    if (!city) return 0;
    CityYield yield;
    City_ComputeYields(city, map, &yield);
    return yield.food - (int)city->population * CITY_FOOD_PER_POP;
}

int City_GrowthThreshold(const City *city) {
    int pop = city ? city->population : 1;
    return CITY_GROWTH_BASE + CITY_GROWTH_PER_POP * pop;
}

int City_BorderThreshold(const City *city) {
    int owned = City_OwnedCount(city);
    int extra = owned > CITY_INITIAL_TILES ? owned - CITY_INITIAL_TILES : 0;
    return CITY_BORDER_BASE + CITY_BORDER_STEP * extra;
}

int City_Maintenance(const City *city) {
    if (!city) return 0;
    int total = 0;
    for (int i = 0; i < BUILDING_COUNT; i++) {
        if (!City_HasBuilding(city, (BuildingType)i)) continue;
        const ProductionDef *def = Data_BuildingDef((BuildingType)i);
        if (def) total += def->maintenance;
    }
    return total;
}

int City_Defense(const City *city) {
    if (!city) return 0;
    int total = 1;
    for (int i = 0; i < BUILDING_COUNT; i++) {
        if (!City_HasBuilding(city, (BuildingType)i)) continue;
        const ProductionDef *def = Data_BuildingDef((BuildingType)i);
        if (def) total += def->defense;
    }
    return total;
}

int City_DefenseWithTech(const City *city, uint32_t researched) {
    int total = City_Defense(city);
    if (!city || !City_HasBuilding(city, BUILDING_WALLS)) return total;
    for (int tech = 1; tech < TECH_COUNT; tech++) {
        if (!Tech_IsKnown(researched, tech)) continue;
        if (Tech_Effect(tech) == TECH_EFFECT_WALL_DEFENSE) {
            total += Tech_EffectValue(tech);
        }
    }
    return total;
}

int City_DefenseMax(const City *city) {
    if (!city) return CITY_DEFENSE_BASE;
    return City_HasBuilding(city, BUILDING_WALLS)
               ? CITY_WALL_DEFENSE : CITY_DEFENSE_BASE;
}

void City_ResetCombat(City *city) {
    if (!city) return;
    city->hp = CITY_HP_MAX;
    city->defense_max = (uint16_t)City_DefenseMax(city);
    city->defense = city->defense_max;
    city->last_hit_turn = 0;
}

void City_UpdateCombat(City *city) {
    if (!city) return;
    /* Fixtures and converted legacy saves may predate the combat fields. */
    if (city->hp == 0 && city->defense == 0 && city->defense_max == 0) {
        City_ResetCombat(city);
    }
    int max = City_DefenseMax(city);
    if (city->defense_max != (uint16_t)max) {
        city->defense_max = (uint16_t)max;
        if (city->defense > city->defense_max) {
            city->defense = city->defense_max;
        }
    }
    if (city->hp > CITY_HP_MAX) city->hp = CITY_HP_MAX;
}

/* Called once at the end of a turn, after all player and AI attacks.  A city
 * recovers only when it was not hit during that turn. */
void City_RecoverHP(City *city, int current_turn) {
    if (!city || current_turn <= 0) return;
    if (city->last_hit_turn >= (uint16_t)current_turn) return;
    if (city->hp >= CITY_HP_MAX) return;
    int hp = (int)city->hp + CITY_HP_REGEN;
    city->hp = (uint16_t)(hp > CITY_HP_MAX ? CITY_HP_MAX : hp);
}

int City_RepairDefense(City *city) {
    if (!city) return 0;
    int max = City_DefenseMax(city);
    city->defense_max = (uint16_t)max;
    if (city->defense >= city->defense_max) return 0;
    int amount = City_HasBuilding(city, BUILDING_WALLS)
                     ? CITY_REPAIR_WALL : CITY_REPAIR_BASE;
    int defense = (int)city->defense + amount;
    if (defense > city->defense_max) defense = city->defense_max;
    int repaired = defense - city->defense;
    city->defense = (uint16_t)defense;
    return repaired;
}

static const ProductionDef *queue_front(const City *city) {
    if (!city || city->queue_count == 0) return NULL;
    return Data_ProductionDef((ProductionType)city->queue[0]);
}

int City_AddProduction(City *city, int amount) {
    if (!city || amount <= 0) return 0;
    int before = city->production_stored;
    int next = before + amount;
    if (next > CITY_STORE_MAX) next = CITY_STORE_MAX;
    if (next < before) next = before;
    city->production_stored = (int16_t)next;
    return next - before;
}

int City_ProductionRemaining(const City *city) {
    const ProductionDef *def = queue_front(city);
    if (!def) return -1;
    int remaining = (int)def->cost - city->production_stored;
    return remaining > 0 ? remaining : 0;
}

int City_TurnsLeft(const City *city, const Map *map) {
    if (!city || city->queue_count == 0) return -1;
    int production = 0;
    CityYield yield;
    City_ComputeYields(city, map, &yield);
    production = yield.production;
    if (production <= 0) return -1;
    int remaining = City_ProductionRemaining(city);
    if (remaining <= 0) return 0;
    return (remaining + production - 1) / production;
}

int City_TurnsLeftWithTech(const City *city, const Map *map,
                           uint32_t researched) {
    if (!city || city->queue_count == 0) return -1;
    CityYield yield;
    City_ComputeYieldsWithTech(city, map, researched, &yield);
    int production = yield.production;
    if (production <= 0) return -1;
    int remaining = City_ProductionRemaining(city);
    if (remaining <= 0) return 0;
    return (remaining + production - 1) / production;
}

int City_RushCost(const City *city) {
    if (!city || city->queue_count == 0) return 0;
    const ProductionDef *def = queue_front(city);
    if (!def) return 0;
    int remaining = (int)def->cost - city->production_stored;
    if (remaining <= 0) return 0;
    int gold = remaining * 2;
    return gold < 5 ? 5 : gold;
}

CityQueueResult City_QueueAdd(City *city, ProductionType type) {
    if (!city || type <= PROD_NONE || type >= PROD_COUNT) {
        return CITY_QUEUE_INVALID;
    }
    if (city->queue_count >= CITY_MAX_QUEUE) return CITY_QUEUE_FULL;
    const ProductionDef *def = Data_ProductionDef(type);
    if (!def) return CITY_QUEUE_INVALID;
    if (def->kind == PROD_KIND_BUILDING) {
        if (City_HasBuilding(city, (BuildingType)def->building_type)) {
            return CITY_QUEUE_ALREADY;
        }
        if (City_QueueContains(city, type)) return CITY_QUEUE_ALREADY;
    }
    city->queue[city->queue_count++] = (uint8_t)type;
    return CITY_QUEUE_OK;
}

int City_QueueRemoveLast(City *city) {
    if (!city || city->queue_count == 0) return -1;
    city->queue_count--;
    if (city->queue_count == 0) city->production_stored = 0;
    return 0;
}

int City_QueueContains(const City *city, ProductionType type) {
    if (!city) return 0;
    for (int i = 0; i < city->queue_count; i++) {
        if (city->queue[i] == (uint8_t)type) return 1;
    }
    return 0;
}

static void remove_front(City *city) {
    if (!city || city->queue_count == 0) return;
    for (int i = 0; i < city->queue_count - 1; i++) {
        city->queue[i] = city->queue[i + 1];
    }
    city->queue_count--;
}

static int spawn_unit_for_city(City *city, const Map *map, UnitPool *units,
                               int unit_type) {
    if (!city || !map || !units) return -1;
    static const int order[9][2] = {
        { 0, 0 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 },
        { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 },
    };
    for (int i = 0; i < 9; i++) {
        int x = city->x + order[i][0];
        int y = city->y + order[i][1];
        if (!Unit_PassableTile(map, x, y)) continue;
        if (Unit_IndexOfAt(units, x, y) >= 0) continue;
        int index = Unit_Spawn(units, unit_type, city->owner, x, y);
        if (index >= 0) {
            units->units[index].moves_left = 0;
            return index;
        }
    }
    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (!City_OwnsIndex(city, i)) continue;
        int x = 0;
        int y = 0;
        City_TileCoord(city, i, &x, &y);
        if (!Unit_PassableTile(map, x, y)) continue;
        if (Unit_IndexOfAt(units, x, y) >= 0) continue;
        int index = Unit_Spawn(units, unit_type, city->owner, x, y);
        if (index >= 0) {
            units->units[index].moves_left = 0;
            return index;
        }
    }
    return -1;
}

/* Return 1 when the front item was completed, 0 when a unit could not be
 * placed (the completed item stays at the front for a later turn). */
static int complete_front(City *city, const Map *map, UnitPool *units) {
    const ProductionDef *def = queue_front(city);
    if (!def) return 0;
    if (def->kind == PROD_KIND_BUILDING) {
        if (def->building_type < BUILDING_COUNT) {
            city->buildings |= (uint16_t)(1u << def->building_type);
        }
        remove_front(city);
        return 1;
    }
    if (spawn_unit_for_city(city, map, units, def->unit_type) < 0) return 0;
    remove_front(city);
    return 1;
}

CityActionResult City_RushProduction(City *city, const Map *map,
                                     UnitPool *units, int *treasury) {
    if (!city || !map || !units || !treasury) return CITY_ACTION_INVALID;
    const ProductionDef *def = queue_front(city);
    if (!def) return CITY_ACTION_NO_QUEUE;
    int gold = City_RushCost(city);
    if (*treasury < gold) return CITY_ACTION_NO_GOLD;

    if (def->kind == PROD_KIND_UNIT) {
        int can_place = 0;
        static const int order[9][2] = {
            { 0, 0 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 },
            { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 },
        };
        for (int i = 0; i < 9 && !can_place; i++) {
            int x = city->x + order[i][0];
            int y = city->y + order[i][1];
            if (Unit_PassableTile(map, x, y)
                && Unit_IndexOfAt(units, x, y) < 0) {
                can_place = 1;
            }
        }
        for (int i = 0; i < CITY_WORK_TILES && !can_place; i++) {
            if (!City_OwnsIndex(city, i)) continue;
            int x = 0;
            int y = 0;
            City_TileCoord(city, i, &x, &y);
            if (Unit_PassableTile(map, x, y)
                && Unit_IndexOfAt(units, x, y) < 0) {
                can_place = 1;
            }
        }
        if (!can_place) return CITY_ACTION_NO_SPACE;
    }

    *treasury -= gold;
    city->production_stored = (int16_t)def->cost;
    int completed = complete_front(city, map, units);
    city->production_stored = 0;
    if (!completed) {
        *treasury += gold;
        return CITY_ACTION_NO_SPACE;
    }
    return CITY_ACTION_OK;
}

int City_TilePurchaseCost(const City *city) {
    if (!city) return 0;
    int owned = City_OwnedCount(city);
    int extra = owned > CITY_INITIAL_TILES ? owned - CITY_INITIAL_TILES : 0;
    return CITY_TILE_BASE_COST + CITY_TILE_COST_STEP * extra;
}

CityTileResult City_CanBuyTile(const City *city, const Map *map, int index) {
    if (!city || !map || index < 0 || index >= CITY_WORK_TILES
        || index == CITY_CENTER_INDEX) {
        return CITY_TILE_INVALID;
    }
    int x = 0;
    int y = 0;
    City_TileCoord(city, index, &x, &y);
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
        return CITY_TILE_INVALID;
    }
    if (City_OwnsIndex(city, index)) return CITY_TILE_OWNED;
    if (!tile_adjacent_to_owned(city, index)) return CITY_TILE_NOT_ADJACENT;
    return CITY_TILE_OK;
}

CityTileResult City_BuyTile(City *city, Map *map, int *treasury,
                            int index) {
    if (!treasury) return CITY_TILE_INVALID;
    CityTileResult result = City_CanBuyTile(city, map, index);
    if (result != CITY_TILE_OK) return result;
    int cost = City_TilePurchaseCost(city);
    if (*treasury < cost) return CITY_TILE_NO_GOLD;
    *treasury -= cost;
    city->owned_mask |= (1u << index);
    int tx = 0;
    int ty = 0;
    if (City_TileCoord(city, index, &tx, &ty)) {
        MapTile *tile = Map_TileAt(map, tx, ty);
        if (tile) {
            tile->explored = 1;
            tile->visible = 1;
        }
    }
    if (City_WorkedCount(city) < city->population) {
        assign_best_tile(city, map);
    }
    return CITY_TILE_OK;
}

int City_GrowBorder(City *city, Map *map) {
    if (!city || !map) return 0;
    CityYield yield;
    City_ComputeYields(city, map, &yield);
    city->influence = (int16_t)(city->influence + yield.influence);

    if (City_OwnedCount(city) >= CITY_WORK_TILES) {
        int threshold = City_BorderThreshold(city);
        if (city->influence > threshold) city->influence = (int16_t)threshold;
        return 0;
    }

    int threshold = City_BorderThreshold(city);
    if (city->influence < threshold) return 0;

    int best = -1;
    int best_score = -1;
    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (City_CanBuyTile(city, map, i) != CITY_TILE_OK) continue;
        int x = 0;
        int y = 0;
        City_TileCoord(city, i, &x, &y);
        int score = tile_score(map, x, y);
        if (score > best_score) {
            best = i;
            best_score = score;
        }
    }
    if (best < 0) {
        city->influence = (int16_t)threshold;
        return 0;
    }

    city->owned_mask |= (1u << best);
    int tx = 0;
    int ty = 0;
    if (City_TileCoord(city, best, &tx, &ty)) {
        MapTile *tile = Map_TileAt(map, tx, ty);
        if (tile) {
            tile->explored = 1;
            tile->visible = 1;
        }
    }
    city->influence = (int16_t)(city->influence - threshold);
    if (City_WorkedCount(city) < city->population) {
        assign_best_tile(city, map);
    }
    return 1;
}

void City_ProcessTurn(City *city, Map *map, UnitPool *units, int broke,
                      CityTurnEvents *events) {
    City_ProcessTurnWithTech(city, map, units, broke, 0, events);
}

void City_ProcessTurnWithTech(City *city, Map *map, UnitPool *units, int broke,
                              uint32_t researched, CityTurnEvents *events) {
    CityTurnEvents local;
    if (!events) events = &local;
    memset(events, 0, sizeof(*events));
    if (!city || !map || !units) return;

    CityYield yield;
    City_ComputeYieldsWithTech(city, map, researched, &yield);

    int surplus = yield.food - (int)city->population * CITY_FOOD_PER_POP;
    int stored = (int)city->food_stored + surplus;
    if (stored > CITY_STORE_MAX) stored = CITY_STORE_MAX;
    if (stored < -CITY_STORE_MAX) stored = -CITY_STORE_MAX;
    city->food_stored = (int16_t)stored;

    while (city->population < CITY_MAX_POP
           && city->food_stored >= City_GrowthThreshold(city)) {
        city->food_stored = (int16_t)(city->food_stored
                                      - City_GrowthThreshold(city));
        city->population++;
        assign_best_tile(city, map);
        events->grew = 1;
    }
    if (city->population >= CITY_MAX_POP
        && city->food_stored > City_GrowthThreshold(city)) {
        city->food_stored = (int16_t)City_GrowthThreshold(city);
    }
    while (city->food_stored < 0 && city->population > 1) {
        city->population--;
        city->food_stored = 0;
        unassign_worst_tile(city, map);
        events->shrank = 1;
    }
    if (city->food_stored < 0) city->food_stored = 0;

    if (city->queue_count == 0) {
        city->production_stored = 0;
    } else {
        int amount = yield.production;
        if (broke) amount /= 2;
        int next = (int)city->production_stored + amount;
        if (next > CITY_STORE_MAX) next = CITY_STORE_MAX;
        city->production_stored = (int16_t)next;

        for (int guard = 0; guard < CITY_MAX_QUEUE + 4; guard++) {
            const ProductionDef *def = queue_front(city);
            if (!def) break;
            if (city->production_stored < def->cost) break;

            if (def->kind == PROD_KIND_BUILDING
                && def->building_type < BUILDING_COUNT
                && City_HasBuilding(city, (BuildingType)def->building_type)) {
                remove_front(city);
                city->production_stored = 0;
                continue;
            }

            if (!complete_front(city, map, units)) {
                events->unit_held = 1;
                break;
            }
            city->production_stored = (int16_t)(city->production_stored
                                                - def->cost);
            events->production_completed = 1;
            if (city->queue_count == 0) {
                city->production_stored = 0;
                break;
            }
        }
    }

    /* A completed wall changes the defense-pool cap immediately; the cap
     * update must not turn into free healing, so pass turn 0 here. */
    City_UpdateCombat(city);
    if (City_GrowBorder(city, map)) events->border_grew = 1;
}

void CityPool_Init(CityPool *pool) {
    memset(pool, 0, sizeof(*pool));
}

CityFoundResult City_CanFound(const CityPool *pool, const Map *map,
                              int x, int y) {
    if (!pool || !map || pool->count >= CITY_MAX) return CITY_FOUND_LIMIT;
    if (!Map_IsLand(map, x, y)) return CITY_FOUND_INVALID_TILE;
    if (Map_TerrainAt(map, x, y) == TERRAIN_MOUNTAIN) {
        return CITY_FOUND_INVALID_TILE;
    }

    for (int i = 0; i < pool->count; i++) {
        const City *city = &pool->cities[i];
        int dx = city->x - x;
        int dy = city->y - y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        int distance = dx > dy ? dx : dy;
        if (distance < CITY_MIN_DISTANCE) return CITY_FOUND_TOO_CLOSE;
    }
    return CITY_FOUND_OK;
}

static int city_name_base(int owner) {
    /* Each civilization starts with its own recognizable capital name while
     * the player keeps the original AURELIA.  Names may repeat later when a
     * civ has founded more than the ten-name table can cover. */
    switch (owner) {
        case 1: return 3;
        case 2: return 6;
        case 3: return 8;
        case 4: return 9;
        default: return 0;
    }
}

static void init_city_state(City *city, const Map *map) {
    city->population = 1;
    city->buildings = 0;
    city->queue_count = 0;
    city->food_stored = 0;
    city->production_stored = 0;
    city->influence = 0;
    city->owned_mask = 0;
    city->worked_mask = 0;
    city->locked_mask = 0;
    memset(city->queue, 0, sizeof(city->queue));
    City_ResetCombat(city);

    for (int dy = -CITY_INITIAL_RADIUS; dy <= CITY_INITIAL_RADIUS; dy++) {
        for (int dx = -CITY_INITIAL_RADIUS; dx <= CITY_INITIAL_RADIUS; dx++) {
            int x = city->x + dx;
            int y = city->y + dy;
            if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) continue;
            int index = City_TileIndex(dx, dy);
            if (index >= 0) city->owned_mask |= (1u << index);
        }
    }
    assign_best_tile(city, map);
}

CityFoundResult City_TryFound(CityPool *pool, const Map *map, int owner,
                              int x, int y, int *out_index) {
    CityFoundResult result = City_CanFound(pool, map, x, y);
    if (result != CITY_FOUND_OK) return result;

    int local_count = 0;
    for (int i = 0; i < pool->count; i++) {
        if (pool->cities[i].owner == owner) local_count++;
    }

    int index = pool->count++;
    City *city = &pool->cities[index];
    memset(city, 0, sizeof(*city));
    city->owner = (uint8_t)owner;
    city->name_index = (uint8_t)((city_name_base(owner) + local_count)
                                 % CITY_NAME_COUNT);
    city->x = (int8_t)x;
    city->y = (int8_t)y;
    init_city_state(city, map);
    if (out_index) *out_index = index;
    return CITY_FOUND_OK;
}

int City_IndexOfAt(const CityPool *pool, int x, int y) {
    if (!pool) return -1;
    for (int i = 0; i < pool->count; i++) {
        if (pool->cities[i].x == x && pool->cities[i].y == y) return i;
    }
    return -1;
}

const City *City_AtConst(const CityPool *pool, int x, int y) {
    int index = City_IndexOfAt(pool, x, y);
    return index >= 0 ? &pool->cities[index] : NULL;
}

City *City_At(CityPool *pool, int x, int y) {
    int index = City_IndexOfAt(pool, x, y);
    return index >= 0 ? &pool->cities[index] : NULL;
}

void City_GrantVision(const CityPool *pool, Map *map) {
    if (!pool || !map) return;
    for (int i = 0; i < pool->count; i++) {
        const City *city = &pool->cities[i];
        /* A city reveals its whole work radius, including the four corners
         * of the 5x5 area.  Circular unit sight is deliberately different,
         * but the city screen must not show owned work tiles as black. */
        for (int dy = -CITY_WORK_RADIUS; dy <= CITY_WORK_RADIUS; dy++) {
            for (int dx = -CITY_WORK_RADIUS; dx <= CITY_WORK_RADIUS; dx++) {
                MapTile *tile = Map_TileAt(map, city->x + dx, city->y + dy);
                if (!tile) continue;
                tile->explored = 1;
                tile->visible = 1;
            }
        }
    }
}

void City_GrantVisionOwner(const CityPool *pool, Map *map, int owner) {
    if (!pool || !map) return;
    for (int i = 0; i < pool->count; i++) {
        const City *city = &pool->cities[i];
        if (city->owner != owner) continue;
        for (int dy = -CITY_WORK_RADIUS; dy <= CITY_WORK_RADIUS; dy++) {
            for (int dx = -CITY_WORK_RADIUS; dx <= CITY_WORK_RADIUS; dx++) {
                MapTile *tile = Map_TileAt(map, city->x + dx, city->y + dy);
                if (!tile) continue;
                tile->explored = 1;
                tile->visible = 1;
            }
        }
    }
}
