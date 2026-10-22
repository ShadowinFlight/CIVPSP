#include <stdio.h>
#include <string.h>

#include "city.h"
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

static void flat_map(void) {
    memset(&map, 0, sizeof(map));
    map.seed = 1;
    map.start_x = 10;
    map.start_y = 10;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map.tiles[y][x].terrain = TERRAIN_GRASSLAND;
        }
    }
}

static void set_terrain(int x, int y, int terrain) {
    map.tiles[y][x].terrain = (uint8_t)terrain;
}

static void test_movement_costs(void) {
    flat_map();
    CHECK(Unit_StepCost(&map, 5, 5, 6, 5) == 2);
    CHECK(Unit_StepCost(&map, 5, 5, 6, 6) == 3);
    CHECK(Unit_StepCost(&map, 5, 5, 5, 5) == -1);
    CHECK(Unit_StepCost(&map, 5, 5, 7, 5) == -1);

    set_terrain(7, 5, TERRAIN_FOREST);
    CHECK(Unit_StepCost(&map, 5, 5, 7, 5) == -1);
    CHECK(Unit_StepCost(&map, 6, 5, 7, 5) == 4);

    set_terrain(6, 6, TERRAIN_HILLS);
    CHECK(Unit_StepCost(&map, 5, 5, 6, 6) == 5);

    set_terrain(8, 5, TERRAIN_MOUNTAIN);
    CHECK(Unit_StepCost(&map, 7, 5, 8, 5) == -1);
    CHECK(!Unit_PassableTile(&map, 8, 5));
    CHECK(Unit_PassableTile(&map, 5, 5));

    set_terrain(8, 6, TERRAIN_OCEAN);
    CHECK(Unit_StepCost(&map, 7, 5, 8, 6) == -1);
    set_terrain(8, 6, TERRAIN_COAST);
    CHECK(!Unit_PassableTile(&map, 8, 6));
}

static void test_unit_movement(void) {
    flat_map();
    UnitPool_Init(&world.units);

    int index = Unit_Spawn(&world.units, UNIT_SETTLER, 0, 5, 5);
    CHECK(index == 0);
    CHECK(world.units.units[0].moves_left == 6);

    CHECK(Unit_MoveTo(&world.units, &map, 0, 6, 5) == 2);
    CHECK(world.units.units[0].x == 6);
    CHECK(world.units.units[0].y == 5);
    CHECK(world.units.units[0].moves_left == 4);

    CHECK(Unit_MoveTo(&world.units, &map, 0, 7, 6) == 3);
    CHECK(world.units.units[0].x == 7);
    CHECK(world.units.units[0].y == 6);
    CHECK(world.units.units[0].moves_left == 1);

    CHECK(Unit_MoveTo(&world.units, &map, 0, 8, 7) == -1);
    CHECK(world.units.units[0].x == 7);
    CHECK(world.units.units[0].y == 6);
    CHECK(Unit_MoveTo(&world.units, &map, 0, 7, 6) == 0);

    flat_map();
    UnitPool_Init(&world.units);
    Unit_Spawn(&world.units, UNIT_SCOUT, 0, 10, 10);
    CHECK(Unit_MoveTo(&world.units, &map, 0, 12, 12) == 6);
    CHECK(world.units.units[0].x == 12);
    CHECK(world.units.units[0].y == 12);
    CHECK(world.units.units[0].moves_left == 2);

    flat_map();
    UnitPool_Init(&world.units);
    Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 5, 5);
    Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 6, 5);
    CHECK(Unit_MoveTo(&world.units, &map, 0, 6, 5) == -1);
    CHECK(world.units.units[0].x == 5);
    CHECK(world.units.units[0].moves_left == 6);

    flat_map();
    UnitPool_Init(&world.units);
    Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 5, 5);
    world.units.units[0].moves_left = 4;
    CHECK(Unit_MoveTo(&world.units, &map, 0, 7, 5) == 4);
    CHECK(world.units.units[0].moves_left == 0);

    flat_map();
    UnitPool_Init(&world.units);
    Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 10, 10);
    for (int y = 0; y < MAP_HEIGHT; y++) set_terrain(11, y, TERRAIN_OCEAN);
    CHECK(Unit_PathCost(&world.units, &map, &world.units.units[0],
                        12, 10, 6) == -1);
    CHECK(Unit_MoveTo(&world.units, &map, 0, 12, 10) == -1);
    CHECK(world.units.units[0].x == 10);
    CHECK(world.units.units[0].moves_left == 6);
    set_terrain(11, 10, TERRAIN_MOUNTAIN);
    CHECK(Unit_MoveTo(&world.units, &map, 0, 11, 10) == -1);
}

