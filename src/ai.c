#include "ai.h"

#include <string.h>

#include "combat.h"
#include "data.h"
#include "gen/strings_gen.h"
#include "log.h"
#include "tech.h"
#include "turn_report.h"

#define AI_MAX_CITIES       4
#define AI_SITE_RADIUS      10
#define AI_TARGET_RANGE     64
#define AI_MAX_STEPS        16
#define BARBARIAN_MAX_UNITS 4
#define BARBARIAN_SPAWN_TURN_INTERVAL 8

static int chebyshev(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax;
    int dy = ay > by ? ay - by : by - ay;
    return dx > dy ? dx : dy;
}

static int adjacent(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax;
    int dy = ay > by ? ay - by : by - ay;
    return dx <= 1 && dy <= 1 && (dx != 0 || dy != 0);
}

/* Battles involving the player are always relevant.  AI-vs-AI battles are
 * reported only while the player can actually see the tile, so the briefing
 * does not leak units through fog of war. */
static int report_battle_relevant(const Map *map, int x, int y,
                                  int owner_a, int owner_b) {
    if (owner_a == WORLD_CIV_PLAYER || owner_b == WORLD_CIV_PLAYER) return 1;
    if (!map || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
        return 0;
    }
    return map->tiles[y][x].visible;
}

static int city_has_unit(const World *world, const City *city) {
    return Unit_IndexOfAt(&world->units, city->x, city->y) >= 0;
}

static int count_owner_type(const World *world, int owner, int type) {
    int count = 0;
    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        if (unit->owner == owner && unit->type == type) count++;
    }
    return count;
}

/* ------------------------------------------------------------------------- */
/* Diplomacy                                                                 */
/* ------------------------------------------------------------------------- */

static void ai_update_diplomacy(World *world) {
    int easy = world->difficulty == WORLD_DIFFICULTY_EASY;
    int hard = world->difficulty == WORLD_DIFFICULTY_HARD;
    int first_turn = hard ? 8 : (easy ? 20 : 12);
    if (world->turn < first_turn) return;
    if (World_PlayerCityCount(world) == 0) return;

    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        if (!world->civs[civ].active) continue;

        if (!World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) {
            int trigger = first_turn + (civ - 1) * (hard ? 2 : 4);
            int interval = hard ? 10 : 16;
            int armies = easy ? 4 : (hard ? 2 : 3);
            int relation_limit = easy ? 10 : (hard ? 35 : 20);
            if (world->turn >= trigger
                && (world->turn - trigger) % interval == 0
                && World_MilitaryCount(world, civ) >= armies
                && World_Relation(world, civ, WORLD_CIV_PLAYER)
                       < relation_limit
                && World_CanDeclareWar(world, civ, WORLD_CIV_PLAYER)) {
                World_DeclareWar(world, civ, WORLD_CIV_PLAYER);
                TurnReport_AddWar(civ, WORLD_CIV_PLAYER);
            }
        }
        /* Peace is now negotiated through Diplomacy_ProcessTurn: the AI
         * proposes it when its army is spent, and the player accepts or
         * rejects through the diplomacy screen. */
    }
}

/* ------------------------------------------------------------------------- */
/* City production and expansion                                             */
/* ------------------------------------------------------------------------- */

static ProductionType ai_best_military(const World *world, int owner) {
    uint32_t researched = World_CivResearched(world, owner);
    if (Tech_IsKnown(researched, TECH_IRON_WORKING)) return PROD_SWORDSMAN;
    if (Tech_IsKnown(researched, TECH_HORSEBACK)) return PROD_HORSEMAN;
    return PROD_WARRIOR;
}

static int ai_knows(const World *world, int owner, int tech) {
    return Tech_IsKnown(World_CivResearched(world, owner), tech);
}

