#include "world.h"

#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "diplomacy.h"
#include "gen/strings_gen.h"
#include "log.h"
#include "tech.h"
#include "turn_report.h"

#define START_OWNER WORLD_CIV_PLAYER

static int spawn_near(World *world, Map *map, int type, int owner,
                      int px, int py) {
    for (int radius = 0; radius <= 12; radius++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int adx = dx < 0 ? -dx : dx;
                int ady = dy < 0 ? -dy : dy;
                if ((adx > ady ? adx : ady) != radius) continue;

                int x = px + dx;
                int y = py + dy;
                if (!Unit_PassableTile(map, x, y)) continue;
                if (Unit_IndexOfAt(&world->units, x, y) >= 0) continue;
                return Unit_Spawn(&world->units, type, owner, x, y);
            }
        }
    }
    return -1;
}

static int chebyshev(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax;
    int dy = ay > by ? ay - by : by - ay;
    return dx > dy ? dx : dy;
}

static int land_neighbors(const Map *map, int x, int y, int radius) {
    int count = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (Unit_PassableTile(map, x + dx, y + dy)) count++;
        }
    }
    return count;
}

/* Greedy far-away placement for the three rival capitals.  The map generator
 * guarantees one connected landmass, so distance plus local land quality is
 * enough; City_CanFound keeps capitals out of each other's minimum radius. */
static int find_rival_start(const World *world, const Map *map,
                            const int *chosen_x, const int *chosen_y,
                            int chosen_count, int *out_x, int *out_y) {
    int best_score = -1;
    int best_x = -1;
    int best_y = -1;

    for (int y = 2; y < MAP_HEIGHT - 2; y++) {
        for (int x = 2; x < MAP_WIDTH - 2; x++) {
            if (!Unit_PassableTile(map, x, y)) continue;
            if (Map_TerrainAt(map, x, y) == TERRAIN_MOUNTAIN) continue;
            if (City_CanFound(&world->cities, map, x, y) != CITY_FOUND_OK) {
                continue;
            }

            int min_dist = chebyshev(x, y, map->start_x, map->start_y);
            for (int i = 0; i < chosen_count; i++) {
                int d = chebyshev(x, y, chosen_x[i], chosen_y[i]);
                if (d < min_dist) min_dist = d;
            }
            if (min_dist < 10) continue;

            int land = land_neighbors(map, x, y, 2);
            if (land < 13) continue;

            int resource_bonus = 0;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int tx = x + dx;
                    int ty = y + dy;
                    if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT) {
                        continue;
                    }
                    if (map->tiles[ty][tx].resource != RESOURCE_NONE) {
                        resource_bonus += 4;
                    }
                }
            }

            int edge = x;
            if (y < edge) edge = y;
            if (MAP_WIDTH - 1 - x < edge) edge = MAP_WIDTH - 1 - x;
            if (MAP_HEIGHT - 1 - y < edge) edge = MAP_HEIGHT - 1 - y;
            int score = min_dist * 40 + land * 4 + resource_bonus
                      - (edge < 4 ? (4 - edge) * 30 : 0);
            if (score > best_score) {
                best_score = score;
                best_x = x;
                best_y = y;
            }
        }
    }

    if (best_x < 0) {
        /* Fallback for an unusually cramped seed: accept the farthest valid
         * land tile even if it is closer than the preferred spacing. */
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (!Unit_PassableTile(map, x, y)) continue;
                if (City_CanFound(&world->cities, map, x, y) != CITY_FOUND_OK) {
                    continue;
                }
                int min_dist = chebyshev(x, y, map->start_x, map->start_y);
                for (int i = 0; i < chosen_count; i++) {
                    int d = chebyshev(x, y, chosen_x[i], chosen_y[i]);
                    if (d < min_dist) min_dist = d;
                }
                int score = min_dist * 40 + land_neighbors(map, x, y, 2);
                if (score > best_score) {
                    best_score = score;
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

static int spawn_raider_near(World *world, Map *map, int cx, int cy,
                             int min_radius, int max_radius) {
    for (int radius = min_radius; radius <= max_radius; radius++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int adx = dx < 0 ? -dx : dx;
                int ady = dy < 0 ? -dy : dy;
                if ((adx > ady ? adx : ady) != radius) continue;
                int x = cx + dx;
                int y = cy + dy;
                if (!Unit_PassableTile(map, x, y)) continue;
                if (Unit_IndexOfAt(&world->units, x, y) >= 0) continue;
                if (City_IndexOfAt(&world->cities, x, y) >= 0) continue;
                int index = Unit_Spawn(&world->units, UNIT_WARRIOR,
                                       WORLD_CIV_BARBARIAN, x, y);
                if (index >= 0) {
                    world->units.units[index].hp = 80;
                    return index;
                }
            }
        }
    }
    return -1;
}

