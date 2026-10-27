#include <stdio.h>
#include <string.h>

#include "city.h"
#include "combat.h"
#include "map.h"
#include "tech.h"
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

static int explored_count(void) {
    int count = 0;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map.tiles[y][x].explored) count++;
        }
    }
    return count;
}

static void test_auto_explore(void) {
    flat_map();
    Map_ResetFog(&map);
    World_Init(&world);

    int index = Unit_Spawn(&world.units, UNIT_SCOUT, WORLD_CIV_PLAYER,
                           10, 10);
    CHECK(index == 0);
    Unit_SetAutoExplore(&world.units.units[0], 1);
    CHECK(Unit_AutoExploreEnabled(&world.units.units[0]));
    CHECK((world.units.units[0].acted & UNIT_ACTED_MASK) == 0);

    Map_MarkVisibleArea(&map, 10, 10, Unit_TypeSight(UNIT_SCOUT));
    int before = explored_count();
    int start_x = world.units.units[0].x;
    int start_y = world.units.units[0].y;
    int steps = World_AutoExploreScout(&world, &map, index);
    CHECK(steps > 0);
    CHECK(world.units.units[0].x != start_x
          || world.units.units[0].y != start_y);
    CHECK(explored_count() > before);
    CHECK(Unit_AutoExploreEnabled(&world.units.units[0]));
    CHECK(world.units.units[0].moves_left < 8);

    world.units.units[0].acted |= 0x01u;
    Unit_ResetTurn(&world.units);
    CHECK(Unit_AutoExploreEnabled(&world.units.units[0]));
    CHECK((world.units.units[0].acted & UNIT_ACTED_MASK) == 0);
    CHECK(world.units.units[0].moves_left == 8);

    /* The end-turn path should run auto-explore for flagged scouts without
     * clearing the saved high-bit toggle. */
    flat_map();
    Map_ResetFog(&map);
    World_Init(&world);
    index = Unit_Spawn(&world.units, UNIT_SCOUT, WORLD_CIV_PLAYER, 10, 10);
    Map_MarkVisibleArea(&map, 10, 10, Unit_TypeSight(UNIT_SCOUT));
    Unit_SetAutoExplore(&world.units.units[index], 1);
    start_x = world.units.units[index].x;
    start_y = world.units.units[index].y;
    world.turn = 1;
    World_EndTurn(&world, &map);
    CHECK(world.turn == 2);
    CHECK(world.units.units[index].x != start_x
          || world.units.units[index].y != start_y);
    CHECK(Unit_AutoExploreEnabled(&world.units.units[index]));
    CHECK((world.units.units[index].acted & UNIT_ACTED_MASK) == 0);

    Unit_SetAutoExplore(&world.units.units[index], 0);
    CHECK(!Unit_AutoExploreEnabled(&world.units.units[index]));
}