static void ai_choose_production(World *world, City *city) {
    if (!city || city->queue_count > 0) return;

    int owner = city->owner;
    int cities = World_CountCities(world, owner);
    int military = World_MilitaryCount(world, owner);
    int settlers = count_owner_type(world, owner, UNIT_SETTLER);
    ProductionType military_type = ai_best_military(world, owner);

    if (world->units.count >= UNIT_MAX - 3) {
        if (!City_HasBuilding(city, BUILDING_MONUMENT)) {
            City_QueueAdd(city, PROD_MONUMENT);
        }
        return;
    }

    /* Expand first, but keep a small garrison alive.  AI research is derived
     * from the turn counter, so advanced units and buildings enter play
     * gradually without adding save fields. */
    if (cities < AI_MAX_CITIES && settlers == 0 && world->turn >= 3) {
        if (City_QueueAdd(city, PROD_SETTLER) == CITY_QUEUE_OK) return;
    }
    if (military < cities * 2 + 1) {
        if (City_QueueAdd(city, military_type) == CITY_QUEUE_OK) return;
    }
    if (!City_HasBuilding(city, BUILDING_MONUMENT)) {
        if (City_QueueAdd(city, PROD_MONUMENT) == CITY_QUEUE_OK) return;
    }
    if (cities < AI_MAX_CITIES && settlers == 0) {
        if (City_QueueAdd(city, PROD_SETTLER) == CITY_QUEUE_OK) return;
    }
    if (military < cities * 3 + 2) {
        if (City_QueueAdd(city, military_type) == CITY_QUEUE_OK) return;
    }

    if (ai_knows(world, owner, TECH_WRITING)
        && !City_HasBuilding(city, BUILDING_LIBRARY)
        && city->population >= 2) {
        if (City_QueueAdd(city, PROD_LIBRARY) == CITY_QUEUE_OK) return;
    }
    if (ai_knows(world, owner, TECH_CURRENCY)
        && !City_HasBuilding(city, BUILDING_MARKET)) {
        if (City_QueueAdd(city, PROD_MARKET) == CITY_QUEUE_OK) return;
    }
    if (ai_knows(world, owner, TECH_ENGINEERING)
        && !City_HasBuilding(city, BUILDING_WORKSHOP)) {
        if (City_QueueAdd(city, PROD_WORKSHOP) == CITY_QUEUE_OK) return;
    }
    if (ai_knows(world, owner, TECH_IRON_WORKING)
        && !City_HasBuilding(city, BUILDING_UNIVERSITY)
        && city->population >= 3) {
        if (City_QueueAdd(city, PROD_UNIVERSITY) == CITY_QUEUE_OK) return;
    }
}

static void ai_manage_cities(World *world) {
    for (int i = 0; i < world->cities.count; i++) {
        City *city = &world->cities.cities[i];
        if (city->owner < 1 || city->owner > WORLD_AI_COUNT) continue;
        ai_choose_production(world, city);
    }
}

static int site_score(const Map *map, int x, int y, int anchor_x,
                      int anchor_y) {
    int score = 0;
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int tx = x + dx;
            int ty = y + dy;
            if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT) {
                continue;
            }
            if (!Unit_PassableTile(map, tx, ty)) continue;
            const MapTile *tile = &map->tiles[ty][tx];
            const Yield *terrain = &g_terrain_yields[tile->terrain];
            const Yield *resource = &g_resource_yields[tile->resource];
            score += terrain->food * 3 + terrain->production * 2
                   + terrain->gold;
            score += resource->food * 3 + resource->production * 2
                   + resource->gold * 2;
            if (tile->resource != RESOURCE_NONE) score += 4;
        }
    }
    int distance = chebyshev(x, y, anchor_x, anchor_y);
    if (distance < 3) score -= 30;
    score -= (distance > 6 ? distance - 6 : 0) * 2;
    return score;
}