static void spawn_rivals(World *world, Map *map) {
    int chosen_x[WORLD_AI_COUNT];
    int chosen_y[WORLD_AI_COUNT];

    for (int owner = 1; owner <= WORLD_AI_COUNT; owner++) {
        CivState *civ = &world->civs[owner];
        civ->active = 1;
        civ->is_ai = 1;
        civ->capital_index = WORLD_CIV_NONE;

        int sx = 0;
        int sy = 0;
        if (!find_rival_start(world, map, chosen_x, chosen_y, owner - 1,
                              &sx, &sy)) {
            civ->active = 0;
            continue;
        }

        int city_index = -1;
        if (City_TryFound(&world->cities, map, owner, sx, sy,
                          &city_index) != CITY_FOUND_OK) {
            civ->active = 0;
            continue;
        }

        City *city = &world->cities.cities[city_index];
        city->is_capital = 1;
        civ->capital_index = (uint8_t)city_index;
        City_QueueAdd(city, PROD_WARRIOR);

        /* A garrison on the capital plus a second warrior and a settler give
         * each rival a credible start without special-case AI free units. */
        int guard = Unit_Spawn(&world->units, UNIT_WARRIOR, owner, sx, sy);
        if (guard >= 0) world->units.units[guard].moves_left = 0;
        spawn_near(world, map, UNIT_WARRIOR, owner, sx, sy);
        spawn_near(world, map, UNIT_SETTLER, owner, sx, sy);
        if (world->difficulty == WORLD_DIFFICULTY_HARD) {
            spawn_near(world, map, UNIT_WARRIOR, owner, sx, sy);
        }

        chosen_x[owner - 1] = sx;
        chosen_y[owner - 1] = sy;
    }

    /* Barbarians are active from turn one and always hostile.  Put a raider
     * near enough to the player to make combat discoverable, but outside the
     * initial city site. */
    world->civs[WORLD_CIV_BARBARIAN].active = 1;
    world->civs[WORLD_CIV_BARBARIAN].is_ai = 1;
    world->civs[WORLD_CIV_BARBARIAN].capital_index = WORLD_CIV_NONE;
    spawn_raider_near(world, map, map->start_x, map->start_y, 6, 10);
}

void World_Init(World *world) {
    memset(world, 0, sizeof(*world));
    world->turn = 1;
    world->treasury = WORLD_START_TREASURY;
    for (int i = 0; i < WORLD_CIV_COUNT; i++) {
        world->civs[i].capital_index = WORLD_CIV_NONE;
    }
    world->civs[START_OWNER].active = 1;
    world->game_over = WORLD_GAME_NONE;
    world->proposal_civ = WORLD_CIV_NONE;
    world->victory_type = WORLD_VICTORY_NONE;
    world->difficulty = WORLD_DIFFICULTY_NORMAL;
}

void World_BeginWithDifficulty(World *world, Map *map, int difficulty) {
    World_Init(world);
    if (difficulty < WORLD_DIFFICULTY_EASY
        || difficulty > WORLD_DIFFICULTY_HARD) {
        difficulty = WORLD_DIFFICULTY_NORMAL;
    }
    world->difficulty = (uint8_t)difficulty;
    UnitPool_Init(&world->units);
    CityPool_Init(&world->cities);
    Map_ResetFog(map);

    int sx = map->start_x;
    int sy = map->start_y;

    /* One settler, one warrior, and one scout are the baseline start.  Easy
     * adds a second warrior; hard gives the rivals an extra warrior and a
     * larger treasury instead. */
    spawn_near(world, map, UNIT_SETTLER, START_OWNER, sx, sy);
    spawn_near(world, map, UNIT_WARRIOR, START_OWNER, sx + 1, sy + 1);
    spawn_near(world, map, UNIT_SCOUT, START_OWNER, sx - 1, sy + 1);
    if (world->difficulty == WORLD_DIFFICULTY_EASY) {
        spawn_near(world, map, UNIT_WARRIOR, START_OWNER, sx, sy);
    }

    spawn_rivals(world, map);
    if (world->difficulty == WORLD_DIFFICULTY_HARD) {
        spawn_raider_near(world, map, sx, sy, 8, 12);
    }
    /* Rivals begin with a mild rivalry toward one another.  The player starts
     * neutral with everyone and chooses a path through gifts, treaties, or
     * war. */
    for (int a = 1; a <= WORLD_AI_COUNT; a++) {
        int treasury = WORLD_AI_START_TREASURY;
        if (world->difficulty == WORLD_DIFFICULTY_EASY) treasury -= 10;
        if (world->difficulty == WORLD_DIFFICULTY_HARD) treasury += 20;
        world->civs[a].treasury = treasury;
        for (int b = a + 1; b <= WORLD_AI_COUNT; b++) {
            world->relation[a][b] = -10;
            world->relation[b][a] = -10;
        }
    }
    World_UpdateVision(world, map);

    snprintf(world->notice, sizeof(world->notice), "%s",
             Str(S_NOTICE_BARBARIANS));
    world->notice_turn = world->turn;
}

