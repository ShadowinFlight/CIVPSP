#include <stdio.h>
#include <string.h>

#include "city.h"
#include "data.h"
#include "map.h"
#include "unit.h"
#include "world.h"

static int failures = 0;
static Map map;
static World world;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void flat_map(int terrain) {
    memset(&map, 0, sizeof(map));
    map.seed = 1;
    map.start_x = 10;
    map.start_y = 10;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map.tiles[y][x].terrain = (uint8_t)terrain;
            map.tiles[y][x].explored = 1;
            map.tiles[y][x].visible = 1;
        }
    }
}

static void set_tile(int x, int y, int terrain, int resource, int improvement) {
    map.tiles[y][x].terrain = (uint8_t)terrain;
    map.tiles[y][x].resource = (uint8_t)resource;
    map.tiles[y][x].improvement = (uint8_t)improvement;
}

static int find_worked(const City *city) {
    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (City_IsWorkedIndex(city, i)) return i;
    }
    return -1;
}

static int find_unworked_owned(const City *city) {
    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (i == CITY_CENTER_INDEX) continue;
        if (City_OwnsIndex(city, i) && !City_IsWorkedIndex(city, i)) return i;
    }
    return -1;
}

static void found_city(CityPool *pool, int x, int y, int *index) {
    CityPool_Init(pool);
    CHECK(City_TryFound(pool, &map, 0, x, y, index) == CITY_FOUND_OK);
}

static void test_yield_table(void) {
    flat_map(TERRAIN_GRASSLAND);
    Yield y;
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.food == 2 && y.production == 0 && y.gold == 1);

    set_tile(5, 5, TERRAIN_PLAINS, RESOURCE_NONE, IMPROVEMENT_NONE);
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.food == 1 && y.production == 1 && y.gold == 2);

    set_tile(5, 5, TERRAIN_FOREST, RESOURCE_NONE, IMPROVEMENT_NONE);
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.food == 1 && y.production == 2);

    set_tile(5, 5, TERRAIN_HILLS, RESOURCE_NONE, IMPROVEMENT_NONE);
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.food == 0 && y.production == 2);

    set_tile(5, 5, TERRAIN_DESERT, RESOURCE_NONE, IMPROVEMENT_NONE);
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.food == 0 && y.production == 0 && y.gold == 1);

    set_tile(5, 5, TERRAIN_COAST, RESOURCE_NONE, IMPROVEMENT_NONE);
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.food == 2 && y.gold == 1);

    set_tile(5, 5, TERRAIN_PLAINS, RESOURCE_HORSES, IMPROVEMENT_FARM);
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.food == 3 && y.production == 2 && y.gold == 3);

    set_tile(5, 5, TERRAIN_PLAINS, RESOURCE_GEMS, IMPROVEMENT_MINE);
    Data_TileYield(&map, 5, 5, &y);
    CHECK(y.gold == 6 && y.production == 2 && y.science == 1);
}

static void test_city_initial_state(void) {
    flat_map(TERRAIN_GRASSLAND);
    int index = -1;
    found_city(&world.cities, 10, 10, &index);
    CHECK(index == 0);
    City *city = &world.cities.cities[0];
    CHECK(city->population == 1);
    CHECK(City_OwnedCount(city) == 9);
    CHECK(City_WorkedCount(city) == 1);
    CHECK(City_OwnsIndex(city, CITY_CENTER_INDEX));
    CHECK(City_OwnsTile(city, 9, 9));
    CHECK(City_OwnsTile(city, 11, 11));
    CHECK(!City_OwnsTile(city, 8, 8));
    CHECK(!City_IsWorkedIndex(city, CITY_CENTER_INDEX));

    CityYield yield;
    City_ComputeYields(city, &map, &yield);
    CHECK(yield.food == 4);
    CHECK(yield.production == 2);
    CHECK(yield.gold == 2);
    CHECK(yield.science == 1);
    CHECK(yield.influence == 1);
    CHECK(yield.maintenance == 0);
    CHECK(City_FoodSurplus(city, &map) == 1);
    CHECK(City_GrowthThreshold(city) == 15);
    CHECK(City_BorderThreshold(city) == 10);
    CHECK(City_TilePurchaseCost(city) == 15);
}

