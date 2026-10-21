#include "combat.h"

#include <string.h>

#include "city.h"
#include "data.h"

static int adjacent(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax;
    int dy = ay > by ? ay - by : by - ay;
    return dx <= 1 && dy <= 1 && (dx != 0 || dy != 0);
}

int Combat_Damage(int attack_strength, int defense_strength) {
    int damage = 30 + (attack_strength - defense_strength) * 5;
    if (damage < 5) damage = 5;
    if (damage > 95) damage = 95;
    return damage;
}

int Combat_DefenseStrength(const World *world, const Map *map, int x, int y,
                           int owner, int base_strength) {
    (void)world;
    if (!map || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
        return base_strength;
    }

    const City *city = City_AtConst(world ? &world->cities : NULL, x, y);
    if (city && city->owner == owner) {
        return base_strength
             + City_DefenseWithTech(city,
                                    World_CivResearched(world, city->owner));
    }

    int terrain = Map_TerrainAt(map, x, y);
    if (terrain == TERRAIN_FOREST || terrain == TERRAIN_HILLS) {
        return base_strength + 1;
    }
    return base_strength;
}

int Combat_CanAttack(const World *world, const Map *map, int attacker_index,
                     int defender_index) {
    if (!world || !map) return COMBAT_ERR_INVALID;
    if (attacker_index < 0 || attacker_index >= world->units.count) {
        return COMBAT_ERR_INVALID;
    }
    if (defender_index < 0 || defender_index >= world->units.count) {
        return COMBAT_ERR_INVALID;
    }
    if (attacker_index == defender_index) return COMBAT_ERR_FRIENDLY;

    const Unit *attacker = &world->units.units[attacker_index];
    const Unit *defender = &world->units.units[defender_index];
    if (attacker->owner == defender->owner) return COMBAT_ERR_FRIENDLY;
    if (g_unit_defs[attacker->type].strength <= 0) return COMBAT_ERR_CIVILIAN;
    if (!World_AreAtWar(world, attacker->owner, defender->owner)
        && attacker->owner != WORLD_CIV_BARBARIAN
        && defender->owner != WORLD_CIV_BARBARIAN
        && !World_CanDeclareWar(world, attacker->owner, defender->owner)) {
        return COMBAT_ERR_TREATY;
    }
    if (attacker->moves_left == 0) return COMBAT_ERR_NO_MOVES;
    if (!adjacent(attacker->x, attacker->y, defender->x, defender->y)) {
        return COMBAT_ERR_NOT_ADJACENT;
    }
    if (defender->hp == 0) return COMBAT_ERR_INVALID;
    return COMBAT_OK;
}

int Combat_AttackUnit(World *world, Map *map, int attacker_index,
                      int defender_index, CombatResult *result) {
    CombatResult local;
    if (!result) result = &local;
    memset(result, 0, sizeof(*result));

    int code = Combat_CanAttack(world, map, attacker_index, defender_index);
    if (code != COMBAT_OK) return code;

    Unit *attacker = &world->units.units[attacker_index];
    Unit *defender = &world->units.units[defender_index];
    int attacker_owner = attacker->owner;
    int defender_owner = defender->owner;
    int target_x = defender->x;
    int target_y = defender->y;
    int defender_type = defender->type;

    if (attacker_owner != WORLD_CIV_BARBARIAN
        && defender_owner != WORLD_CIV_BARBARIAN
        && !World_AreAtWar(world, attacker_owner, defender_owner)) {
        World_DeclareWar(world, attacker_owner, defender_owner);
    }

    int attack_strength = g_unit_defs[attacker->type].strength;
    int defense_base = g_unit_defs[defender->type].strength;
    int defense_strength = Combat_DefenseStrength(world, map, target_x,
                                                  target_y, defender_owner,
                                                  defense_base);

    int attacker_hp = attacker->hp ? attacker->hp : UNIT_HP_MAX;
    int defender_hp = defender->hp ? defender->hp : UNIT_HP_MAX;

    int damage = Combat_Damage(attack_strength, defense_strength);
    defender_hp -= damage;
    result->attacker_damage = damage;

    if (defender_hp > 0) {
        int counter = Combat_Damage(defense_base, attack_strength);
        attacker_hp -= counter;
        result->defender_damage = counter;
    }

    if (attacker_hp < 0) attacker_hp = 0;
    if (defender_hp < 0) defender_hp = 0;
    attacker->hp = (uint8_t)attacker_hp;
    defender->hp = (uint8_t)defender_hp;
    attacker->moves_left = 0;
    result->attacker_hp = attacker_hp;
    result->defender_hp = defender_hp;
    result->defender_died = defender_hp == 0;
    result->attacker_died = attacker_hp == 0;
    result->city_index = -1;
    world->battles++;

    int defender_x = defender->x;
    int defender_y = defender->y;
    (void)defender_type;

    if (result->defender_died) {
        Unit_RemoveAt(&world->units, defender_index);
        if (defender_index < attacker_index) attacker_index--;
    }
    if (result->attacker_died) {
        Unit_RemoveAt(&world->units, attacker_index);
        return COMBAT_OK;
    }

    attacker = &world->units.units[attacker_index];
    if (result->defender_died) {
        int city_index = City_IndexOfAt(&world->cities, defender_x,
                                        defender_y);
        if (city_index >= 0
            && world->cities.cities[city_index].owner != attacker_owner) {
            if (World_CaptureCity(world, map, city_index, attacker_owner)) {
                result->city_captured = 1;
                result->city_index = city_index;
            }
        }
        if (!result->city_captured) {
            attacker->x = (int8_t)defender_x;
            attacker->y = (int8_t)defender_y;
        } else {
            attacker->x = (int8_t)defender_x;
            attacker->y = (int8_t)defender_y;
        }
    }
    return COMBAT_OK;
}

