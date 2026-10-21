#include <stdio.h>
#include <string.h>

#include "city.h"
#include "tech.h"
#include "world.h"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void make_world(World *world) {
    memset(world, 0, sizeof(*world));
    world->turn = 4;
    world->treasury = 100;
    world->player_founded = 1;
    world->units.count = 1;
    world->units.units[0].type = UNIT_WARRIOR;
    world->units.units[0].owner = WORLD_CIV_PLAYER;
    world->units.units[0].hp = 100;
    world->units.units[0].x = 10;
    world->units.units[0].y = 10;
    world->cities.count = 1;
    world->cities.cities[0].owner = WORLD_CIV_PLAYER;
    world->cities.cities[0].name_index = 0;
    world->cities.cities[0].population = 4;
    world->cities.cities[0].x = 10;
    world->cities.cities[0].y = 10;
    world->cities.cities[0].owned_mask = (1u << CITY_CENTER_INDEX)
                                       | (1u << (CITY_CENTER_INDEX + 1));
    world->cities.cities[0].buildings = (uint16_t)(1u << BUILDING_MARKET);
    world->cities.cities[0].is_capital = 1;
}

static void test_score(void) {
    World world;
    make_world(&world);
    world.researched = (1u << TECH_POTTERY) | (1u << TECH_MINING);

    const City *city = &world.cities.cities[0];
    int expected = 10 + 4 * 6 + City_OwnedCount(city)
                 + City_BuildingCount(city) * 5
                 + 4 + 100 / 25
                 + 2 * 10 + 100 / 20;
    CHECK(World_PlayerScore(&world) == expected);
    CHECK(World_PlayerPopulation(&world) == 4);
    CHECK(World_PlayerTechCount(&world) == 2);

    world.civs[1].active = 1;
    world.civs[1].treasury = 40;
    world.cities.count = 2;
    world.cities.cities[1].owner = 1;
    world.cities.cities[1].name_index = 1;
    world.cities.cities[1].population = 2;
    world.cities.cities[1].x = 20;
    world.cities.cities[1].y = 20;
    world.cities.cities[1].owned_mask = 1u << CITY_CENTER_INDEX;
    world.cities.cities[1].buildings = 0;
    world.units.count = 2;
    world.units.units[1].type = UNIT_WARRIOR;
    world.units.units[1].owner = 1;
    world.units.units[1].hp = 100;
    world.units.units[1].x = 20;
    world.units.units[1].y = 20;

    CHECK(World_BestRivalScore(&world) > 0);
    CHECK(World_PlayerHasTopScore(&world));
    world.civs[1].treasury = 10000;
    CHECK(!World_PlayerHasTopScore(&world));
}

static void test_science_victory(void) {
    World world;
    make_world(&world);
    world.researched = 0;
    CHECK(!World_AllTechsResearched(&world));
    CHECK(!World_ScienceGoalsComplete(&world));

    world.researched = (1u << TECH_ASTRONOMY)
                     | (1u << TECH_SCIENTIFIC_METHOD);
    CHECK(World_ScienceGoalCount(&world) == 2);
    CHECK(!World_ScienceGoalsComplete(&world));
    World_CheckGameOver(&world);
    CHECK(world.game_over == WORLD_GAME_NONE);

    world.researched |= 1u << TECH_NAVIGATION;
    CHECK(World_ScienceGoalsComplete(&world));
    World_CheckGameOver(&world);
    CHECK(world.game_over == WORLD_GAME_VICTORY);
    CHECK(world.victory_type == WORLD_VICTORY_SCIENCE);

    for (int tech = 1; tech < TECH_COUNT; tech++) {
        world.researched |= 1u << tech;
    }
    CHECK(World_AllTechsResearched(&world));

    world.game_over = WORLD_GAME_NONE;
    world.victory_type = WORLD_VICTORY_NONE;
    world.player_founded = 1;
    world.cities.count = 0;
    World_CheckGameOver(&world);
    CHECK(world.game_over == WORLD_GAME_DEFEAT);
}

int main(void) {
    test_score();
    test_science_victory();

    if (failures) {
        fprintf(stderr, "%d m7 check(s) failed\n", failures);
        return 1;
    }
    printf("PASS: score, top-score ranking, and science victory\n");
    return 0;
}