void World_Begin(World *world, Map *map) {
    World_BeginWithDifficulty(world, map, WORLD_DIFFICULTY_NORMAL);
}

void World_UpdateVision(World *world, Map *map) {
    Map_ClearVisibility(map);
    Unit_GrantVisionOwner(&world->units, map, START_OWNER);
    City_GrantVisionOwner(&world->cities, map, START_OWNER);
    /* Open borders and alliances share maps: the partner's units and cities
     * grant the player vision while the agreement lasts. */
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        if (!world->civs[civ].active) continue;
        if (World_HasTreaty(world, START_OWNER, civ,
                            WORLD_TREATY_OPEN_BORDERS
                            | WORLD_TREATY_ALLIANCE)) {
            Unit_GrantVisionOwner(&world->units, map, civ);
            City_GrantVisionOwner(&world->cities, map, civ);
        }
    }
}

StrId World_CivNameId(int owner) {
    switch (owner) {
        case 0: return S_CIV_AURELIA;
        case 1: return S_CIV_BOREA;
        case 2: return S_CIV_CYRENE;
        case 3: return S_CIV_DRAVEN;
        case 4: return S_CIV_BARBARIANS;
        default: return S_CIV_BARBARIANS;
    }
}

unsigned int World_CivColor(int owner) {
    switch (owner) {
        case 0: return 0xFF4A90D9u;
        case 1: return 0xFFD06050u;
        case 2: return 0xFFD8B25Cu;
        case 3: return 0xFFA070D0u;
        case 4: return 0xFF9FB0A8u;
        default: return 0xFF9FB0A8u;
    }
}

int World_CountCities(const World *world, int owner) {
    if (!world) return 0;
    int count = 0;
    for (int i = 0; i < world->cities.count; i++) {
        if (world->cities.cities[i].owner == owner) count++;
    }
    return count;
}

int World_CountUnits(const World *world, int owner) {
    if (!world) return 0;
    int count = 0;
    for (int i = 0; i < world->units.count; i++) {
        if (world->units.units[i].owner == owner) count++;
    }
    return count;
}

int World_MilitaryCount(const World *world, int owner) {
    if (!world) return 0;
    int count = 0;
    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        if (unit->owner != owner) continue;
        if (g_unit_defs[unit->type].strength > 0) count++;
    }
    return count;
}

int World_PlayerCityCount(const World *world) {
    return World_CountCities(world, START_OWNER);
}

int World_PlayerPopulation(const World *world) {
    if (!world) return 0;
    int population = 0;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner == START_OWNER) population += city->population;
    }
    return population;
}

int World_PlayerTechCount(const World *world) {
    return world ? Tech_CountKnown(world->researched) : 0;
}

/* AI research is abstracted as one new tech every eight turns.  It is derived
 * from the turn counter instead of stored in CivState, so M7 version-2 saves
 * keep the same layout and remain loadable. */
uint32_t World_CivResearched(const World *world, int owner) {
    if (!world || owner < 0 || owner >= WORLD_CIV_COUNT) return 0;
    if (owner == START_OWNER) return world->researched;
    if (owner == WORLD_CIV_BARBARIAN) return 0;

    int quota = 1 + (world->turn - 1) / 8;
    if (quota > TECH_COUNT - 1) quota = TECH_COUNT - 1;
    uint32_t mask = 0;
    int known = 0;
    while (known < quota) {
        int added = 0;
        for (int tech = 1; tech < TECH_COUNT && known < quota; tech++) {
            if (mask & (1u << tech)) continue;
            if (!Tech_HasPrereqs(mask, tech)) continue;
            mask |= 1u << tech;
            known++;
            added = 1;
        }
        if (!added) break;
    }
    return mask;
}