static int ai_find_settle_site(const World *world, const Map *map, int owner,
                               int from_x, int from_y, int *out_x,
                               int *out_y) {
    int best = -1;
    int best_x = -1;
    int best_y = -1;

    for (int c = 0; c < world->cities.count; c++) {
        const City *anchor = &world->cities.cities[c];
        if (anchor->owner != owner) continue;
        for (int dy = -AI_SITE_RADIUS; dy <= AI_SITE_RADIUS; dy++) {
            for (int dx = -AI_SITE_RADIUS; dx <= AI_SITE_RADIUS; dx++) {
                int x = anchor->x + dx;
                int y = anchor->y + dy;
                if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
                    continue;
                }
                if (!Unit_PassableTile(map, x, y)) continue;
                if (City_CanFound(&world->cities, map, x, y) != CITY_FOUND_OK) {
                    continue;
                }
                if (Unit_IndexOfAt(&world->units, x, y) >= 0) continue;

                int score = site_score(map, x, y, anchor->x, anchor->y);
                score -= chebyshev(from_x, from_y, x, y);
                if (score > best) {
                    best = score;
                    best_x = x;
                    best_y = y;
                }
            }
        }
    }

    if (best_x < 0) return 0;
    *out_x = best_x;
    *out_y = best_y;
    return 1;
}

static void ai_found_city(World *world, Map *map, int unit_index, int *city_out) {
    Unit *unit = &world->units.units[unit_index];
    int owner = unit->owner;
    int city_index = -1;
    if (City_TryFound(&world->cities, map, owner, unit->x, unit->y,
                      &city_index) != CITY_FOUND_OK) {
        return;
    }

    City *city = &world->cities.cities[city_index];
    City_QueueAdd(city, PROD_WARRIOR);
    if (world->civs[owner].capital_index == WORLD_CIV_NONE) {
        city->is_capital = 1;
        world->civs[owner].capital_index = (uint8_t)city_index;
    }
    Unit_RemoveAt(&world->units, unit_index);
    if (city_out) *city_out = city_index;
}

static void ai_act_settler(World *world, Map *map, int unit_index) {
    Unit *unit = &world->units.units[unit_index];
    int owner = unit->owner;

    if (World_CountCities(world, owner) < AI_MAX_CITIES
        && City_CanFound(&world->cities, map, unit->x, unit->y)
               == CITY_FOUND_OK) {
        ai_found_city(world, map, unit_index, NULL);
        return;
    }

    int target_x = 0;
    int target_y = 0;
    if (!ai_find_settle_site(world, map, owner, unit->x, unit->y,
                             &target_x, &target_y)) {
        return;
    }

    for (int step = 0; step < AI_MAX_STEPS; step++) {
        if (unit->moves_left == 0) break;
        int next_x = 0;
        int next_y = 0;
        int path = Unit_PathNextStep(&world->units, map, unit_index,
                                     target_x, target_y, &next_x, &next_y);
        if (path <= 0) break;
        if (Unit_IndexOfAt(&world->units, next_x, next_y) >= 0) break;
        if (Unit_MoveTo(&world->units, map, unit_index, next_x, next_y) < 0) {
            break;
        }
    }

    unit = &world->units.units[unit_index];
    if (City_CanFound(&world->cities, map, unit->x, unit->y)
        == CITY_FOUND_OK) {
        ai_found_city(world, map, unit_index, NULL);
    }
}

/* ------------------------------------------------------------------------- */
/* Military behavior                                                         */
/* ------------------------------------------------------------------------- */

static int find_enemy_unit(const World *world, int owner, int x, int y,
                           int *out_index) {
    int best = AI_TARGET_RANGE + 1;
    int best_index = -1;
    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        if (unit->owner == owner) continue;
        if (!World_AreAtWar(world, owner, unit->owner)) continue;
        int distance = chebyshev(x, y, unit->x, unit->y);
        if (distance < best) {
            best = distance;
            best_index = i;
        }
    }
    if (best_index >= 0) *out_index = best_index;
    return best_index;
}

