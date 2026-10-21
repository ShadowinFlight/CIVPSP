#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "city.h"
#include "data.h"
#include "map.h"
#include "tech.h"
#include "world.h"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static int popcount32(uint32_t value) {
    int count = 0;
    while (value) {
        count += (int)(value & 1u);
        value >>= 1;
    }
    return count;
}

static void flat_map(Map *map, int terrain) {
    memset(map, 0, sizeof(*map));
    map->seed = 7;
    map->start_x = 10;
    map->start_y = 10;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->tiles[y][x].terrain = (uint8_t)terrain;
            map->tiles[y][x].explored = 1;
            map->tiles[y][x].visible = 1;
        }
    }
}

static void make_city(World *world, City **out_city) {
    memset(world, 0, sizeof(*world));
    world->turn = 1;
    world->treasury = 100;
    world->player_founded = 1;
    world->cities.count = 1;

    City *city = &world->cities.cities[0];
    memset(city, 0, sizeof(*city));
    city->owner = WORLD_CIV_PLAYER;
    city->name_index = 0;
    city->population = 2;
    city->x = 10;
    city->y = 10;
    city->is_capital = 1;
    city->owned_mask = (1u << CITY_CENTER_INDEX)
                     | (1u << (CITY_CENTER_INDEX + 1))
                     | (1u << (CITY_CENTER_INDEX + CITY_WORK_DIAMETER));
    city->worked_mask = (1u << (CITY_CENTER_INDEX + 1))
                      | (1u << (CITY_CENTER_INDEX + CITY_WORK_DIAMETER));
    if (out_city) *out_city = city;
}

static void test_tech_tree(void) {
    CHECK(TECH_COUNT == 31); /* TECH_NONE plus the 30 planned techs. */
    int era_count[ERA_COUNT] = {0};
    for (int tech = 1; tech < TECH_COUNT; tech++) {
        const TechDef *def = &g_tech_defs[tech];
        CHECK(def->cost > 0);
        CHECK(def->era < ERA_COUNT);
        era_count[def->era]++;
        CHECK(def->prereq1 >= -1 && def->prereq1 < TECH_COUNT);
        CHECK(def->prereq2 >= -1 && def->prereq2 < TECH_COUNT);
        CHECK(def->prereq1 != tech);
        CHECK(def->prereq2 != tech);
        if (def->effect != TECH_EFFECT_NONE) {
            CHECK(def->effect_value > 0);
        }
    }
    CHECK(era_count[ERA_ANCIENT] == 8);
    CHECK(era_count[ERA_CLASSICAL] == 8);
    CHECK(era_count[ERA_MEDIEVAL] == 7);
    CHECK(era_count[ERA_RENAISSANCE] == 7);

    CHECK(!Tech_CanSelect(0, TECH_WRITING));
    CHECK(Tech_CanSelect(0, TECH_POTTERY));
    CHECK(Tech_CanSelect(1u << TECH_POTTERY, TECH_WRITING));
    CHECK(Tech_HasPrereqs(1u << TECH_POTTERY, TECH_WRITING));

    uint32_t all = 0;
    for (int tech = 1; tech < TECH_COUNT; tech++) all |= 1u << tech;
    CHECK(Tech_CountKnown(all) == 30);
    CHECK(Tech_CountKnown(0) == 0);

    CHECK(Tech_Effect(TECH_IRRIGATION) == TECH_EFFECT_FARM_FOOD);
    CHECK(Tech_Effect(TECH_EDUCATION) == TECH_EFFECT_UNIVERSITY_SCIENCE);
    CHECK(Tech_Effect(TECH_MACHINERY) == TECH_EFFECT_WORKSHOP_PROD);
    CHECK(Tech_Effect(TECH_FEUDALISM) == TECH_EFFECT_WALL_DEFENSE);
    CHECK(Tech_Effect(TECH_PRINTING) == TECH_EFFECT_RESEARCH_PCT);
}

