#include "data.h"

#include <stddef.h>

/* Central rules table.  Keep balance changes here instead of scattering
 * constants through the city logic or the views. */
const Yield g_terrain_yields[TERRAIN_COUNT] = {
    [TERRAIN_GRASSLAND] = { 2, 0, 1, 0 },
    [TERRAIN_PLAINS]    = { 1, 1, 2, 0 },
    [TERRAIN_FOREST]    = { 1, 2, 1, 0 },
    [TERRAIN_HILLS]     = { 0, 2, 0, 0 },
    [TERRAIN_MOUNTAIN]  = { 0, 1, 0, 0 },
    [TERRAIN_DESERT]    = { 0, 0, 1, 0 },
    [TERRAIN_TUNDRA]    = { 1, 0, 0, 0 },
    [TERRAIN_COAST]     = { 2, 0, 1, 0 },
    [TERRAIN_OCEAN]     = { 1, 0, 1, 0 },
};

const Yield g_resource_yields[RESOURCE_COUNT] = {
    [RESOURCE_NONE]   = { 0, 0, 0, 0 },
    [RESOURCE_HORSES] = { 0, 1, 0, 0 },
    [RESOURCE_IRON]   = { 0, 1, 0, 0 },
    [RESOURCE_STONE]  = { 0, 1, 0, 0 },
    [RESOURCE_GEMS]   = { 0, 0, 2, 0 },
};

const Yield g_improvement_yields[IMPROVEMENT_COUNT] = {
    [IMPROVEMENT_NONE]     = { 0, 0, 0, 0 },
    [IMPROVEMENT_FARM]     = { 2, 0, 1, 0 },
    [IMPROVEMENT_MINE]     = { 0, 1, 2, 1 },
    [IMPROVEMENT_PASTURE]  = { 1, 0, 1, 0 },
    [IMPROVEMENT_QUARRY]   = { 0, 1, 1, 1 },
    [IMPROVEMENT_ROAD]     = { 0, 0, 0, 0 },
};

const ProductionDef g_production_defs[PROD_COUNT] = {
    [PROD_NONE] = {
        S_CITY_NO_QUEUE, PROD_KIND_BUILDING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    },
    [PROD_SETTLER] = {
        S_UNIT_SETTLER, PROD_KIND_UNIT, UNIT_SETTLER, 0, 20, 0, 0, 0, 0, 0, 0, 0
    },
    [PROD_WARRIOR] = {
        S_UNIT_WARRIOR, PROD_KIND_UNIT, UNIT_WARRIOR, 0, 10, 0, 0, 0, 0, 0, 0, 0
    },
    [PROD_SCOUT] = {
        S_UNIT_SCOUT, PROD_KIND_UNIT, UNIT_SCOUT, 0, 8, 0, 0, 0, 0, 0, 0, 0
    },
    [PROD_GRANARY] = {
        S_BUILDING_GRANARY, PROD_KIND_BUILDING, 0, BUILDING_GRANARY,
        20, 1, 2, 0, 0, 0, 0, 0, TECH_POTTERY
    },
    [PROD_MONUMENT] = {
        S_BUILDING_MONUMENT, PROD_KIND_BUILDING, 0, BUILDING_MONUMENT,
        15, 1, 0, 0, 1, 0, 1, 0, TECH_NONE
    },
    [PROD_BARRACKS] = {
        S_BUILDING_BARRACKS, PROD_KIND_BUILDING, 0, BUILDING_BARRACKS,
        25, 1, 0, 1, 0, 0, 0, 0, TECH_BRONZE_WORKING
    },
    [PROD_MARKET] = {
        S_BUILDING_MARKET, PROD_KIND_BUILDING, 0, BUILDING_MARKET,
        25, 1, 0, 0, 4, 0, 0, 0, TECH_CURRENCY
    },
    [PROD_LIBRARY] = {
        S_BUILDING_LIBRARY, PROD_KIND_BUILDING, 0, BUILDING_LIBRARY,
        30, 1, 0, 0, 0, 2, 1, 0, TECH_WRITING
    },
    [PROD_WALLS] = {
        S_BUILDING_WALLS, PROD_KIND_BUILDING, 0, BUILDING_WALLS,
        20, 1, 0, 0, 0, 0, 0, 2, TECH_MASONRY
    },
    [PROD_WORKSHOP] = {
        S_BUILDING_WORKSHOP, PROD_KIND_BUILDING, 0, BUILDING_WORKSHOP,
        35, 1, 0, 2, 1, 1, 0, 0, TECH_ENGINEERING
    },
    [PROD_UNIVERSITY] = {
        S_BUILDING_UNIVERSITY, PROD_KIND_BUILDING, 0, BUILDING_UNIVERSITY,
        45, 2, 0, 0, 0, 3, 2, 0, TECH_IRON_WORKING
    },
    [PROD_AQUEDUCT] = {
        S_BUILDING_AQUEDUCT, PROD_KIND_BUILDING, 0, BUILDING_AQUEDUCT,
        40, 2, 2, 1, 1, 0, 0, 0, TECH_CONSTRUCTION
    },
    [PROD_SWORDSMAN] = {
        S_UNIT_SWORDSMAN, PROD_KIND_UNIT, UNIT_SWORDSMAN, 0, 12, 0,
        0, 0, 0, 0, 0, 0, TECH_IRON_WORKING
    },
    [PROD_HORSEMAN] = {
        S_UNIT_HORSEMAN, PROD_KIND_UNIT, UNIT_HORSEMAN, 0, 14, 0,
        0, 0, 0, 0, 0, 0, TECH_HORSEBACK
    },
    [PROD_WORKER] = {
        S_UNIT_WORKER, PROD_KIND_UNIT, UNIT_WORKER, 0, 10, 0,
        0, 0, 0, 0, 0, 0, TECH_NONE
    },
};