int World_CivTechCount(const World *world, int owner) {
    return Tech_CountKnown(World_CivResearched(world, owner));
}

int World_AllTechsResearched(const World *world) {
    if (!world) return 0;
    uint32_t all = 0;
    for (int tech = 1; tech < TECH_COUNT; tech++) {
        all |= 1u << tech;
    }
    return (world->researched & all) == all;
}

/* The three late-game science projects are simplified to three late techs. */
static const int science_goals[WORLD_SCIENCE_GOAL_COUNT] = {
    TECH_ASTRONOMY, TECH_SCIENTIFIC_METHOD, TECH_NAVIGATION
};

int World_ScienceGoalCount(const World *world) {
    if (!world) return 0;
    int count = 0;
    for (int i = 0; i < WORLD_SCIENCE_GOAL_COUNT; i++) {
        if (Tech_IsKnown(world->researched, science_goals[i])) count++;
    }
    return count;
}

int World_ScienceGoalsComplete(const World *world) {
    return World_ScienceGoalCount(world) == WORLD_SCIENCE_GOAL_COUNT;
}

static int civ_treasury(const World *world, int owner) {
    if (owner == START_OWNER) return (int)world->treasury;
    if (owner < 0 || owner >= WORLD_CIV_COUNT) return 0;
    return (int)world->civs[owner].treasury;
}

int World_CivScore(const World *world, int owner) {
    if (!world || owner < 0 || owner >= WORLD_CIV_COUNT) return 0;

    int score = 0;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner != owner) continue;
        score += 10;
        score += (int)city->population * 6;
        score += City_OwnedCount(city);
        score += City_BuildingCount(city) * 5;
    }
    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        if (unit->owner != owner) continue;
        score += 4;
        score += unit->hp / 25;
    }
    if (owner != WORLD_CIV_BARBARIAN) {
        score += World_CivTechCount(world, owner) * 10;
    }
    int treasury = civ_treasury(world, owner);
    if (treasury > 0) score += treasury / 20;
    return score;
}

int World_PlayerScore(const World *world) {
    return World_CivScore(world, START_OWNER);
}

int World_BestRivalScore(const World *world) {
    int best = 0;
    if (!world) return 0;
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        if (!world->civs[civ].active) continue;
        int score = World_CivScore(world, civ);
        if (score > best) best = score;
    }
    return best;
}

int World_PlayerHasTopScore(const World *world) {
    return World_PlayerScore(world) >= World_BestRivalScore(world);
}

void World_FinishScoreGame(World *world) {
    if (!world || world->game_over != WORLD_GAME_NONE) return;
    int eliminated = world->player_founded
                  && World_PlayerCityCount(world) == 0;
    int top_score = World_PlayerHasTopScore(world);
    world->game_over = (!eliminated && top_score)
                           ? WORLD_GAME_VICTORY : WORLD_GAME_DEFEAT;
    world->victory_type = WORLD_VICTORY_SCORE;
}

int World_LivingRivals(const World *world) {
    if (!world) return 0;
    int count = 0;
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        if (!world->civs[civ].active) continue;
        if (World_CountCities(world, civ) == 0) continue;
        count++;
    }
    return count;
}

static int valid_major_pair(const World *world, int a, int b) {
    if (!world || a == b) return 0;
    if (a < 0 || a >= WORLD_CIV_COUNT || b < 0 || b >= WORLD_CIV_COUNT) {
        return 0;
    }
    if (a == WORLD_CIV_BARBARIAN || b == WORLD_CIV_BARBARIAN) return 0;
    return 1;
}

int World_Relation(const World *world, int a, int b) {
    if (!valid_major_pair(world, a, b)) return 0;
    return world->relation[a][b];
}

void World_ChangeRelation(World *world, int a, int b, int delta) {
    if (!valid_major_pair(world, a, b)) return;
    int value = (int)world->relation[a][b] + delta;
    if (value < -100) value = -100;
    if (value > 100) value = 100;
    world->relation[a][b] = (int8_t)value;
    world->relation[b][a] = (int8_t)value;
}

int World_HasTreaty(const World *world, int a, int b, uint8_t treaty) {
    if (!valid_major_pair(world, a, b)) return 0;
    return (world->treaty_mask[a][b] & treaty) != 0;
}

int World_TreatyTurns(const World *world, int a, int b) {
    if (!valid_major_pair(world, a, b)) return 0;
    return world->treaty_turns[a][b];
}

int World_OpenBordersTurns(const World *world, int a, int b) {
    if (!valid_major_pair(world, a, b)) return 0;
    return world->open_borders_turns[a][b];
}