static void test_city_founding(void) {
    flat_map();
    CityPool_Init(&world.cities);

    int index = -1;
    CHECK(City_TryFound(&world.cities, &map, 0, 10, 10, &index)
          == CITY_FOUND_OK);
    CHECK(index == 0);
    CHECK(world.cities.count == 1);
    CHECK(world.cities.cities[0].name_index == 0);
    CHECK(world.cities.cities[0].population == 1);
    CHECK(City_OwnedCount(&world.cities.cities[0]) == 9);
    CHECK(City_WorkedCount(&world.cities.cities[0]) == 1);
    CHECK(City_OwnsIndex(&world.cities.cities[0], CITY_CENTER_INDEX));
    CHECK(City_AtConst(&world.cities, 10, 10) != NULL);

    CHECK(City_TryFound(&world.cities, &map, 0, 12, 12, &index)
          == CITY_FOUND_TOO_CLOSE);
    CHECK(City_TryFound(&world.cities, &map, 0, 13, 13, &index)
          == CITY_FOUND_OK);
    CHECK(index == 1);
    CHECK(world.cities.cities[1].name_index == 1);

    set_terrain(20, 20, TERRAIN_OCEAN);
    CHECK(City_TryFound(&world.cities, &map, 0, 20, 20, &index)
          == CITY_FOUND_INVALID_TILE);
    set_terrain(20, 21, TERRAIN_MOUNTAIN);
    CHECK(City_TryFound(&world.cities, &map, 0, 20, 21, &index)
          == CITY_FOUND_INVALID_TILE);

    CityPool_Init(&world.cities);
    Map_ResetFog(&map);
    City_TryFound(&world.cities, &map, 0, 10, 10, &index);
    City_GrantVision(&world.cities, &map);
    CHECK(map.tiles[10][10].visible);
    CHECK(map.tiles[10][12].visible);
    CHECK(!map.tiles[10][13].visible);
    CHECK(map.tiles[10][12].explored);
    CHECK(map.tiles[8][8].visible);   /* city vision covers the whole work square */
    CHECK(map.tiles[8][8].explored);
    CHECK(!map.tiles[7][10].visible);
}

static void test_world_founding_and_turn(void) {
    flat_map();
    Map_ResetFog(&map);
    World_Init(&world);

    Unit_Spawn(&world.units, UNIT_SETTLER, 0, 10, 10);
    World_UpdateVision(&world, &map);
    CHECK(map.tiles[10][10].visible);

    CHECK(World_FoundCity(&world, &map, 0) == 0);
    CHECK(world.units.count == 0);
    CHECK(world.cities.count == 1);
    CHECK(City_AtConst(&world.cities, 10, 10) != NULL);
    CHECK(map.tiles[10][12].visible);

    Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 20, 20);
    CHECK(World_FoundCity(&world, &map, 0) == WORLD_FOUND_NOT_SETTLER);
    CHECK(world.units.count == 1);

    Unit_Spawn(&world.units, UNIT_SETTLER, 0, 12, 12);
    CHECK(World_FoundCity(&world, &map, 1) == WORLD_FOUND_TOO_CLOSE);
    CHECK(world.units.count == 2);

    UnitPool_Init(&world.units);
    Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 5, 5);
    Unit_Spawn(&world.units, UNIT_SCOUT, 0, 6, 6);
    Unit_Spawn(&world.units, UNIT_SETTLER, 0, 7, 7);
    world.units.units[0].moves_left = 0;
    world.units.units[1].moves_left = 0;
    world.units.units[2].moves_left = 0;
    world.turn = 1;
    World_EndTurn(&world, &map);
    CHECK(world.turn == 2);
    CHECK(world.units.units[0].moves_left == 6);
    CHECK(world.units.units[1].moves_left == 8);
    CHECK(world.units.units[2].moves_left == 6);

    CHECK(World_CycleUnit(&world, -1, 1) == 0);
    CHECK(World_CycleUnit(&world, 0, 1) == 1);
    CHECK(World_CycleUnit(&world, 0, -1) == 2);

    world.units.units[0].moves_left = 0;
    world.units.units[1].moves_left = 0;
    world.units.units[2].moves_left = 5;
    CHECK(World_CycleUnit(&world, 0, 1) == 2);
    CHECK(World_CycleUnit(&world, 2, 1) == 2);

    world.units.units[2].moves_left = 0;
    CHECK(World_CycleUnit(&world, 2, 1) == 0);
    CHECK(World_FirstUnitWithMoves(&world) == -1);
}

