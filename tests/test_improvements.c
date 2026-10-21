#include <stdio.h>
#include <string.h>

#include "city.h"
#include "data.h"
#include "improvement.h"
#include "map.h"
#include "tech.h"
#include "unit.h"

static int failures = 0;
static Map map;
static CityPool cities;
static UnitPool units;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static uint32_t all_improvement_techs(void) {
    return (1u << TECH_POTTERY) | (1u << TECH_MINING)
         | (1u << TECH_MASONRY) | (1u << TECH_THE_WHEEL)
         | (1u << TECH_HORSEBACK);
}

static void flat_map(void) {
    memset(&map, 0, sizeof(map));
    map.seed = 1;
    map.start_x = 10;
    map.start_y = 10;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map.tiles[y][x].terrain = TERRAIN_GRASSLAND;
            map.tiles[y][x].explored = 1;
            map.tiles[y][x].visible = 1;
        }
    }
}

static void setup_city_and_worker(int *worker_index) {
    flat_map();
    CityPool_Init(&cities);
    UnitPool_Init(&units);
    int city_index = -1;
    CHECK(City_TryFound(&cities, &map, 0, 10, 10, &city_index)
          == CITY_FOUND_OK);
    CHECK(city_index == 0);
    *worker_index = Unit_Spawn(&units, UNIT_WORKER, 0, 10, 11);
    CHECK(*worker_index == 0);
    CHECK(units.units[0].moves_left == 4);
}

static void set_tile(int x, int y, int terrain, int resource) {
    map.tiles[y][x].terrain = (uint8_t)terrain;
    map.tiles[y][x].resource = (uint8_t)resource;
}

static void test_worker_data(void) {
    CHECK(Unit_TypeMovePoints(UNIT_WORKER) == 4);
    CHECK(Unit_TypeSight(UNIT_WORKER) == 2);
    CHECK(Improvement_IsWorkerUnit(UNIT_WORKER));
    CHECK(!Improvement_IsWorkerUnit(UNIT_WARRIOR));

    const ProductionDef *worker = Data_ProductionDef(PROD_WORKER);
    CHECK(worker != NULL);
    CHECK(worker && worker->kind == PROD_KIND_UNIT);
    CHECK(worker && worker->unit_type == UNIT_WORKER);
    CHECK(worker && worker->cost == 10);
    CHECK(worker && worker->required_tech == TECH_NONE);
}

static void test_prerequisites_and_ownership(void) {
    int worker_index = -1;
    setup_city_and_worker(&worker_index);
    Unit *worker = &units.units[worker_index];

    CHECK(Improvement_CanBuild(&map, &cities, worker, 0, IMPROVEMENT_FARM)
          == IMPROVEMENT_ERR_LOCKED);
    CHECK(Improvement_AllowedOnTile(&map, 10, 11, IMPROVEMENT_FARM));

    Unit warrior = { UNIT_WARRIOR, 0, 4, 10, 11, UNIT_HP_MAX, 0 };
    CHECK(Improvement_CanBuild(&map, &cities, &warrior,
                               1u << TECH_POTTERY, IMPROVEMENT_FARM)
          == IMPROVEMENT_ERR_NOT_WORKER);

    worker->x = 15;
    worker->y = 15;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_POTTERY,
                               IMPROVEMENT_FARM)
          == IMPROVEMENT_ERR_NOT_OWNED);

    worker->x = 10;
    worker->y = 10;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_POTTERY,
                               IMPROVEMENT_FARM)
          == IMPROVEMENT_ERR_CITY);

    set_tile(9, 11, TERRAIN_FOREST, RESOURCE_NONE);
    worker->x = 9;
    worker->y = 11;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_POTTERY,
                               IMPROVEMENT_FARM)
          == IMPROVEMENT_ERR_TERRAIN);

    worker->moves_left = 0;
    worker->x = 10;
    worker->y = 11;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_POTTERY,
                               IMPROVEMENT_FARM)
          == IMPROVEMENT_ERR_NO_MOVES);
}

static void test_terrain_rules(void) {
    int worker_index = -1;
    setup_city_and_worker(&worker_index);
    Unit *worker = &units.units[worker_index];

    set_tile(11, 10, TERRAIN_HILLS, RESOURCE_NONE);
    worker->x = 11;
    worker->y = 10;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_MINING,
                               IMPROVEMENT_MINE)
          == IMPROVEMENT_OK);
    CHECK(Improvement_CanBuild(&map, &cities, worker, 0, IMPROVEMENT_MINE)
          == IMPROVEMENT_ERR_LOCKED);

    worker->x = 10;
    worker->y = 11;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_MINING,
                               IMPROVEMENT_MINE)
          == IMPROVEMENT_ERR_TERRAIN);

    set_tile(9, 10, TERRAIN_GRASSLAND, RESOURCE_HORSES);
    worker->x = 9;
    worker->y = 10;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_HORSEBACK,
                               IMPROVEMENT_PASTURE)
          == IMPROVEMENT_OK);
    CHECK(Improvement_CanBuild(&map, &cities, worker, 0,
                               IMPROVEMENT_PASTURE)
          == IMPROVEMENT_ERR_LOCKED);

    set_tile(11, 9, TERRAIN_PLAINS, RESOURCE_STONE);
    worker->x = 11;
    worker->y = 9;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_MASONRY,
                               IMPROVEMENT_QUARRY)
          == IMPROVEMENT_OK);

    worker->x = 10;
    worker->y = 11;
    CHECK(Improvement_CanBuild(&map, &cities, worker, 1u << TECH_THE_WHEEL,
                               IMPROVEMENT_ROAD)
          == IMPROVEMENT_OK);
}

static void test_build_and_yields(void) {
    int worker_index = -1;
    setup_city_and_worker(&worker_index);
    Unit *worker = &units.units[worker_index];
    uint32_t techs = all_improvement_techs();

    CHECK(Improvement_Build(&map, &cities, worker, techs, IMPROVEMENT_FARM)
          == IMPROVEMENT_OK);
    CHECK(map.tiles[11][10].improvement == IMPROVEMENT_FARM);
    CHECK(worker->moves_left == 0);

    worker->moves_left = 4;
    CHECK(Improvement_Build(&map, &cities, worker, techs, IMPROVEMENT_MINE)
          == IMPROVEMENT_ERR_ALREADY);

    Yield yield;
    Data_TileYield(&map, 10, 11, &yield);
    CHECK(yield.food == 4);
    CHECK(yield.production == 0);
    CHECK(yield.gold == 2);
}

static void test_road_speed(void) {
    flat_map();
    map.tiles[11][12].improvement = IMPROVEMENT_ROAD;
    CHECK(Unit_StepCost(&map, 11, 11, 12, 11) == 1);
    CHECK(Unit_StepCost(&map, 11, 11, 13, 11) == -1);
    map.tiles[12][12].improvement = IMPROVEMENT_ROAD;
    CHECK(Unit_StepCost(&map, 11, 11, 12, 12) == 2);
    map.tiles[11][12].improvement = IMPROVEMENT_NONE;
    CHECK(Unit_StepCost(&map, 11, 11, 12, 11) == 2);
}

int main(void) {
    test_worker_data();
    test_prerequisites_and_ownership();
    test_terrain_rules();
    test_build_and_yields();
    test_road_speed();

    if (failures) {
        printf("%d improvement test(s) failed\n", failures);
        return 1;
    }
    printf("PASS: worker, tech-gated improvements, terrain rules, and roads\n");
    return 0;
}