static void test_queue_and_citizens(void) {
    flat_map(TERRAIN_GRASSLAND);
    int index = -1;
    found_city(&world.cities, 10, 10, &index);
    City *city = &world.cities.cities[index];

    CHECK(City_QueueAdd(city, PROD_WARRIOR) == CITY_QUEUE_OK);
    CHECK(City_QueueAdd(city, PROD_GRANARY) == CITY_QUEUE_OK);
    CHECK(City_QueueAdd(city, PROD_GRANARY) == CITY_QUEUE_ALREADY);
    CHECK(City_QueueAdd(city, PROD_WARRIOR) == CITY_QUEUE_OK);
    CHECK(city->queue_count == 3);
    CHECK(City_QueueAdd(city, PROD_SCOUT) == CITY_QUEUE_OK);
    CHECK(City_QueueAdd(city, PROD_MONUMENT) == CITY_QUEUE_OK);
    CHECK(City_QueueAdd(city, PROD_LIBRARY) == CITY_QUEUE_FULL);
    CHECK(City_QueueContains(city, PROD_WARRIOR));
    CHECK(City_QueueRemoveLast(city) == 0);
    CHECK(city->queue_count == 4);
    CHECK(City_ProductionRemaining(city) == 10);
    CHECK(City_TurnsLeft(city, &map) == 5);

    int worked = find_worked(city);
    CHECK(worked >= 0);
    CHECK(City_ToggleWork(city, &map, worked) == CITY_ACTION_OK);
    CHECK(City_WorkedCount(city) == 0);
    int other = find_unworked_owned(city);
    CHECK(other >= 0);
    CHECK(City_ToggleWork(city, &map, other) == CITY_ACTION_OK);
    CHECK(City_WorkedCount(city) == 1);

    int extra = find_unworked_owned(city);
    CHECK(extra >= 0);
    CHECK(City_ToggleLock(city, &map, extra) == CITY_ACTION_NO_IDLE);
    CHECK(City_ToggleLock(city, &map, other) == CITY_ACTION_OK);
    CHECK(City_IsLockedIndex(city, other));
    City_AutoAssign(city, &map);
    CHECK(City_IsWorkedIndex(city, other));
    CHECK(City_IsLockedIndex(city, other));
    CHECK(City_WorkedCount(city) == 1);
}

static void test_growth_and_starvation(void) {
    flat_map(TERRAIN_GRASSLAND);
    int index = -1;
    found_city(&world.cities, 10, 10, &index);
    City *city = &world.cities.cities[index];
    UnitPool units;
    UnitPool_Init(&units);

    city->food_stored = 14;
    CityTurnEvents events;
    City_ProcessTurn(city, &map, &units, 0, &events);
    CHECK(events.grew == 1);
    CHECK(city->population == 2);
    CHECK(city->food_stored == 0);
    CHECK(City_WorkedCount(city) == 2);

    flat_map(TERRAIN_DESERT);
    city->population = 2;
    city->food_stored = 0;
    City_AutoAssign(city, &map);
    CHECK(City_WorkedCount(city) == 2); /* two worked desert tiles still fit pop 2 */
    City_ProcessTurn(city, &map, &units, 0, &events);
    CHECK(events.shrank == 1);
    CHECK(city->population == 1);
    CHECK(city->food_stored == 0);
    CHECK(City_WorkedCount(city) == 1);
}

static void test_production(void) {
    flat_map(TERRAIN_GRASSLAND);
    int index = -1;
    found_city(&world.cities, 10, 10, &index);
    City *city = &world.cities.cities[index];
    UnitPool units;
    UnitPool_Init(&units);
    CityTurnEvents events;

    CHECK(City_QueueAdd(city, PROD_WARRIOR) == CITY_QUEUE_OK);
    for (int i = 0; i < 5; i++) {
        City_ProcessTurn(city, &map, &units, 0, &events);
    }
    CHECK(units.count == 1);
    CHECK(units.units[0].type == UNIT_WARRIOR);
    CHECK(city->queue_count == 0);
    CHECK(city->production_stored == 0);
    CHECK(events.production_completed == 1);

    CHECK(City_QueueAdd(city, PROD_GRANARY) == CITY_QUEUE_OK);
    city->production_stored = 19;
    City_ProcessTurn(city, &map, &units, 0, &events);
    CHECK(City_HasBuilding(city, BUILDING_GRANARY));
    CHECK(city->queue_count == 0);
    CHECK(City_Maintenance(city) == 1);
    CityYield yield;
    City_ComputeYields(city, &map, &yield);
    CHECK(yield.food == 6);

    flat_map(TERRAIN_GRASSLAND);
    UnitPool_Init(&units);
    CityPool_Init(&world.cities);
    City_TryFound(&world.cities, &map, 0, 10, 10, &index);
    city = &world.cities.cities[index];
    CHECK(City_QueueAdd(city, PROD_WARRIOR) == CITY_QUEUE_OK);
    CHECK(City_QueueAdd(city, PROD_SCOUT) == CITY_QUEUE_OK);
    city->production_stored = 9;
    City_ProcessTurn(city, &map, &units, 0, &events);
    CHECK(units.count == 1);
    CHECK(city->queue_count == 1);
    CHECK(city->queue[0] == PROD_SCOUT);
    CHECK(city->production_stored == 1);
}

