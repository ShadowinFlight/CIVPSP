#ifndef CIVPSP_DATA_H
#define CIVPSP_DATA_H

#include <stdint.h>

#include "gen/strings_gen.h"
#include "map.h"
#include "tech.h"
#include "unit.h"

#define CITY_NAME_COUNT 10
#define BUILDING_COUNT 9

typedef enum {
    BUILDING_GRANARY = 0,
    BUILDING_MONUMENT,
    BUILDING_BARRACKS,
    BUILDING_MARKET,
    BUILDING_LIBRARY,
    BUILDING_WALLS,
    BUILDING_WORKSHOP,
    BUILDING_UNIVERSITY,
    BUILDING_AQUEDUCT
} BuildingType;

typedef enum {
    PROD_NONE = 0,
    PROD_SETTLER,
    PROD_WARRIOR,
    PROD_SCOUT,
    PROD_GRANARY,
    PROD_MONUMENT,
    PROD_BARRACKS,
    PROD_MARKET,
    PROD_LIBRARY,
    PROD_WALLS,
    PROD_WORKSHOP,
    PROD_UNIVERSITY,
    PROD_AQUEDUCT,
    PROD_SWORDSMAN,
    PROD_HORSEMAN,
    PROD_WORKER,
    PROD_COUNT
} ProductionType;

typedef enum {
    PROD_KIND_UNIT = 0,
    PROD_KIND_BUILDING
} ProductionKind;

typedef struct {
    int16_t food;
    int16_t production;
    int16_t gold;
    int16_t science;
} Yield;

typedef struct {
    StrId name_id;
    uint8_t kind;
    uint8_t unit_type;
    uint8_t building_type;
    uint8_t cost;
    uint8_t maintenance;
    int8_t food;
    int8_t production;
    int8_t gold;
    int8_t science;
    int8_t influence;
    int8_t defense;
    /* Technology that must be researched before this can be built (TECH_NONE
     * means no prerequisite). Appended last so existing initializers stay
     * valid (they leave this field zero). */
    uint8_t required_tech;
} ProductionDef;

extern const Yield g_terrain_yields[TERRAIN_COUNT];
extern const Yield g_resource_yields[RESOURCE_COUNT];
extern const Yield g_improvement_yields[IMPROVEMENT_COUNT];
extern const ProductionDef g_production_defs[PROD_COUNT];
extern const StrId g_city_name_ids[CITY_NAME_COUNT];

void Data_TileYield(const Map *map, int x, int y, Yield *out);
const ProductionDef *Data_ProductionDef(ProductionType type);
const ProductionDef *Data_BuildingDef(BuildingType type);
ProductionType Data_ProductionForBuilding(BuildingType type);
int Data_ImprovementRequiredTech(int improvement); /* TechType or TECH_NONE */
StrId Data_TerrainName(int terrain);
StrId Data_ResourceName(int resource);
StrId Data_ImprovementName(int improvement);
StrId Data_UnitName(int unit_type);
StrId Data_CityName(int name_index);

#endif