static int find_enemy_city(const World *world, int owner, int x, int y,
                           int *out_index) {
    int best = AI_TARGET_RANGE + 1;
    int best_index = -1;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner == owner) continue;
        if (!World_AreAtWar(world, owner, city->owner)) continue;
        int distance = chebyshev(x, y, city->x, city->y);
        if (distance < best) {
            best = distance;
            best_index = i;
        }
    }
    if (best_index >= 0) *out_index = best_index;
    return best_index;
}

static int find_unguarded_city(const World *world, int owner, int x, int y,
                               int *out_index) {
    int best = AI_TARGET_RANGE + 1;
    int best_index = -1;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner != owner) continue;
        if (city_has_unit(world, city)) continue;
        int distance = chebyshev(x, y, city->x, city->y);
        if (distance < best) {
            best = distance;
            best_index = i;
        }
    }
    if (best_index >= 0) *out_index = best_index;
    return best_index;
}

static void move_toward(World *world, Map *map, int unit_index, int target_x,
                        int target_y) {
    for (int step = 0; step < AI_MAX_STEPS; step++) {
        Unit *unit = &world->units.units[unit_index];
        if (unit->moves_left == 0) break;
        int next_x = 0;
        int next_y = 0;
        int path = Unit_PathNextStep(&world->units, map, unit_index,
                                     target_x, target_y, &next_x, &next_y);
        if (path <= 0) break;
        if (Unit_IndexOfAt(&world->units, next_x, next_y) >= 0) break;
        if (Unit_MoveTo(&world->units, map, unit_index, next_x, next_y) < 0) {
            break;
        }
    }
}

static int attack_adjacent_unit(World *world, Map *map, int unit_index,
                                int target_index) {
    if (target_index < 0 || target_index >= world->units.count) return 0;
    if (Combat_CanAttack(world, map, unit_index, target_index) != COMBAT_OK) {
        return 0;
    }
    int attacker_owner = world->units.units[unit_index].owner;
    int attacker_type = world->units.units[unit_index].type;
    int ax = world->units.units[unit_index].x;
    int ay = world->units.units[unit_index].y;
    int target_owner = world->units.units[target_index].owner;
    int target_type = world->units.units[target_index].type;
    int tx = world->units.units[target_index].x;
    int ty = world->units.units[target_index].y;
    int report = report_battle_relevant(map, tx, ty, attacker_owner,
                                        target_owner);
    CombatResult result;
    Combat_AttackUnit(world, map, unit_index, target_index, &result);
    if (report) {
        TurnReport_AddUnitBattle(attacker_owner, attacker_type, target_owner,
                                 target_type, tx, ty, result.attacker_damage,
                                 result.defender_damage, result.attacker_died,
                                 result.defender_died);
    }
    Log_Printf("ai combat: owner=%d type=%d at %d,%d -> owner=%d type=%d "
               "at %d,%d dmg=%d counter=%d attacker_hp=%d defender_hp=%d "
               "defender_dead=%d attacker_dead=%d city_captured=%d",
               attacker_owner, attacker_type, ax, ay,
               target_owner, target_type, tx, ty,
               result.attacker_damage, result.defender_damage,
               result.attacker_hp, result.defender_hp,
               result.defender_died, result.attacker_died,
               result.city_captured);
    return 1;
}

static int ai_attack_city(World *world, Map *map, int unit_index,
                          int city_index) {
    if (city_index < 0 || city_index >= world->cities.count) return 0;
    int owner = world->units.units[unit_index].owner;
    int attacker_type = world->units.units[unit_index].type;
    int city_x = world->cities.cities[city_index].x;
    int city_y = world->cities.cities[city_index].y;
    int city_owner = world->cities.cities[city_index].owner;
    int city_name_index = world->cities.cities[city_index].name_index;
    int report = report_battle_relevant(map, city_x, city_y, owner,
                                        city_owner);
    CombatResult result;
    int code = Combat_AttackCity(world, map, unit_index, city_index,
                                 &result);
    if (code == COMBAT_OK) {
        if (report) {
            TurnReport_AddCityBattle(owner, attacker_type, city_owner,
                                     city_name_index, city_x, city_y,
                                     result.city_defense_damage,
                                     result.city_hp_damage,
                                     result.city_captured);
        }
        Log_Printf("ai assault: owner=%d city=%s at %d,%d def=%d hp=%d "
                   "captured=%d",
                   owner, Str(Data_CityName(city_name_index)),
                   city_x, city_y, result.city_defense_damage,
                   result.city_hp_damage, result.city_captured);
        return 1;
    }
    return 0;
}