void World_SetTreaty(World *world, int a, int b, uint8_t treaty,
                     uint8_t turns) {
    if (!valid_major_pair(world, a, b)) return;
    world->treaty_mask[a][b] |= treaty;
    world->treaty_mask[b][a] |= treaty;
    if (treaty & WORLD_TREATY_NAP) {
        world->treaty_turns[a][b] = turns;
        world->treaty_turns[b][a] = turns;
    }
    if (treaty & WORLD_TREATY_OPEN_BORDERS) {
        world->open_borders_turns[a][b] = turns;
        world->open_borders_turns[b][a] = turns;
    }
    World_ChangeRelation(world, a, b, 5);
}

void World_ClearTreaty(World *world, int a, int b, uint8_t treaty) {
    if (!valid_major_pair(world, a, b)) return;
    world->treaty_mask[a][b] &= (uint8_t)~treaty;
    world->treaty_mask[b][a] &= (uint8_t)~treaty;
    if (treaty & WORLD_TREATY_NAP) {
        world->treaty_turns[a][b] = 0;
        world->treaty_turns[b][a] = 0;
    }
    if (treaty & WORLD_TREATY_OPEN_BORDERS) {
        world->open_borders_turns[a][b] = 0;
        world->open_borders_turns[b][a] = 0;
    }
}

int World_HasTrade(const World *world, int a, int b) {
    if (!valid_major_pair(world, a, b)) return 0;
    return world->trade_turns[a][b] > 0;
}

int World_TradeTurns(const World *world, int a, int b) {
    if (!valid_major_pair(world, a, b)) return 0;
    return world->trade_turns[a][b];
}

void World_SetTrade(World *world, int a, int b, uint8_t turns) {
    if (!valid_major_pair(world, a, b)) return;
    world->trade_turns[a][b] = turns;
    world->trade_turns[b][a] = turns;
    World_ChangeRelation(world, a, b, 5);
}

void World_ClearTrade(World *world, int a, int b) {
    if (!valid_major_pair(world, a, b)) return;
    world->trade_turns[a][b] = 0;
    world->trade_turns[b][a] = 0;
}

int World_CanDeclareWar(const World *world, int a, int b) {
    if (!valid_major_pair(world, a, b)) return 0;
    if (World_AreAtWar(world, a, b)) return 0;
    if (World_HasTreaty(world, a, b, WORLD_TREATY_NAP)) return 0;
    if (World_HasTreaty(world, a, b, WORLD_TREATY_ALLIANCE)) return 0;
    return 1;
}

int World_AreAtWar(const World *world, int a, int b) {
    if (!world || a == b) return 0;
    if (a < 0 || a >= WORLD_CIV_COUNT || b < 0 || b >= WORLD_CIV_COUNT) {
        return 0;
    }
    if (a == WORLD_CIV_BARBARIAN || b == WORLD_CIV_BARBARIAN) return 1;
    return (world->civs[a].at_war_mask & (1u << b)) != 0;
}

void World_SetNotice(World *world, const char *text) {
    if (!world) return;
    snprintf(world->notice, sizeof(world->notice), "%s",
             text ? text : "");
    world->notice_turn = world->turn;
}

void World_DeclareWar(World *world, int a, int b) {
    if (!World_CanDeclareWar(world, a, b)) return;

    world->civs[a].at_war_mask |= (uint8_t)(1u << b);
    world->civs[b].at_war_mask |= (uint8_t)(1u << a);
    world->civs[a].war_since[b] = (uint16_t)world->turn;
    world->civs[b].war_since[a] = (uint16_t)world->turn;

    /* War tears up every agreement between the pair.  The relation penalty is
     * symmetric so the AI and the player share the same memory of the war. */
    world->treaty_mask[a][b] = 0;
    world->treaty_mask[b][a] = 0;
    world->treaty_turns[a][b] = 0;
    world->treaty_turns[b][a] = 0;
    world->open_borders_turns[a][b] = 0;
    world->open_borders_turns[b][a] = 0;
    world->trade_turns[a][b] = 0;
    world->trade_turns[b][a] = 0;
    World_ChangeRelation(world, a, b, -30);
    Log_Printf("diplo: war a=%d b=%d turn=%d relation=%d", a, b,
               world->turn, World_Relation(world, a, b));
    world->alliance_victory_turns = 0;
    if (world->proposal_civ == a || world->proposal_civ == b) {
        world->proposal_civ = WORLD_CIV_NONE;
        world->proposal_type = WORLD_PROPOSAL_NONE;
        world->proposal_cooldown = 3;
    }

    char text[96];
    snprintf(text, sizeof(text), "%s %s %s",
             Str(World_CivNameId(a)), Str(S_NOTICE_WAR),
             Str(World_CivNameId(b)));
    World_SetNotice(world, text);
}