const StrId g_city_name_ids[CITY_NAME_COUNT] = {
    S_CITY_NAME_AURELIA,
    S_CITY_NAME_CASTRUM,
    S_CITY_NAME_PORTA,
    S_CITY_NAME_NOVA,
    S_CITY_NAME_VINCA,
    S_CITY_NAME_ARX,
    S_CITY_NAME_OSTIA,
    S_CITY_NAME_VELIA,
    S_CITY_NAME_LUMEN,
    S_CITY_NAME_FERRUM,
};

const ProductionDef *Data_ProductionDef(ProductionType type) {
    if (type <= PROD_NONE || type >= PROD_COUNT) return NULL;
    return &g_production_defs[type];
}

const ProductionDef *Data_BuildingDef(BuildingType type) {
    if (type < 0 || type >= BUILDING_COUNT) return NULL;
    return &g_production_defs[PROD_GRANARY + (int)type];
}

ProductionType Data_ProductionForBuilding(BuildingType type) {
    if (type < 0 || type >= BUILDING_COUNT) return PROD_NONE;
    return (ProductionType)(PROD_GRANARY + (int)type);
}

void Data_TileYield(const Map *map, int x, int y, Yield *out) {
    if (!out) return;
    *out = (Yield){ 0, 0, 0, 0 };
    if (!map || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return;

    const MapTile *tile = &map->tiles[y][x];
    if (tile->terrain >= TERRAIN_COUNT) return;
    out->food += g_terrain_yields[tile->terrain].food;
    out->production += g_terrain_yields[tile->terrain].production;
    out->gold += g_terrain_yields[tile->terrain].gold;
    out->science += g_terrain_yields[tile->terrain].science;

    if (tile->resource < RESOURCE_COUNT) {
        out->food += g_resource_yields[tile->resource].food;
        out->production += g_resource_yields[tile->resource].production;
        out->gold += g_resource_yields[tile->resource].gold;
        out->science += g_resource_yields[tile->resource].science;
    }
    if (tile->improvement < IMPROVEMENT_COUNT) {
        out->food += g_improvement_yields[tile->improvement].food;
        out->production += g_improvement_yields[tile->improvement].production;
        out->gold += g_improvement_yields[tile->improvement].gold;
        out->science += g_improvement_yields[tile->improvement].science;
    }
}

StrId Data_TerrainName(int terrain) {
    switch (terrain) {
        case TERRAIN_GRASSLAND: return S_TERRAIN_GRASSLAND;
        case TERRAIN_PLAINS:    return S_TERRAIN_PLAINS;
        case TERRAIN_FOREST:    return S_TERRAIN_FOREST;
        case TERRAIN_HILLS:     return S_TERRAIN_HILLS;
        case TERRAIN_MOUNTAIN:  return S_TERRAIN_MOUNTAIN;
        case TERRAIN_DESERT:    return S_TERRAIN_DESERT;
        case TERRAIN_TUNDRA:    return S_TERRAIN_TUNDRA;
        case TERRAIN_COAST:     return S_TERRAIN_COAST;
        default:                return S_TERRAIN_OCEAN;
    }
}

StrId Data_ResourceName(int resource) {
    switch (resource) {
        case RESOURCE_HORSES: return S_RESOURCE_HORSES;
        case RESOURCE_IRON:   return S_RESOURCE_IRON;
        case RESOURCE_STONE:  return S_RESOURCE_STONE;
        case RESOURCE_GEMS:   return S_RESOURCE_GEMS;
        default:              return S_RESOURCE_NONE;
    }
}

int Data_ImprovementRequiredTech(int improvement) {
    if (improvement <= IMPROVEMENT_NONE || improvement >= IMPROVEMENT_COUNT) {
        return TECH_NONE;
    }
    static const uint8_t req[IMPROVEMENT_COUNT] = {
        [IMPROVEMENT_NONE]    = TECH_NONE,
        [IMPROVEMENT_FARM]    = TECH_POTTERY,
        [IMPROVEMENT_MINE]    = TECH_MINING,
        [IMPROVEMENT_PASTURE] = TECH_HORSEBACK,
        [IMPROVEMENT_QUARRY]  = TECH_MASONRY,
        [IMPROVEMENT_ROAD]    = TECH_THE_WHEEL,
    };
    return req[improvement];
}

StrId Data_ImprovementName(int improvement) {
    switch (improvement) {
        case IMPROVEMENT_FARM:    return S_IMPROVEMENT_FARM;
        case IMPROVEMENT_MINE:    return S_IMPROVEMENT_MINE;
        case IMPROVEMENT_PASTURE: return S_IMPROVEMENT_PASTURE;
        case IMPROVEMENT_QUARRY:  return S_IMPROVEMENT_QUARRY;
        case IMPROVEMENT_ROAD:    return S_IMPROVEMENT_ROAD;
        default:                  return S_IMPROVEMENT_NONE;
    }
}

StrId Data_CityName(int name_index) {
    if (name_index < 0 || name_index >= CITY_NAME_COUNT) {
        return g_city_name_ids[0];
    }
    return g_city_name_ids[name_index];
}
