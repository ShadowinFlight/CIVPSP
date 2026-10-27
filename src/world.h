#ifndef CIVPSP_WORLD_H
#define CIVPSP_WORLD_H

#include <stdint.h>

#include "city.h"
#include "map.h"
#include "unit.h"

#define WORLD_START_TREASURY 20
#define WORLD_AI_COUNT       3
#define WORLD_CIV_COUNT      5
#define WORLD_CIV_PLAYER     0
#define WORLD_CIV_BARBARIAN  4
#define WORLD_CIV_NONE       0xFF

/* M6 treaties.  NAP/open-borders have a turn counter; an alliance lasts until
 * it is broken by war (which treaties currently prevent) or the game ends. */
#define WORLD_TREATY_NAP          0x01
#define WORLD_TREATY_OPEN_BORDERS 0x02
#define WORLD_TREATY_ALLIANCE     0x04

#define WORLD_TRADE_GOLD             2
#define WORLD_TRADE_TURNS            10
#define WORLD_AI_START_TREASURY      30

#define WORLD_ALLIANCE_VICTORY_TURNS 10
#define WORLD_STANDARD_TURN_LIMIT    200
#define WORLD_SCIENCE_GOAL_COUNT     3

typedef enum {
    WORLD_DIFFICULTY_EASY = 0,
    WORLD_DIFFICULTY_NORMAL,
    WORLD_DIFFICULTY_HARD
} WorldDifficulty;

typedef enum {
    WORLD_PROPOSAL_NONE = 0,
    WORLD_PROPOSAL_PEACE,
    WORLD_PROPOSAL_NAP,
    WORLD_PROPOSAL_ALLIANCE
} WorldProposalType;

typedef enum {
    WORLD_VICTORY_NONE = 0,
    WORLD_VICTORY_CONQUEST,
    WORLD_VICTORY_DIPLOMATIC,
    WORLD_VICTORY_SCIENCE,
    WORLD_VICTORY_SCORE
} WorldVictoryType;

typedef struct {
    uint8_t active;
    uint8_t is_ai;
    uint8_t eliminated;
    uint8_t at_war_mask;
    uint8_t capital_index;      /* city index, or WORLD_CIV_NONE */
    uint16_t war_since[WORLD_CIV_COUNT];
    int32_t treasury;           /* AI trade payments; player uses World.treasury */
} CivState;

typedef enum {
    WORLD_GAME_NONE = 0,
    WORLD_GAME_VICTORY,
    WORLD_GAME_DEFEAT
} WorldGameOver;