void World_MakePeace(World *world, int a, int b) {
    if (!world || a == b) return;
    if (a < 0 || a >= WORLD_CIV_COUNT || b < 0 || b >= WORLD_CIV_COUNT) {
        return;
    }
    if (a == WORLD_CIV_BARBARIAN || b == WORLD_CIV_BARBARIAN) return;
    if (!World_AreAtWar(world, a, b)) return;

    world->civs[a].at_war_mask &= (uint8_t)~(1u << b);
    world->civs[b].at_war_mask &= (uint8_t)~(1u << a);
    world->civs[a].war_since[b] = 0;
    world->civs[b].war_since[a] = 0;

    World_ChangeRelation(world, a, b, 10);
    Log_Printf("diplo: peace a=%d b=%d turn=%d relation=%d", a, b,
               world->turn, World_Relation(world, a, b));
    world->alliance_victory_turns = 0;

    char text[96];
    snprintf(text, sizeof(text), "%s %s %s",
             Str(World_CivNameId(a)), Str(S_NOTICE_PEACE),
             Str(World_CivNameId(b)));
    World_SetNotice(world, text);
}

static int research_percent(uint32_t researched) {
    int percent = 0;
    for (int tech = 1; tech < TECH_COUNT; tech++) {
        if (!Tech_IsKnown(researched, tech)) continue;
        if (Tech_Effect(tech) == TECH_EFFECT_RESEARCH_PCT) {
            percent += Tech_EffectValue(tech);
        }
    }
    return percent;
}

int World_CityGold(const World *world, const Map *map) {
    if (!world || !map) return 0;
    int total = 0;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner != START_OWNER) continue;
        CityYield yield;
        City_ComputeYieldsWithTech(city, map, world->researched, &yield);
        total += yield.gold;
    }
    return total;
}

int World_CityScience(const World *world, const Map *map) {
    if (!world || !map) return 0;
    int total = 0;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner != START_OWNER) continue;
        CityYield yield;
        City_ComputeYieldsWithTech(city, map, world->researched, &yield);
        total += yield.science;
    }
    int percent = research_percent(world->researched);
    return total + total * percent / 100;
}

int World_BuildingMaintenance(const World *world, const Map *map) {
    (void)map;
    if (!world) return 0;
    int total = 0;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner != START_OWNER) continue;
        total += City_Maintenance(city);
    }
    return total;
}

int World_UnitMaintenance(const World *world) {
    if (!world) return 0;
    /* One gold per three player units, rounded up. */
    return (World_CountUnits(world, START_OWNER) + 2) / 3;
}

int World_NetGold(const World *world, const Map *map) {
    return World_CityGold(world, map)
         - World_BuildingMaintenance(world, map)
         - World_UnitMaintenance(world);
}

void World_ProcessCities(World *world, Map *map) {
    if (!world || !map) return;

    int player_broke = world->treasury < 0;
    int gold = 0;
    int science = 0;
    int maintenance = 0;

    for (int i = 0; i < world->cities.count; i++) {
        City *city = &world->cities.cities[i];
        int is_player = city->owner == START_OWNER;
        /* Keep the wall cap and legacy combat fields current.  HP recovery is
         * applied after all attacks at the end of the turn. */
        City_UpdateCombat(city);
        uint32_t researched = World_CivResearched(world, city->owner);
        CityYield yield;
        City_ComputeYieldsWithTech(city, map, researched, &yield);
        if (is_player) {
            gold += yield.gold;
            science += player_broke ? yield.science / 2 : yield.science;
            maintenance += yield.maintenance;
        } else if (city->owner < WORLD_CIV_COUNT) {
            int net = yield.gold - yield.maintenance;
            if (net < 0) net = 0;
            world->civs[city->owner].treasury += net;
        }
        City_ProcessTurnWithTech(city, map, &world->units,
                                 is_player && player_broke, researched, NULL);
    }

    int percent = research_percent(world->researched);
    science += science * percent / 100;
    maintenance += World_UnitMaintenance(world);
    world->treasury += gold - maintenance;
    world->science += science;
    world->gold_per_turn = gold - maintenance;
    world->science_per_turn = science;

    /* Spend this turn's science on the player's active research.  A finished
     * tech stops research until the player picks the next one. */
    if (world->research_type > TECH_NONE
        && world->research_type < TECH_COUNT) {
        world->research_progress += science;
        int cost = Tech_Cost(world->research_type);
        if (world->research_progress >= cost
            && Tech_HasPrereqs(world->researched, world->research_type)) {
            world->researched |= (1u << world->research_type);
            world->research_type = TECH_NONE;
            world->research_progress = 0;
        }
    }
}

