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

static void prepare_game(uint32_t seed) {
    Map_Generate(&map, seed);
    World_Begin(&world, &map);

    int settler = -1;
    int warrior = -1;
    int city_index = -1;
    for (int i = 0; i < world.units.count; i++) {
        const Unit *unit = &world.units.units[i];
        if (unit->owner != WORLD_CIV_PLAYER) continue;
        if (unit->type == UNIT_SETTLER && settler < 0) settler = i;
        if (unit->type == UNIT_WARRIOR && warrior < 0) warrior = i;
    }
    CHECK(settler >= 0);
    CHECK(warrior >= 0);
    if (settler >= 0) {
        city_index = World_FoundCity(&world, &map, settler);
        CHECK(city_index >= 0);
    }
    if (city_index >= 0) {
        City *city = &world.cities.cities[city_index];
        City_QueueAdd(city, PROD_WARRIOR);
        if (warrior >= 0) {
            /* Re-find the warrior because founding removed the settler and
             * shifted indices. */
            warrior = -1;
            for (int i = 0; i < world.units.count; i++) {
                const Unit *unit = &world.units.units[i];
                if (unit->owner == WORLD_CIV_PLAYER
                    && unit->type == UNIT_WARRIOR) {
                    warrior = i;
                    break;
                }
            }
            if (warrior >= 0 && Unit_IndexOfAt(&world.units, city->x,
                                               city->y) < 0) {
                world.units.units[warrior].x = city->x;
                world.units.units[warrior].y = city->y;
            }
        }
    }
}

static void test_full_start(void) {
    Map_Generate(&map, 0x5A5A1234u);
    World_Begin(&world, &map);
    CHECK(world.turn == 1);
    CHECK(world.units.count > 3);
    CHECK(world.cities.count == WORLD_AI_COUNT);
    CHECK(World_CountUnits(&world, WORLD_CIV_PLAYER) == 3);
    CHECK(World_PlayerCityCount(&world) == 0);
    CHECK(World_CountUnits(&world, WORLD_CIV_BARBARIAN) >= 1);

    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        CHECK(world.civs[civ].active);
        CHECK(world.civs[civ].is_ai);
        CHECK(world.civs[civ].capital_index != WORLD_CIV_NONE);
        CHECK(world.civs[civ].capital_index < world.cities.count);
        if (world.civs[civ].capital_index < world.cities.count) {
            const City *capital =
                &world.cities.cities[world.civs[civ].capital_index];
            CHECK(capital->owner == civ);
            CHECK(capital->is_capital);
        }
        CHECK(World_CountCities(&world, civ) == 1);
        CHECK(World_CountUnits(&world, civ) >= 2);
    }
    CHECK(!World_AreAtWar(&world, 0, 1));
    CHECK(World_AreAtWar(&world, 0, WORLD_CIV_BARBARIAN));
}