static void test_unit_space_and_rush(void) {
    flat_map(TERRAIN_GRASSLAND);
    int index = -1;
    found_city(&world.cities, 10, 10, &index);
    City *city = &world.cities.cities[index];
    UnitPool units;
    UnitPool_Init(&units);
    CityTurnEvents events;

    for (int y = 9; y <= 11; y++) {
        for (int x = 9; x <= 11; x++) {
            CHECK(Unit_Spawn(&units, UNIT_WARRIOR, 0, x, y) >= 0);
        }
    }
    CHECK(units.count == 9);
    CHECK(City_QueueAdd(city, PROD_WARRIOR) == CITY_QUEUE_OK);
    city->production_stored = 10;
    City_ProcessTurn(city, &map, &units, 0, &events);
    CHECK(events.unit_held == 1);
    CHECK(units.count == 9);
    CHECK(city->queue_count == 1);
    CHECK(city->production_stored == 12);

    Unit_RemoveAt(&units, 0);
    City_ProcessTurn(city, &map, &units, 0, &events);
    CHECK(events.production_completed == 1);
    CHECK(units.count == 9);
    CHECK(city->queue_count == 0);

    found_city(&world.cities, 20, 20, &index);
    city = &world.cities.cities[index];
    UnitPool_Init(&units);
    int treasury = 100;
    CHECK(City_QueueAdd(city, PROD_GRANARY) == CITY_QUEUE_OK);
    city->production_stored = 5;
    CHECK(City_RushCost(city) == 30);
    CHECK(City_RushProduction(city, &map, &units, &treasury)
          == CITY_ACTION_OK);
    CHECK(treasury == 70);
    CHECK(City_HasBuilding(city, BUILDING_GRANARY));
    CHECK(city->queue_count == 0);
    CHECK(city->production_stored == 0);

    CHECK(City_QueueAdd(city, PROD_MARKET) == CITY_QUEUE_OK);
    treasury = 0;
    CHECK(City_RushProduction(city, &map, &units, &treasury)
          == CITY_ACTION_NO_GOLD);
    CHECK(city->queue_count == 1);
}

static void test_tile_purchase_and_border(void) {
    flat_map(TERRAIN_GRASSLAND);
    int index = -1;
    found_city(&world.cities, 10, 10, &index);
    City *city = &world.cities.cities[index];
    int treasury = 100;

    uint32_t saved_mask = city->owned_mask;
    city->owned_mask = 1u << CITY_CENTER_INDEX;
    CHECK(City_CanBuyTile(city, &map, 0) == CITY_TILE_NOT_ADJACENT);
    city->owned_mask = saved_mask;

    CHECK(City_CanBuyTile(city, &map, 1) == CITY_TILE_OK);
    CHECK(City_BuyTile(city, &map, &treasury, 1) == CITY_TILE_OK);
    CHECK(City_OwnedCount(city) == 10);
    CHECK(treasury == 85);
    CHECK(map.tiles[8][8].explored);
    CHECK(map.tiles[8][8].visible);
    CHECK(City_BuyTile(city, &map, &treasury, 1) == CITY_TILE_OWNED);
    CHECK(City_BuyTile(city, &map, &treasury, 0) == CITY_TILE_OK);
    CHECK(City_OwnedCount(city) == 11);
    CHECK(treasury == 65);

    treasury = 0;
    CHECK(City_BuyTile(city, &map, &treasury, 2) == CITY_TILE_NO_GOLD);
    CHECK(City_OwnedCount(city) == 11);

    CityPool_Init(&world.cities);
    City_TryFound(&world.cities, &map, 0, 20, 20, &index);
    city = &world.cities.cities[index];
    city->influence = 9;
    CHECK(City_GrowBorder(city, &map) == 1);
    CHECK(City_OwnedCount(city) == 10);
    CHECK(city->influence == 0);
    city->influence = 14;
    CHECK(City_GrowBorder(city, &map) == 1);
    CHECK(City_OwnedCount(city) == 11);
    CHECK(city->influence == 0);

    city->owned_mask = 0x01FFFFFFu; /* all 25 bits */
    city->influence = 0;
    CHECK(City_GrowBorder(city, &map) == 0);
    CHECK(City_OwnedCount(city) == 25);
}