int Combat_CanCaptureCity(const World *world, const Map *map,
                          int attacker_index, int city_index) {
    if (!world || !map) return COMBAT_ERR_INVALID;
    if (attacker_index < 0 || attacker_index >= world->units.count) {
        return COMBAT_ERR_INVALID;
    }
    if (city_index < 0 || city_index >= world->cities.count) {
        return COMBAT_ERR_NO_CITY;
    }

    const Unit *attacker = &world->units.units[attacker_index];
    const City *city = &world->cities.cities[city_index];
    if (attacker->owner == city->owner) return COMBAT_ERR_FRIENDLY;
    if (g_unit_defs[attacker->type].strength <= 0) return COMBAT_ERR_CIVILIAN;
    if (!World_AreAtWar(world, attacker->owner, city->owner)
        && attacker->owner != WORLD_CIV_BARBARIAN
        && city->owner != WORLD_CIV_BARBARIAN
        && !World_CanDeclareWar(world, attacker->owner, city->owner)) {
        return COMBAT_ERR_TREATY;
    }
    if (attacker->moves_left == 0) return COMBAT_ERR_NO_MOVES;
    if (!adjacent(attacker->x, attacker->y, city->x, city->y)) {
        return COMBAT_ERR_NOT_ADJACENT;
    }
    if (Unit_IndexOfAt(&world->units, city->x, city->y) >= 0) {
        return COMBAT_ERR_DEFENDED;
    }
    return COMBAT_OK;
}

int Combat_CaptureCity(World *world, Map *map, int attacker_index,
                       int city_index, CombatResult *result) {
    CombatResult local;
    if (!result) result = &local;
    memset(result, 0, sizeof(*result));
    result->city_index = -1;

    int code = Combat_CanCaptureCity(world, map, attacker_index, city_index);
    if (code != COMBAT_OK) return code;

    Unit *attacker = &world->units.units[attacker_index];
    City *city = &world->cities.cities[city_index];
    int attacker_owner = attacker->owner;
    int defender_owner = city->owner;

    if (attacker_owner != WORLD_CIV_BARBARIAN
        && defender_owner != WORLD_CIV_BARBARIAN
        && !World_AreAtWar(world, attacker_owner, defender_owner)) {
        World_DeclareWar(world, attacker_owner, defender_owner);
    }

    if (!World_CaptureCity(world, map, city_index, attacker_owner)) {
        return COMBAT_ERR_INVALID;
    }

    attacker = &world->units.units[attacker_index];
    attacker->x = city->x;
    attacker->y = city->y;
    attacker->moves_left = 0;
    result->city_captured = 1;
    result->city_index = city_index;
    world->battles++;
    return COMBAT_OK;
}
