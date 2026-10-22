#include <stdio.h>
#include <string.h>

#include "city.h"
#include "combat.h"
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

static void flat_map(void) {
    memset(&map, 0, sizeof(map));
    map.seed = 7;
    map.start_x = 10;
    map.start_y = 10;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map.tiles[y][x].terrain = TERRAIN_GRASSLAND;
        }
    }
}

static void fresh_world(void) {
    flat_map();
    World_Init(&world);
    UnitPool_Init(&world.units);
    CityPool_Init(&world.cities);
}

static void test_damage_table(void) {
    CHECK(Combat_Damage(6, 6) == 30);
    CHECK(Combat_Damage(8, 6) == 40);
    CHECK(Combat_Damage(6, 8) == 20);
    CHECK(Combat_Damage(20, 0) == 95);
    CHECK(Combat_Damage(0, 20) == 5);
    CHECK(Combat_Damage(8, 6) > Combat_Damage(7, 6));
    CHECK(Combat_Damage(6, 7) < Combat_Damage(6, 6));
}

static void test_attack_counter_and_death(void) {
    fresh_world();
    int attacker = Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 10, 10);
    int defender = Unit_Spawn(&world.units, UNIT_WARRIOR, 1, 11, 10);
    CHECK(attacker == 0);
    CHECK(defender == 1);
    CHECK(!World_AreAtWar(&world, 0, 1));
    CHECK(Combat_CanAttack(&world, &map, attacker, defender) == COMBAT_OK);

    CombatResult result;
    CHECK(Combat_AttackUnit(&world, &map, attacker, defender, &result)
          == COMBAT_OK);
    CHECK(result.attacker_damage == 30);
    CHECK(result.defender_damage == 30);
    CHECK(result.defender_hp == 70);
    CHECK(result.attacker_hp == 70);
    CHECK(world.units.units[attacker].hp == 70);
    CHECK(world.units.units[defender].hp == 70);
    CHECK(world.units.units[attacker].moves_left == 0);
    CHECK(world.battles == 1);
    CHECK(World_AreAtWar(&world, 0, 1));
    CHECK(world.civs[0].at_war_mask & (1u << 1));
    CHECK(world.civs[1].at_war_mask & (1u << 0));

    /* A lethal blow removes the defender and advances the attacker. */
    world.units.units[attacker].hp = 60;
    world.units.units[attacker].moves_left = 6;
    world.units.units[defender].hp = 10;
    CHECK(Combat_AttackUnit(&world, &map, attacker, defender, &result)
          == COMBAT_OK);
    CHECK(result.defender_died);
    CHECK(!result.attacker_died);
    CHECK(world.units.count == 1);
    CHECK(world.units.units[0].x == 11);
    CHECK(world.units.units[0].y == 10);
    CHECK(world.units.units[0].hp == 60);
}

static void test_errors_and_civilian(void) {
    fresh_world();
    int warrior = Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 10, 10);
    int friendly = Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 11, 10);
    int far = Unit_Spawn(&world.units, UNIT_WARRIOR, 1, 13, 10);
    int settler = Unit_Spawn(&world.units, UNIT_SETTLER, 0, 10, 11);
    CHECK(Combat_CanAttack(&world, &map, warrior, friendly)
          == COMBAT_ERR_FRIENDLY);
    CHECK(Combat_CanAttack(&world, &map, warrior, far)
          == COMBAT_ERR_NOT_ADJACENT);
    CHECK(Combat_CanAttack(&world, &map, settler, far)
          == COMBAT_ERR_CIVILIAN);
    world.units.units[warrior].moves_left = 0;
    CHECK(Combat_CanAttack(&world, &map, warrior, far)
          == COMBAT_ERR_NO_MOVES);
}

static void test_terrain_and_city_defense(void) {
    fresh_world();
    map.tiles[10][11].terrain = TERRAIN_FOREST;
    CHECK(Combat_DefenseStrength(&world, &map, 11, 10, 1, 6) == 7);

    int city_index = -1;
    CHECK(City_TryFound(&world.cities, &map, 1, 20, 20, &city_index)
          == CITY_FOUND_OK);
    CHECK(city_index >= 0);
    int defender = Unit_Spawn(&world.units, UNIT_WARRIOR, 1, 20, 20);
    CHECK(Combat_DefenseStrength(&world, &map, 20, 20, 1, 6) == 7);
    int attacker = Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 19, 20);
    CHECK(Combat_AttackUnit(&world, &map, attacker, defender, NULL)
          == COMBAT_OK);
    CHECK(world.units.units[defender].hp == 75);
    CHECK(world.units.units[attacker].hp == 70);
}