static void test_world_start_and_vision(void) {
    Map_Generate(&map, 0x12345678u);
    World_Begin(&world, &map);

    CHECK(world.turn == 1);
    CHECK(World_CountUnits(&world, WORLD_CIV_PLAYER) == 3);
    CHECK(World_PlayerCityCount(&world) == 0);
    CHECK(world.cities.count == 3);
    CHECK(World_CountUnits(&world, WORLD_CIV_BARBARIAN) >= 1);
    CHECK(world.treasury == WORLD_START_TREASURY);
    CHECK(world.science == 0);

    int settlers = 0;
    int warriors = 0;
    int scouts = 0;
    for (int i = 0; i < world.units.count; i++) {
        const Unit *unit = &world.units.units[i];
        if (unit->owner != WORLD_CIV_PLAYER) continue;
        if (unit->type == UNIT_SETTLER) settlers++;
        if (unit->type == UNIT_WARRIOR) warriors++;
        if (unit->type == UNIT_SCOUT) scouts++;
        CHECK(Unit_PassableTile(&map, unit->x, unit->y));
        CHECK(unit->moves_left == Unit_TypeMovePoints(unit->type));
        for (int j = 0; j < i; j++) {
            const Unit *other = &world.units.units[j];
            if (other->owner != WORLD_CIV_PLAYER) continue;
            CHECK(unit->x != other->x || unit->y != other->y);
        }
    }
    CHECK(settlers == 1);
    CHECK(warriors == 1);
    CHECK(scouts == 1);
    CHECK(world.units.units[0].type == UNIT_SETTLER);
    CHECK(world.units.units[0].owner == WORLD_CIV_PLAYER);
    CHECK(world.units.units[0].x == map.start_x);
    CHECK(world.units.units[0].y == map.start_y);

    for (int owner = 1; owner <= WORLD_AI_COUNT; owner++) {
        CHECK(world.civs[owner].active);
        CHECK(world.civs[owner].capital_index != WORLD_CIV_NONE);
        CHECK(World_CountCities(&world, owner) == 1);
        CHECK(World_CountUnits(&world, owner) >= 2);
    }

    CHECK(map.tiles[map.start_y][map.start_x].explored);
    CHECK(map.tiles[map.start_y][map.start_x].visible);
    CHECK(!map.tiles[0][0].explored);
}

static void test_difficulty_setup(void) {
    Map_Generate(&map, 0x12345678u);

    World_BeginWithDifficulty(&world, &map, WORLD_DIFFICULTY_EASY);
    CHECK(world.difficulty == WORLD_DIFFICULTY_EASY);
    CHECK(World_CountUnits(&world, WORLD_CIV_PLAYER) == 4);
    CHECK(world.civs[1].treasury == WORLD_AI_START_TREASURY - 10);

    World_BeginWithDifficulty(&world, &map, WORLD_DIFFICULTY_NORMAL);
    CHECK(world.difficulty == WORLD_DIFFICULTY_NORMAL);
    CHECK(World_CountUnits(&world, WORLD_CIV_PLAYER) == 3);
    CHECK(world.civs[1].treasury == WORLD_AI_START_TREASURY);

    World_BeginWithDifficulty(&world, &map, WORLD_DIFFICULTY_HARD);
    CHECK(world.difficulty == WORLD_DIFFICULTY_HARD);
    CHECK(World_CountUnits(&world, WORLD_CIV_PLAYER) == 3);
    CHECK(world.civs[1].treasury == WORLD_AI_START_TREASURY + 20);
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        CHECK(World_CountUnits(&world, civ) >= 3);
    }
    CHECK(World_CountUnits(&world, WORLD_CIV_BARBARIAN) >= 2);
}

int main(void) {
    test_movement_costs();
    test_unit_movement();
    test_city_founding();
    test_world_founding_and_turn();
    test_world_start_and_vision();
    test_difficulty_setup();

    if (failures == 0) {
        printf("PASS: unit movement, city founding, turns, and start setup\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