static void recover_cities(World *world) {
    for (int i = 0; i < world->cities.count; i++) {
        City_RecoverHP(&world->cities.cities[i], world->turn);
    }
}

int World_SetResearch(World *world, int tech) {
    if (!world) return 0;
    if (tech < 0 || tech >= TECH_COUNT) return 0;
    if (!Tech_CanSelect(world->researched, tech)) return 0;
    /* Re-selecting the same tech keeps its progress; switching restarts. */
    if (world->research_type != tech) {
        world->research_progress = 0;
    }
    world->research_type = tech;
    return 1;
}

int World_ResearchType(const World *world) {
    return world ? world->research_type : TECH_NONE;
}

int World_ResearchProgress(const World *world) {
    return world ? world->research_progress : 0;
}

int World_ResearchCost(const World *world) {
    if (!world || world->research_type <= TECH_NONE) return 0;
    int remain = Tech_Cost(world->research_type) - world->research_progress;
    return remain < 0 ? 0 : remain;
}

static int city_owns_unit_tile(const World *world, int owner, int x, int y) {
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner == owner && City_OwnsTile(city, x, y)) return 1;
    }
    return 0;
}

static void heal_units(World *world, Map *map) {
    (void)map;
    for (int i = 0; i < world->units.count; i++) {
        Unit *unit = &world->units.units[i];
        if (unit->hp == 0 || unit->hp > UNIT_HP_MAX) unit->hp = UNIT_HP_MAX;
        if (unit->hp >= UNIT_HP_MAX) continue;
        /* Units that moved or attacked do not recover until a full turn of
         * rest.  Friendly territory heals twice as fast as neutral ground. */
        int full = Unit_TypeMovePoints(unit->type);
        if (unit->moves_left != full) continue;
        int heal = city_owns_unit_tile(world, unit->owner, unit->x, unit->y)
                       ? 20 : 10;
        int hp = (int)unit->hp + heal;
        unit->hp = (uint8_t)(hp > UNIT_HP_MAX ? UNIT_HP_MAX : hp);
    }
}

void World_EndTurn(World *world, Map *map) {
    if (!world || !map) return;

    TurnReport_Clear();
    World_ProcessCities(world, map);
    world->notice[0] = '\0';
    Ai_ProcessTurn(world, map);
    recover_cities(world);
    Diplomacy_ProcessTurn(world);
    heal_units(world, map);
    Unit_ResetTurn(&world->units);
    world->turn++;
    World_UpdateVision(world, map);
    if (world->notice[0] != '\0') {
        world->notice_turn = world->turn;
    }
    World_CheckGameOver(world);
}

int World_FoundCity(World *world, Map *map, int unit_index) {
    if (unit_index < 0 || unit_index >= world->units.count) {
        return WORLD_FOUND_NOT_UNIT;
    }
    const Unit *unit = &world->units.units[unit_index];
    if (unit->owner != START_OWNER) return WORLD_FOUND_NOT_SETTLER;
    if (unit->type != UNIT_SETTLER) return WORLD_FOUND_NOT_SETTLER;

    int city_index = -1;
    CityFoundResult result = City_TryFound(&world->cities, map, START_OWNER,
                                           unit->x, unit->y, &city_index);
    switch (result) {
        case CITY_FOUND_OK:
            break;
        case CITY_FOUND_INVALID_TILE:
            return WORLD_FOUND_INVALID_TILE;
        case CITY_FOUND_TOO_CLOSE:
            return WORLD_FOUND_TOO_CLOSE;
        default:
            return WORLD_FOUND_LIMIT;
    }

    world->cities.cities[city_index].is_capital = 1;
    world->player_founded = 1;
    Unit_RemoveAt(&world->units, unit_index);
    World_UpdateVision(world, map);
    World_CheckGameOver(world);
    return city_index;
}

int World_FirstUnitWithMoves(const World *world) {
    for (int i = 0; i < world->units.count; i++) {
        if (world->units.units[i].owner == START_OWNER
            && world->units.units[i].moves_left > 0) {
            return i;
        }
    }
    return -1;
}