static void test_city_assault(void) {
    fresh_world();
    int city_index = -1;
    CHECK(City_TryFound(&world.cities, &map, 1, 12, 10, &city_index)
          == CITY_FOUND_OK);
    City *city = &world.cities.cities[city_index];
    CHECK(city->hp == CITY_HP_MAX);
    CHECK(city->defense == CITY_DEFENSE_BASE);
    CHECK(city->defense_max == CITY_DEFENSE_BASE);

    int attacker = Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 11, 10);
    CHECK(Combat_CanAttackCity(&world, &map, attacker, city_index)
          == COMBAT_OK);
    CHECK(!World_AreAtWar(&world, 0, 1));

    CombatResult result;
    CHECK(Combat_AttackCity(&world, &map, attacker, city_index, &result)
          == COMBAT_OK);
    CHECK(!result.city_captured);
    CHECK(result.city_defense_damage > 0);
    CHECK(result.city_hp_damage == 0);
    CHECK(city->defense == CITY_DEFENSE_BASE - result.city_defense_damage);
    CHECK(city->hp == CITY_HP_MAX);
    CHECK(city->owner == 1);
    CHECK(world.units.units[attacker].moves_left == 0);
    CHECK(world.units.units[attacker].x == 11);
    CHECK(world.units.units[attacker].y == 10);
    CHECK(World_AreAtWar(&world, 0, 1));

    /* Deplete the defense pool, then knock the city down to one HP. */
    city->defense = 0;
    city->hp = 1;
    world.units.units[attacker].moves_left = 3;
    CHECK(Combat_AttackCity(&world, &map, attacker, city_index, &result)
          == COMBAT_OK);
    CHECK(result.city_captured);
    CHECK(result.city_hp_damage == 1);
    CHECK(city->owner == 0);
    CHECK(city->hp == CITY_HP_MAX);
    CHECK(city->defense == CITY_DEFENSE_BASE);
    CHECK(world.units.units[attacker].x == 12);
    CHECK(world.units.units[attacker].y == 10);
    CHECK(world.units.units[attacker].moves_left == 0);
    CHECK(world.cities_captured == 1);
    CHECK(world.player_founded);
}

static void test_garrison_shields_city(void) {
    fresh_world();
    int city_index = -1;
    CHECK(City_TryFound(&world.cities, &map, 1, 12, 10, &city_index)
          == CITY_FOUND_OK);
    City *city = &world.cities.cities[city_index];
    int city_hp = city->hp;
    int city_defense = city->defense;
    int garrison = Unit_Spawn(&world.units, UNIT_WARRIOR, 1, 12, 10);
    int attacker = Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 11, 10);
    CHECK(Combat_CanAttackCity(&world, &map, attacker, city_index)
          == COMBAT_ERR_DEFENDED);

    world.units.units[garrison].hp = 1;
    CHECK(Combat_AttackUnit(&world, &map, attacker, garrison, NULL)
          == COMBAT_OK);
    CHECK(world.units.count == 1);
    CHECK(city->owner == 1);
    CHECK(city->hp == city_hp);
    CHECK(city->defense == city_defense);
    CHECK(world.units.units[attacker].x == 11);
    CHECK(world.units.units[attacker].y == 10);
    attacker = Unit_IndexOfAt(&world.units, 11, 10);
    CHECK(attacker >= 0);
    world.units.units[attacker].moves_left = 3;
    CHECK(Combat_CanAttackCity(&world, &map, attacker, city_index)
          == COMBAT_OK);
}

static void test_walls_repair_and_regen(void) {
    fresh_world();
    int city_index = -1;
    CHECK(City_TryFound(&world.cities, &map, 1, 12, 10, &city_index)
          == CITY_FOUND_OK);
    City *city = &world.cities.cities[city_index];

    CHECK(City_RepairDefense(city) == 0);
    city->defense = 50;
    CHECK(City_RepairDefense(city) == CITY_REPAIR_BASE);
    CHECK(city->defense == 50 + CITY_REPAIR_BASE);

    city->buildings = (uint16_t)(1u << BUILDING_WALLS);
    City_UpdateCombat(city);
    CHECK(city->defense_max == CITY_WALL_DEFENSE);
    CHECK(city->defense == 75);          /* raising the cap does not heal */
    CHECK(City_RepairDefense(city) == CITY_REPAIR_WALL);
    CHECK(city->defense == 75 + CITY_REPAIR_WALL);
    city->defense = city->defense_max;
    CHECK(City_RepairDefense(city) == 0);

    city->hp = 40;
    city->last_hit_turn = 7;
    City_RecoverHP(city, 7);
    CHECK(city->hp == 40);
    City_RecoverHP(city, 8);
    CHECK(city->hp == 40 + CITY_HP_REGEN);
    City_RecoverHP(city, 9);
    CHECK(city->hp == 40 + CITY_HP_REGEN * 2);
}

static void test_game_over_conditions(void) {
    fresh_world();
    world.player_founded = 1;
    CHECK(World_PlayerCityCount(&world) == 0);
    World_CheckGameOver(&world);
    CHECK(world.game_over == WORLD_GAME_DEFEAT);

    fresh_world();
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        world.civs[civ].active = 1;
    }
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        int city_index = -1;
        CHECK(City_TryFound(&world.cities, &map, 0, 8 + civ * 4, 10,
                            &city_index) == CITY_FOUND_OK);
        world.civs[civ].capital_index = (uint8_t)city_index;
    }
    World_CheckGameOver(&world);
    CHECK(world.game_over == WORLD_GAME_VICTORY);
}

int main(void) {
    test_damage_table();
    test_attack_counter_and_death();
    test_errors_and_civilian();
    test_terrain_and_city_defense();
    test_city_assault();
    test_garrison_shields_city();
    test_walls_repair_and_regen();
    test_game_over_conditions();

    if (failures == 0) {
        printf("PASS: combat, city assault, garrison, walls, repair, and regen\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