static void test_tech_effects(void) {
    Map map;
    flat_map(&map, TERRAIN_GRASSLAND);
    map.tiles[10][11].improvement = IMPROVEMENT_FARM;
    map.tiles[11][10].improvement = IMPROVEMENT_FARM;

    World world;
    City *city = NULL;
    make_city(&world, &city);
    city->buildings = (uint16_t)((1u << BUILDING_LIBRARY)
                               | (1u << BUILDING_UNIVERSITY)
                               | (1u << BUILDING_MARKET)
                               | (1u << BUILDING_WORKSHOP)
                               | (1u << BUILDING_BARRACKS)
                               | (1u << BUILDING_WALLS));

    CityYield base;
    CityYield with_tech;
    City_ComputeYieldsWithTech(city, &map, 0, &base);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_IRRIGATION,
                               &with_tech);
    CHECK(with_tech.food == base.food + 2);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_PHILOSOPHY,
                               &with_tech);
    CHECK(with_tech.science == base.science + 1);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_EDUCATION,
                               &with_tech);
    CHECK(with_tech.science == base.science + 1);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_GUILDS, &with_tech);
    CHECK(with_tech.gold == base.gold + 1);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_MACHINERY,
                               &with_tech);
    CHECK(with_tech.production == base.production + 1);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_MILITARY_SCIENCE,
                               &with_tech);
    CHECK(with_tech.production == base.production + 1);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_SAILING, &with_tech);
    CHECK(with_tech.gold == base.gold + 1);

    City_ComputeYieldsWithTech(city, &map, 1u << TECH_THEOLOGY, &with_tech);
    CHECK(with_tech.science == base.science + 1);

    int base_defense = City_DefenseWithTech(city, 0);
    CHECK(City_DefenseWithTech(city, 1u << TECH_FEUDALISM)
          == base_defense + 1);

    /* Printing and Scientific Method each add 10% empire science. */
    World process;
    make_city(&process, NULL);
    City *second = &process.cities.cities[1];
    *second = process.cities.cities[0];
    second->x = 20;
    second->y = 10;
    process.cities.count = 2;
    process.cities.cities[0].buildings =
        (uint16_t)((1u << BUILDING_LIBRARY) | (1u << BUILDING_UNIVERSITY));
    second->buildings = process.cities.cities[0].buildings;

    process.researched = 0;
    process.research_type = TECH_NONE;
    World_ProcessCities(&process, &map);
    int base_science = process.science_per_turn;
    CHECK(base_science >= 10);

    process.researched = 1u << TECH_PRINTING;
    process.research_type = TECH_NONE;
    process.science = 0;
    World_ProcessCities(&process, &map);
    CHECK(process.science_per_turn == base_science + base_science / 10);

    process.researched = (1u << TECH_PRINTING)
                       | (1u << TECH_SCIENTIFIC_METHOD);
    process.research_type = TECH_NONE;
    process.science = 0;
    World_ProcessCities(&process, &map);
    CHECK(process.science_per_turn == base_science + base_science / 5);
}

static void test_ai_research(void) {
    World world;
    memset(&world, 0, sizeof(world));

    world.turn = 1;
    uint32_t turn_1 = World_CivResearched(&world, 1);
    CHECK(turn_1 != 0);
    CHECK(World_CivTechCount(&world, 1) == popcount32(turn_1));
    CHECK(World_CivResearched(&world, WORLD_CIV_BARBARIAN) == 0);

    world.turn = 40;
    uint32_t turn_40 = World_CivResearched(&world, 1);
    CHECK((turn_1 & ~turn_40) == 0);
    CHECK(popcount32(turn_40) > popcount32(turn_1));

    world.turn = 160;
    uint32_t turn_160 = World_CivResearched(&world, 1);
    CHECK((turn_40 & ~turn_160) == 0);
    CHECK(popcount32(turn_160) > popcount32(turn_40));

    for (int tech = 1; tech < TECH_COUNT; tech++) {
        if (turn_160 & (1u << tech)) {
            CHECK(Tech_HasPrereqs(turn_160, tech));
        }
    }

    world.researched = (1u << TECH_POTTERY) | (1u << TECH_MINING);
    CHECK(World_CivResearched(&world, WORLD_CIV_PLAYER)
          == world.researched);
}