int World_NextUnitWithMoves(const World *world, int current) {
    if (!world || world->units.count == 0) return -1;
    int count = world->units.count;
    int start = current;
    if (start < 0 || start >= count) start = -1;
    for (int step = 1; step <= count; step++) {
        int index = (start + step) % count;
        const Unit *unit = &world->units.units[index];
        if (unit->owner == START_OWNER && unit->moves_left > 0) {
            return index;
        }
    }
    return -1;
}

int World_CycleUnit(const World *world, int current, int direction) {
    int count = world->units.count;
    if (count == 0) return -1;
    if (direction == 0) direction = 1;

    for (int pass = 0; pass < 2; pass++) {
        int index;
        if (current < 0 || current >= count) {
            index = direction > 0 ? 0 : count - 1;
        } else {
            index = (current + direction + count) % count;
        }
        for (int step = 0; step < count; step++) {
            const Unit *unit = &world->units.units[index];
            if (unit->owner == START_OWNER
                && (pass == 1 || unit->moves_left > 0)) {
                return index;
            }
            index = (index + direction + count) % count;
        }
    }
    return -1;
}

int World_CaptureCity(World *world, Map *map, int city_index, int new_owner) {
    if (!world || !map) return 0;
    if (city_index < 0 || city_index >= world->cities.count) return 0;
    if (new_owner < 0 || new_owner >= WORLD_CIV_COUNT) return 0;

    City *city = &world->cities.cities[city_index];
    int old_owner = city->owner;
    if (old_owner == new_owner) return 0;

    city->owner = (uint8_t)new_owner;
    if (city->population > 1) city->population--;
    city->queue_count = 0;
    city->production_stored = 0;
    city->food_stored = 0;
    city->influence = 0;
    city->locked_mask = 0;
    City_ResetCombat(city);
    City_AutoAssign(city, map);

    if (new_owner == START_OWNER) world->player_founded = 1;
    world->cities_captured++;

    if (old_owner != WORLD_CIV_BARBARIAN
        && new_owner != WORLD_CIV_BARBARIAN) {
        world->treaty_mask[old_owner][new_owner] = 0;
        world->treaty_mask[new_owner][old_owner] = 0;
        world->treaty_turns[old_owner][new_owner] = 0;
        world->treaty_turns[new_owner][old_owner] = 0;
        world->open_borders_turns[old_owner][new_owner] = 0;
        world->open_borders_turns[new_owner][old_owner] = 0;
        world->trade_turns[old_owner][new_owner] = 0;
        world->trade_turns[new_owner][old_owner] = 0;
        if (world->proposal_civ == old_owner
            || world->proposal_civ == new_owner) {
            world->proposal_civ = WORLD_CIV_NONE;
            world->proposal_type = WORLD_PROPOSAL_NONE;
            world->proposal_cooldown = 3;
        }
        World_ChangeRelation(world, old_owner, new_owner, -25);
        /* Other major civilizations dislike a conqueror even when they were
         * not the victim. */
        for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
            if (!world->civs[civ].active) continue;
            if (civ == old_owner || civ == new_owner) continue;
            World_ChangeRelation(world, new_owner, civ, -5);
        }
        world->alliance_victory_turns = 0;
    }

    char text[96];
    snprintf(text, sizeof(text), "%s %s %s",
             Str(World_CivNameId(new_owner)), Str(S_NOTICE_CAPTURED),
             Str(Data_CityName(city->name_index)));
    World_SetNotice(world, text);

    World_CheckGameOver(world);
    return 1;
}

void World_CheckGameOver(World *world) {
    if (!world || world->game_over != WORLD_GAME_NONE) return;

    if (world->player_founded && World_PlayerCityCount(world) == 0) {
        world->game_over = WORLD_GAME_DEFEAT;
        world->victory_type = WORLD_VICTORY_NONE;
        return;
    }

    if (World_ScienceGoalsComplete(world)) {
        world->game_over = WORLD_GAME_VICTORY;
        world->victory_type = WORLD_VICTORY_SCIENCE;
        return;
    }

    int rival_capitals = 0;
    int player_capitals = 0;
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        if (!world->civs[civ].active) continue;
        int index = world->civs[civ].capital_index;
        if (index == WORLD_CIV_NONE || index >= world->cities.count) continue;
        rival_capitals++;
        if (world->cities.cities[index].owner == START_OWNER) {
            player_capitals++;
        }
    }
    if (rival_capitals > 0 && player_capitals == rival_capitals) {
        world->game_over = WORLD_GAME_VICTORY;
        world->victory_type = WORLD_VICTORY_CONQUEST;
    }
}
