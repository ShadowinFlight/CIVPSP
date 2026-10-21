#include "improvement.h"

#include <stddef.h>

#include "data.h"
#include "tech.h"

static const City *owner_city_at(const CityPool *cities, int x, int y,
                                 int owner) {
    if (!cities) return NULL;
    for (int i = 0; i < cities->count; i++) {
        const City *city = &cities->cities[i];
        if (city->owner != owner) continue;
        if (City_OwnsTile(city, x, y)) return city;
    }
    return NULL;
}

int Improvement_IsWorkerUnit(int unit_type) {
    return unit_type == UNIT_WORKER;
}

int Improvement_AllowedOnTile(const Map *map, int x, int y, int improvement) {
    if (!map || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return 0;
    if (!Map_IsLand(map, x, y)) return 0;

    int terrain = Map_TerrainAt(map, x, y);
    int resource = Map_ResourceAt(map, x, y);
    switch (improvement) {
        case IMPROVEMENT_FARM:
            return terrain == TERRAIN_GRASSLAND || terrain == TERRAIN_PLAINS
                || terrain == TERRAIN_DESERT;
        case IMPROVEMENT_MINE:
            return terrain == TERRAIN_HILLS || resource == RESOURCE_IRON
                || resource == RESOURCE_GEMS;
        case IMPROVEMENT_PASTURE:
            return resource == RESOURCE_HORSES;
        case IMPROVEMENT_QUARRY:
            return resource == RESOURCE_STONE;
        case IMPROVEMENT_ROAD:
            return 1;
        default:
            return 0;
    }
}

int Improvement_CanBuild(const Map *map, const CityPool *cities,
                         const Unit *unit, uint32_t researched,
                         int improvement) {
    if (improvement <= IMPROVEMENT_NONE || improvement >= IMPROVEMENT_COUNT) {
        return IMPROVEMENT_ERR_INVALID;
    }
    if (!unit || !Improvement_IsWorkerUnit(unit->type)) {
        return IMPROVEMENT_ERR_NOT_WORKER;
    }
    if (unit->moves_left == 0) return IMPROVEMENT_ERR_NO_MOVES;

    int tech = Data_ImprovementRequiredTech(improvement);
    if (!Tech_IsKnown(researched, tech)) return IMPROVEMENT_ERR_LOCKED;

    if (!map || unit->x < 0 || unit->x >= MAP_WIDTH
        || unit->y < 0 || unit->y >= MAP_HEIGHT) {
        return IMPROVEMENT_ERR_TERRAIN;
    }
    int x = unit->x;
    int y = unit->y;
    if (!Map_IsLand(map, x, y)) return IMPROVEMENT_ERR_TERRAIN;
    if (City_IndexOfAt(cities, x, y) >= 0) return IMPROVEMENT_ERR_CITY;

    const MapTile *tile = &map->tiles[y][x];
    if (tile->improvement != IMPROVEMENT_NONE) {
        return IMPROVEMENT_ERR_ALREADY;
    }
    if (!owner_city_at(cities, x, y, unit->owner)) {
        return IMPROVEMENT_ERR_NOT_OWNED;
    }
    if (!Improvement_AllowedOnTile(map, x, y, improvement)) {
        return IMPROVEMENT_ERR_TERRAIN;
    }
    return IMPROVEMENT_OK;
}

int Improvement_Build(Map *map, const CityPool *cities, Unit *unit,
                      uint32_t researched, int improvement) {
    int result = Improvement_CanBuild(map, cities, unit, researched,
                                      improvement);
    if (result != IMPROVEMENT_OK) return result;

    map->tiles[unit->y][unit->x].improvement = (uint8_t)improvement;
    unit->moves_left = 0;
    return IMPROVEMENT_OK;
}