typedef struct {
    UnitPool units;
    CityPool cities;
    CivState civs[WORLD_CIV_COUNT];
    int turn;
    int32_t treasury;
    int32_t science;
    int32_t gold_per_turn;
    int32_t science_per_turn;
    /* M4 research: bitmask of researched techs, the tech currently being
     * researched (or TECH_NONE), and how much science is banked into it. */
    uint32_t researched;
    int32_t research_type;
    int32_t research_progress;
    /* M5: rival civilizations, barbarians, war state, and end conditions. */
    uint8_t player_founded;
    uint8_t game_over;          /* WorldGameOver */
    uint16_t battles;
    uint16_t cities_captured;
    int notice_turn;
    char notice[96];
    /* M6 diplomacy.  Relations and treaties are symmetric and indexed by the
     * two civ owners; barbarians never participate. */
    int8_t relation[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t treaty_mask[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t treaty_turns[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t open_borders_turns[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t trade_turns[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t gift_count[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t proposal_civ;       /* civ offering, or WORLD_CIV_NONE */
    uint8_t proposal_type;      /* WorldProposalType */
    uint8_t proposal_cooldown;
    uint8_t alliance_victory_turns;
    uint8_t victory_type;       /* WorldVictoryType */
    /* V0.2 new-game options and per-turn diplomacy limits. */
    uint8_t difficulty;         /* WorldDifficulty */
    uint8_t denounce_used[WORLD_CIV_COUNT];
} World;

typedef enum {
    WORLD_FOUND_NOT_UNIT = -1,
    WORLD_FOUND_NOT_SETTLER = -2,
    WORLD_FOUND_INVALID_TILE = -3,
    WORLD_FOUND_TOO_CLOSE = -4,
    WORLD_FOUND_LIMIT = -5
} WorldFoundError;

void World_Init(World *world);
void World_Begin(World *world, Map *map);
void World_BeginWithDifficulty(World *world, Map *map, int difficulty);
void World_UpdateVision(World *world, Map *map);
void World_EndTurn(World *world, Map *map);

/* V0.3 scout automation.  Auto-explore is stored in the high bit of
 * Unit.acted, so it survives saves without changing the save layout. */
int World_AutoExploreScout(World *world, Map *map, int unit_index);
void World_AutoExploreScouts(World *world, Map *map);

/* M5 civilizations, war, and empire helpers. */
StrId World_CivNameId(int owner);
unsigned int World_CivColor(int owner);
int World_AreAtWar(const World *world, int a, int b);
void World_DeclareWar(World *world, int a, int b);
void World_MakePeace(World *world, int a, int b);
int World_CountCities(const World *world, int owner);
int World_CountUnits(const World *world, int owner);
int World_MilitaryCount(const World *world, int owner);
int World_PlayerCityCount(const World *world);
int World_PlayerPopulation(const World *world);
int World_PlayerTechCount(const World *world);
/* V0.3 music mood helpers: highest researched era and war with a living major
 * rival (barbarians are excluded because they are permanently hostile). */
int World_PlayerEra(const World *world);
int World_PlayerAtWar(const World *world);
int World_CivTechCount(const World *world, int owner);
uint32_t World_CivResearched(const World *world, int owner);
int World_AllTechsResearched(const World *world);
int World_ScienceGoalCount(const World *world);
int World_ScienceGoalsComplete(const World *world);

/* M7 results/timed mode.  Scores count population, cities, territory,
 * buildings, units, technology, and a little treasury. */
int World_CivScore(const World *world, int owner);
int World_PlayerScore(const World *world);
int World_BestRivalScore(const World *world);
int World_PlayerHasTopScore(const World *world);
/* Ends a standard-mode game at the turn limit using score ranking. */
void World_FinishScoreGame(World *world);

/* M6 diplomacy primitives.  A relation is symmetric (-100..100).  Treaties
 * are symmetric masks; the turn counter is only meaningful for NAP/open
 * borders.  Barbarians are excluded from all of these. */
int World_Relation(const World *world, int a, int b);
void World_ChangeRelation(World *world, int a, int b, int delta);
int World_HasTreaty(const World *world, int a, int b, uint8_t treaty);
int World_TreatyTurns(const World *world, int a, int b);
int World_OpenBordersTurns(const World *world, int a, int b);
void World_SetTreaty(World *world, int a, int b, uint8_t treaty,
                     uint8_t turns);
void World_ClearTreaty(World *world, int a, int b, uint8_t treaty);
int World_HasTrade(const World *world, int a, int b);
int World_TradeTurns(const World *world, int a, int b);
void World_SetTrade(World *world, int a, int b, uint8_t turns);
void World_ClearTrade(World *world, int a, int b);
int World_CanDeclareWar(const World *world, int a, int b);
int World_LivingRivals(const World *world);

/* Capture resolves ownership, resets the city queue, checks victory/defeat,
 * and records a notice.  Returns 1 on success. */
int World_CaptureCity(World *world, Map *map, int city_index, int new_owner);
void World_CheckGameOver(World *world);
void World_SetNotice(World *world, const char *text);

/* M4 research. Returns 1 on success (0 if the tech is locked or unknown). */
int World_SetResearch(World *world, int tech);
int World_ResearchType(const World *world);   /* TechType or TECH_NONE */
int World_ResearchProgress(const World *world);
int World_ResearchCost(const World *world);   /* remaining points to finish */

/* End-of-turn city processing: growth, production, borders, gold,
 * maintenance, and science. */
void World_ProcessCities(World *world, Map *map);
int World_CityGold(const World *world, const Map *map);
int World_CityScience(const World *world, const Map *map);
int World_BuildingMaintenance(const World *world, const Map *map);
int World_UnitMaintenance(const World *world);
int World_NetGold(const World *world, const Map *map);

/* Returns the new city index (>= 0) or a negative WorldFoundError.  A
 * successful founding consumes the settler. */
int World_FoundCity(World *world, Map *map, int unit_index);

int World_FirstUnitWithMoves(const World *world);
/* R-shoulder cycling: only player units that still have movement points. */
int World_NextUnitWithMoves(const World *world, int current);
int World_CycleUnit(const World *world, int current, int direction);

#endif