/* Move next to a city instead of onto its center.  Until V0.2 an AI could
 * walk into an empty city and capture it instantly; the city now has to be
 * assaulted from an adjacent tile. */
static void move_toward_city(World *world, Map *map, int unit_index,
                             const City *city) {
    if (!world || !map || !city) return;
    const Unit *unit = &world->units.units[unit_index];
    if (adjacent(unit->x, unit->y, city->x, city->y)) return;

    int best_x = -1;
    int best_y = -1;
    int best_distance = 0x7FFFF;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int tx = city->x + dx;
            int ty = city->y + dy;
            if (!Unit_PassableTile(map, tx, ty)) continue;
            if (Unit_IndexOfAt(&world->units, tx, ty) >= 0) continue;
            int distance = chebyshev(unit->x, unit->y, tx, ty);
            if (distance < best_distance) {
                best_distance = distance;
                best_x = tx;
                best_y = ty;
            }
        }
    }
    if (best_x >= 0) move_toward(world, map, unit_index, best_x, best_y);
}

static void ai_act_soldier(World *world, Map *map, int unit_index) {
    Unit *unit = &world->units.units[unit_index];
    int owner = unit->owner;

    int unit_target = -1;
    int city_target = -1;
    find_enemy_unit(world, owner, unit->x, unit->y, &unit_target);
    find_enemy_city(world, owner, unit->x, unit->y, &city_target);

    int unit_distance = unit_target >= 0
        ? chebyshev(unit->x, unit->y, world->units.units[unit_target].x,
                    world->units.units[unit_target].y)
        : 999;
    int city_distance = city_target >= 0
        ? chebyshev(unit->x, unit->y, world->cities.cities[city_target].x,
                    world->cities.cities[city_target].y)
        : 999;

    if (city_target >= 0 && city_distance <= unit_distance) {
        City *city = &world->cities.cities[city_target];
        if (adjacent(unit->x, unit->y, city->x, city->y)) {
            int garrison = Unit_IndexOfAt(&world->units, city->x, city->y);
            if (garrison >= 0) {
                if (Combat_CanAttack(world, map, unit_index, garrison)
                    == COMBAT_OK) {
                    attack_adjacent_unit(world, map, unit_index, garrison);
                    return;
                }
            } else if (Combat_CanAttackCity(world, map, unit_index,
                                            city_target) == COMBAT_OK) {
                ai_attack_city(world, map, unit_index, city_target);
                return;
            }
        }
        move_toward_city(world, map, unit_index, city);
        unit = &world->units.units[unit_index];
        if (adjacent(unit->x, unit->y, city->x, city->y)) {
            int garrison = Unit_IndexOfAt(&world->units, city->x, city->y);
            if (garrison >= 0) {
                attack_adjacent_unit(world, map, unit_index, garrison);
            } else if (Combat_CanAttackCity(world, map, unit_index,
                                            city_target) == COMBAT_OK) {
                ai_attack_city(world, map, unit_index, city_target);
            }
        }
        return;
    }

    if (unit_target >= 0) {
        Unit *target = &world->units.units[unit_target];
        if (adjacent(unit->x, unit->y, target->x, target->y)) {
            attack_adjacent_unit(world, map, unit_index, unit_target);
            return;
        }
        int tx = target->x;
        int ty = target->y;
        move_toward(world, map, unit_index, tx, ty);
        unit = &world->units.units[unit_index];
        if (unit->moves_left > 0 && adjacent(unit->x, unit->y, tx, ty)) {
            int current = Unit_IndexOfAt(&world->units, tx, ty);
            attack_adjacent_unit(world, map, unit_index, current);
        }
        return;
    }

    /* No visible enemy: garrison an empty friendly city or hold position. */
    int city_index = -1;
    if (find_unguarded_city(world, owner, unit->x, unit->y,
                            &city_index) >= 0) {
        City *city = &world->cities.cities[city_index];
        move_toward(world, map, unit_index, city->x, city->y);
    }
}