static void test_medic_support(void) {
    CHECK(Unit_TypeMovePoints(UNIT_MEDIC) == 4);
    CHECK(Unit_TypeSight(UNIT_MEDIC) == 2);
    CHECK(g_unit_defs[UNIT_MEDIC].strength == 0);
    CHECK(Data_UnitName(UNIT_MEDIC) != Data_UnitName(UNIT_WARRIOR));

    const ProductionDef *medic = Data_ProductionDef(PROD_MEDIC);
    CHECK(medic != NULL);
    CHECK(medic && medic->kind == PROD_KIND_UNIT);
    CHECK(medic && medic->unit_type == UNIT_MEDIC);
    CHECK(medic && medic->cost == 12);
    CHECK(medic && medic->required_tech == TECH_NONE);

    flat_map();
    Map_ResetFog(&map);
    World_Init(&world);
    int medic_a = Unit_Spawn(&world.units, UNIT_MEDIC, WORLD_CIV_PLAYER,
                             10, 10);
    int medic_b = Unit_Spawn(&world.units, UNIT_MEDIC, WORLD_CIV_PLAYER,
                             11, 10);
    int rested = Unit_Spawn(&world.units, UNIT_WARRIOR, WORLD_CIV_PLAYER,
                            10, 11);
    int moved = Unit_Spawn(&world.units, UNIT_WARRIOR, WORLD_CIV_PLAYER,
                           20, 20);
    int lone = Unit_Spawn(&world.units, UNIT_WARRIOR, WORLD_CIV_PLAYER,
                          20, 21);
    CHECK(medic_a >= 0 && medic_b >= 0 && rested >= 0);
    CHECK(moved >= 0 && lone >= 0);
    world.units.units[rested].hp = 50;
    world.units.units[moved].hp = 50;
    world.units.units[lone].hp = 50;
    world.units.units[moved].moves_left = 5;

    int enemy = Unit_Spawn(&world.units, UNIT_WARRIOR, 1, 11, 11);
    CHECK(enemy >= 0);
    CHECK(Combat_CanAttack(&world, &map, medic_a, enemy)
          == COMBAT_ERR_CIVILIAN);

    world.turn = 1;
    World_EndTurn(&world, &map);
    CHECK(world.units.units[rested].hp == 70); /* 10 neutral + 10 medic */
    CHECK(world.units.units[moved].hp == 50);  /* rested units only */
    CHECK(world.units.units[lone].hp == 60);   /* no medic nearby */

    /* Friendly territory doubles the base heal; the medic bonus stays a
     * single flat +10. */
    flat_map();
    Map_ResetFog(&map);
    World_Init(&world);
    int city_index = -1;
    CHECK(City_TryFound(&world.cities, &map, WORLD_CIV_PLAYER, 10, 10,
                        &city_index) == CITY_FOUND_OK);
    int friendly_medic = Unit_Spawn(&world.units, UNIT_MEDIC,
                                    WORLD_CIV_PLAYER, 10, 11);
    int friendly_warrior = Unit_Spawn(&world.units, UNIT_WARRIOR,
                                      WORLD_CIV_PLAYER, 11, 10);
    CHECK(city_index == 0);
    CHECK(friendly_medic >= 0 && friendly_warrior >= 0);
    world.units.units[friendly_warrior].hp = 50;
    world.player_founded = 1;
    world.turn = 1;
    World_EndTurn(&world, &map);
    CHECK(world.units.units[friendly_warrior].hp == 80); /* 20 + 10 */
}

static void test_era_and_war_state(void) {
    World_Init(&world);
    CHECK(World_PlayerEra(&world) == ERA_ANCIENT);

    world.researched = 1u << TECH_MATHEMATICS;
    CHECK(World_PlayerEra(&world) == ERA_CLASSICAL);
    world.researched |= 1u << TECH_EDUCATION;
    CHECK(World_PlayerEra(&world) == ERA_MEDIEVAL);
    world.researched |= 1u << TECH_PRINTING;
    CHECK(World_PlayerEra(&world) == ERA_RENAISSANCE);

    CHECK(!World_PlayerAtWar(&world));
    world.civs[1].active = 1;
    world.civs[0].at_war_mask = 1u << 1;
    world.civs[1].at_war_mask = 1u << 0;
    CHECK(World_AreAtWar(&world, 0, 1));
    CHECK(World_PlayerAtWar(&world));

    world.civs[2].active = 1;
    world.civs[0].at_war_mask = 0;
    world.civs[1].at_war_mask = 0;
    CHECK(!World_PlayerAtWar(&world));

    /* Barbarians are permanently hostile but must not select war music. */
    world.civs[WORLD_CIV_BARBARIAN].active = 1;
    CHECK(World_AreAtWar(&world, 0, WORLD_CIV_BARBARIAN));
    CHECK(!World_PlayerAtWar(&world));
}

int main(void) {
    test_movement_costs();
    test_unit_movement();
    test_city_founding();
    test_world_founding_and_turn();
    test_world_start_and_vision();
    test_difficulty_setup();
    test_auto_explore();
    test_medic_support();
    test_era_and_war_state();

    if (failures == 0) {
        printf("PASS: unit movement, city founding, turns, auto-explore, "
               "medic healing, music state, and start setup\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