static void test_ai_advanced_units(void) {
    Map map;
    flat_map(&map, TERRAIN_GRASSLAND);

    World world;
    memset(&world, 0, sizeof(world));
    world.turn = 200;
    world.civs[1].active = 1;
    world.civs[1].is_ai = 1;
    world.civs[1].capital_index = 0;
    world.cities.count = 1;

    City *city = &world.cities.cities[0];
    city->owner = 1;
    city->name_index = 0;
    city->population = 2;
    city->x = 10;
    city->y = 10;
    city->owned_mask = 1u << CITY_CENTER_INDEX;
    city->is_capital = 1;

    /* A settler stops the AI from choosing expansion first.  With no military
     * and Iron Working known, the next military item must be a swordsman. */
    world.units.count = 1;
    world.units.units[0].type = UNIT_SETTLER;
    world.units.units[0].owner = 1;
    world.units.units[0].hp = UNIT_HP_MAX;
    world.units.units[0].x = 20;
    world.units.units[0].y = 20;

    Ai_ProcessTurn(&world, &map);
    CHECK(city->queue_count > 0);
    CHECK(city->queue[0] == PROD_SWORDSMAN);
}

static void test_science_and_score_limit(void) {
    World world;
    make_city(&world, NULL);

    world.researched = (1u << TECH_ASTRONOMY)
                     | (1u << TECH_SCIENTIFIC_METHOD);
    CHECK(World_ScienceGoalCount(&world) == 2);
    CHECK(!World_ScienceGoalsComplete(&world));
    World_CheckGameOver(&world);
    CHECK(world.game_over == WORLD_GAME_NONE);

    world.researched |= 1u << TECH_NAVIGATION;
    CHECK(World_ScienceGoalCount(&world) == WORLD_SCIENCE_GOAL_COUNT);
    CHECK(World_ScienceGoalsComplete(&world));
    World_CheckGameOver(&world);
    CHECK(world.game_over == WORLD_GAME_VICTORY);
    CHECK(world.victory_type == WORLD_VICTORY_SCIENCE);

    world.game_over = WORLD_GAME_NONE;
    world.victory_type = WORLD_VICTORY_NONE;
    world.researched = 0;
    int base_score = World_PlayerScore(&world);
    world.researched = (1u << TECH_POTTERY) | (1u << TECH_MINING);
    CHECK(World_PlayerScore(&world) == base_score + 20);

    world.turn = WORLD_STANDARD_TURN_LIMIT + 1;
    World_FinishScoreGame(&world);
    CHECK(world.game_over == WORLD_GAME_VICTORY);
    CHECK(world.victory_type == WORLD_VICTORY_SCORE);

    world.game_over = WORLD_GAME_NONE;
    world.victory_type = WORLD_VICTORY_NONE;
    world.civs[1].active = 1;
    world.civs[1].treasury = 100000;
    CHECK(!World_PlayerHasTopScore(&world));
    World_FinishScoreGame(&world);
    CHECK(world.game_over == WORLD_GAME_DEFEAT);
    CHECK(world.victory_type == WORLD_VICTORY_SCORE);

    world.game_over = WORLD_GAME_VICTORY;
    world.victory_type = WORLD_VICTORY_SCIENCE;
    World_FinishScoreGame(&world);
    CHECK(world.game_over == WORLD_GAME_VICTORY);
    CHECK(world.victory_type == WORLD_VICTORY_SCIENCE);
}

int main(void) {
    test_tech_tree();
    test_tech_effects();
    test_ai_research();
    test_ai_advanced_units();
    test_science_and_score_limit();

    if (failures) {
        fprintf(stderr, "%d m8 check(s) failed\n", failures);
        return 1;
    }
    printf("PASS: 30-tech tree, tech effects, AI research, science goals, "
           "and standard score limit\n");
    return 0;
}