static void test_200_turn_simulation(void) {
    prepare_game(0x5A5A1234u);
    int initial_ai_cities = 0;
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        initial_ai_cities += World_CountCities(&world, civ);
    }
    CHECK(initial_ai_cities == WORLD_AI_COUNT);

    int ai_cities_at_40 = 0;
    int any_ai_war = 0;
    for (int turn = 0; turn < 200; turn++) {
        World_EndTurn(&world, &map);

        CHECK(world.units.count <= UNIT_MAX);
        CHECK(world.cities.count <= CITY_MAX);
        CHECK(world.units.count >= 0);
        CHECK(world.cities.count >= 0);

        for (int i = 0; i < world.units.count; i++) {
            const Unit *unit = &world.units.units[i];
            CHECK(unit->owner < WORLD_CIV_COUNT);
            CHECK(unit->type < UNIT_TYPE_COUNT);
            CHECK(unit->x >= 0 && unit->x < MAP_WIDTH);
            CHECK(unit->y >= 0 && unit->y < MAP_HEIGHT);
            CHECK(unit->hp > 0 && unit->hp <= UNIT_HP_MAX);
            CHECK(Unit_PassableTile(&map, unit->x, unit->y));
        }
        for (int i = 0; i < world.cities.count; i++) {
            const City *city = &world.cities.cities[i];
            CHECK(city->owner < WORLD_CIV_COUNT);
            CHECK(city->x >= 0 && city->x < MAP_WIDTH);
            CHECK(city->y >= 0 && city->y < MAP_HEIGHT);
            CHECK(Unit_PassableTile(&map, city->x, city->y));
        }
        if (world.turn == 40) {
            for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
                ai_cities_at_40 += World_CountCities(&world, civ);
            }
        }
        for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
            if (World_AreAtWar(&world, 0, civ)) any_ai_war = 1;
        }
    }

    int final_ai_cities = 0;
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        final_ai_cities += World_CountCities(&world, civ);
    }
    printf("ai sim: turn=%d ai_cities=%d->%d battles=%d captures=%d "
           "units=%d game_over=%d\n",
           world.turn, initial_ai_cities, final_ai_cities, world.battles,
           world.cities_captured, world.units.count, world.game_over);

    CHECK(world.turn == 201);
    CHECK(ai_cities_at_40 > initial_ai_cities);
    CHECK(final_ai_cities >= initial_ai_cities);
    CHECK(any_ai_war);
    CHECK(world.battles > 0);
    CHECK(world.units.count > 0);
    CHECK(world.cities.count > 0);
    CHECK(World_CivTechCount(&world, 1) >= 10);
    CHECK(World_CivTechCount(&world, 2) >= 10);
    CHECK(World_CivTechCount(&world, 3) >= 10);
}

static void test_long_simulation(void) {
    prepare_game(0xC0FFEE11u);

    for (int turn = 0; turn < 320; turn++) {
        World_EndTurn(&world, &map);

        CHECK(world.units.count <= UNIT_MAX);
        CHECK(world.cities.count <= CITY_MAX);
        for (int i = 0; i < world.units.count; i++) {
            const Unit *unit = &world.units.units[i];
            CHECK(unit->owner < WORLD_CIV_COUNT);
            CHECK(unit->type < UNIT_TYPE_COUNT);
            CHECK(unit->x >= 0 && unit->x < MAP_WIDTH);
            CHECK(unit->y >= 0 && unit->y < MAP_HEIGHT);
            CHECK(unit->hp > 0 && unit->hp <= UNIT_HP_MAX);
            CHECK(Unit_PassableTile(&map, unit->x, unit->y));
        }
        for (int i = 0; i < world.cities.count; i++) {
            const City *city = &world.cities.cities[i];
            CHECK(city->owner < WORLD_CIV_COUNT);
            CHECK(city->x >= 0 && city->x < MAP_WIDTH);
            CHECK(city->y >= 0 && city->y < MAP_HEIGHT);
            CHECK(Unit_PassableTile(&map, city->x, city->y));
        }
    }

    printf("ai long sim: turn=%d ai_tech=%d/%d/%d units=%d cities=%d "
           "game_over=%d\n",
           world.turn, World_CivTechCount(&world, 1),
           World_CivTechCount(&world, 2), World_CivTechCount(&world, 3),
           world.units.count, world.cities.count, world.game_over);

    CHECK(world.turn == 321);
    CHECK(World_CivTechCount(&world, 1) >= 20);
    CHECK(World_CivTechCount(&world, 2) >= 20);
    CHECK(World_CivTechCount(&world, 3) >= 20);
    CHECK(world.units.count > 0);
    CHECK(world.cities.count > 0);
}

static void test_ai_declares_war(void) {
    prepare_game(0x13572468u);
    int war_declared = 0;
    for (int turn = 0; turn < 40; turn++) {
        World_EndTurn(&world, &map);
        for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
            if (World_AreAtWar(&world, 0, civ)) war_declared = 1;
        }
    }
    CHECK(war_declared);
}

int main(void) {
    test_full_start();
    test_200_turn_simulation();
    test_long_simulation();
    test_ai_declares_war();

    if (failures == 0) {
        printf("PASS: rival starts, AI expansion, war, and 200-turn simulation\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