static void ai_process_units(World *world, Map *map, int owner) {
    for (;;) {
        int index = -1;
        for (int i = 0; i < world->units.count; i++) {
            const Unit *unit = &world->units.units[i];
            if (unit->owner == owner && !unit->acted
                && unit->moves_left > 0) {
                index = i;
                break;
            }
        }
        if (index < 0) break;

        world->units.units[index].acted = 1;
        const Unit *unit = &world->units.units[index];
        if (unit->type == UNIT_SETTLER) {
            ai_act_settler(world, map, index);
        } else if (g_unit_defs[unit->type].strength > 0) {
            ai_act_soldier(world, map, index);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Barbarians                                                                */
/* ------------------------------------------------------------------------- */

static int spawn_barbarian(World *world, Map *map) {
    int center_x = map->start_x;
    int center_y = map->start_y;
    for (int i = 0; i < world->cities.count; i++) {
        if (world->cities.cities[i].owner == WORLD_CIV_PLAYER) {
            center_x = world->cities.cities[i].x;
            center_y = world->cities.cities[i].y;
            break;
        }
    }

    for (int hidden_pass = 1; hidden_pass >= 0; hidden_pass--) {
        for (int radius = 6; radius <= 12; radius++) {
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int adx = dx < 0 ? -dx : dx;
                    int ady = dy < 0 ? -dy : dy;
                    if ((adx > ady ? adx : ady) != radius) continue;
                    int x = center_x + dx;
                    int y = center_y + dy;
                    if (!Unit_PassableTile(map, x, y)) continue;
                    if (Unit_IndexOfAt(&world->units, x, y) >= 0) continue;
                    if (City_IndexOfAt(&world->cities, x, y) >= 0) continue;
                    if (hidden_pass && map->tiles[y][x].visible) continue;
                    int index = Unit_Spawn(&world->units, UNIT_WARRIOR,
                                           WORLD_CIV_BARBARIAN, x, y);
                    if (index < 0) return 0;
                    Unit *unit = &world->units.units[index];
                    unit->hp = 80;
                    unit->moves_left = 0;
                    unit->acted = 1;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void ai_spawn_barbarians(World *world, Map *map) {
    int easy = world->difficulty == WORLD_DIFFICULTY_EASY;
    int hard = world->difficulty == WORLD_DIFFICULTY_HARD;
    int interval = hard ? 5 : (easy ? 12 : BARBARIAN_SPAWN_TURN_INTERVAL);
    int max_units = hard ? 6 : (easy ? 2 : BARBARIAN_MAX_UNITS);
    if (world->turn < interval) return;
    if (world->turn % interval != 0) return;
    if (World_CountUnits(world, WORLD_CIV_BARBARIAN) >= max_units) {
        return;
    }
    if (spawn_barbarian(world, map) && world->notice[0] == '\0') {
        World_SetNotice(world, Str(S_NOTICE_BARBARIANS));
    }
}

/* ------------------------------------------------------------------------- */
/* Turn driver                                                               */
/* ------------------------------------------------------------------------- */

void Ai_ProcessTurn(World *world, Map *map) {
    if (!world || !map) return;

    ai_update_diplomacy(world);

    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        if (!world->civs[civ].active) continue;
        ai_manage_cities(world);
        ai_process_units(world, map, civ);
    }

    ai_spawn_barbarians(world, map);
    ai_process_units(world, map, WORLD_CIV_BARBARIAN);
}