static void test_world_economy(void) {
    flat_map(TERRAIN_GRASSLAND);
    World_Init(&world);
    world.treasury = 100;
    int index = -1;
    City_TryFound(&world.cities, &map, 0, 10, 10, &index);
    City *city = &world.cities.cities[index];
    CHECK(City_QueueAdd(city, PROD_MONUMENT) == CITY_QUEUE_OK);

    for (int i = 0; i < 10; i++) World_EndTurn(&world, &map);
    CHECK(world.turn == 11);
    CHECK(City_HasBuilding(city, BUILDING_MONUMENT));
    CHECK(world.science == 10);
    CHECK(world.treasury == 120);
    CHECK(world.gold_per_turn == 2);
    CHECK(world.science_per_turn == 1);

    world.treasury = -1;
    city->production_stored = 0;
    city->queue_count = 0;
    CHECK(City_QueueAdd(city, PROD_WARRIOR) == CITY_QUEUE_OK);
    int science_before = (int)world.science;
    World_ProcessCities(&world, &map);
    CHECK(city->production_stored == 1); /* halved production */
    CHECK((int)world.science == science_before); /* halved science */
}

static void test_30_turn_city(void) {
    flat_map(TERRAIN_GRASSLAND);
    set_tile(9, 8, TERRAIN_PLAINS, RESOURCE_NONE, IMPROVEMENT_NONE);
    set_tile(10, 8, TERRAIN_FOREST, RESOURCE_NONE, IMPROVEMENT_NONE);
    set_tile(11, 8, TERRAIN_HILLS, RESOURCE_NONE, IMPROVEMENT_NONE);
    set_tile(8, 9, TERRAIN_FOREST, RESOURCE_NONE, IMPROVEMENT_NONE);
    set_tile(12, 9, TERRAIN_PLAINS, RESOURCE_HORSES, IMPROVEMENT_NONE);
    set_tile(8, 10, TERRAIN_HILLS, RESOURCE_NONE, IMPROVEMENT_NONE);
    set_tile(12, 10, TERRAIN_GRASSLAND, RESOURCE_GEMS, IMPROVEMENT_NONE);
    set_tile(9, 11, TERRAIN_PLAINS, RESOURCE_NONE, IMPROVEMENT_NONE);
    set_tile(11, 11, TERRAIN_FOREST, RESOURCE_NONE, IMPROVEMENT_NONE);

    World_Init(&world);
    world.treasury = 200;
    int index = -1;
    City_TryFound(&world.cities, &map, 0, 10, 10, &index);
    City *city = &world.cities.cities[index];

    static const ProductionType plan[] = {
        PROD_MONUMENT, PROD_GRANARY, PROD_WARRIOR, PROD_LIBRARY,
        PROD_AQUEDUCT, PROD_MARKET, PROD_WORKSHOP, PROD_UNIVERSITY,
        PROD_WALLS, PROD_BARRACKS, PROD_SETTLER, PROD_SCOUT,
    };
    int plan_index = 0;
    for (int turn = 0; turn < 30; turn++) {
        while (city->queue_count < 3
               && plan_index < (int)(sizeof(plan) / sizeof(plan[0]))) {
            CityQueueResult result = City_QueueAdd(city, plan[plan_index++]);
            if (result != CITY_QUEUE_OK
                && result != CITY_QUEUE_ALREADY) {
                CHECK(0);
            }
        }
        World_EndTurn(&world, &map);
    }

    CHECK(world.turn == 31);
    CHECK(city->population >= 2);
    CHECK(City_OwnedCount(city) > 9 && City_OwnedCount(city) <= 25);
    CHECK(City_BuildingCount(city) >= 3);
    CHECK(world.units.count >= 1);
    CHECK(world.science > 0);
    CHECK(city->queue_count <= CITY_MAX_QUEUE);
    CHECK(city->food_stored >= 0);
    CHECK(City_WorkedCount(city) <= city->population);
    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (City_IsWorkedIndex(city, i)) {
            CHECK(City_OwnsIndex(city, i));
            CHECK(i != CITY_CENTER_INDEX);
        }
    }
    printf("30-turn city: pop=%d owned=%d buildings=%d units=%d "
           "science=%d treasury=%d queue=%d\n",
           city->population, City_OwnedCount(city),
           City_BuildingCount(city), world.units.count,
           (int)world.science, (int)world.treasury, city->queue_count);
}

int main(void) {
    test_yield_table();
    test_city_initial_state();
    test_queue_and_citizens();
    test_growth_and_starvation();
    test_production();
    test_unit_space_and_rush();
    test_tile_purchase_and_border();
    test_world_economy();
    test_30_turn_city();

    if (failures == 0) {
        printf("PASS: city yields, growth, production, borders, gold, "
               "and 30-turn management\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
