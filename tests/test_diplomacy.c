#include <stdio.h>
#include <string.h>

#include "city.h"
#include "combat.h"
#include "diplomacy.h"
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

static void setup(void) {
    Map_Generate(&map, 0x5A5A1234u);
    World_Begin(&world, &map);
    CHECK(world.cities.count == WORLD_AI_COUNT);
    CHECK(World_LivingRivals(&world) == WORLD_AI_COUNT);

    int settler = -1;
    for (int i = 0; i < world.units.count; i++) {
        if (world.units.units[i].owner == WORLD_CIV_PLAYER
            && world.units.units[i].type == UNIT_SETTLER) {
            settler = i;
            break;
        }
    }
    CHECK(settler >= 0);
    if (settler >= 0) {
        CHECK(World_FoundCity(&world, &map, settler) >= 0);
    }
}

static void test_initial_and_bands(void) {
    setup();
    CHECK(World_Relation(&world, 0, 1) == 0);
    CHECK(World_Relation(&world, 1, 2) == -10);
    CHECK(!World_HasTreaty(&world, 0, 1, WORLD_TREATY_NAP));
    CHECK(World_CanDeclareWar(&world, 0, 1));
    CHECK(Diplomacy_RelationBand(0) == DIPLO_BAND_NEUTRAL);
    CHECK(Diplomacy_RelationBand(-100) == DIPLO_BAND_HOSTILE);
    CHECK(Diplomacy_RelationBand(-20) == DIPLO_BAND_COLD);
    CHECK(Diplomacy_RelationBand(20) == DIPLO_BAND_FRIENDLY);
    CHECK(Diplomacy_RelationBand(60) == DIPLO_BAND_ALLIED);
}

static void test_war_peace_and_nap(void) {
    setup();
    CHECK(Diplomacy_DeclareWar(&world, 1));
    CHECK(World_AreAtWar(&world, 0, 1));
    CHECK(World_Relation(&world, 0, 1) == -30);
    CHECK(!World_CanDeclareWar(&world, 0, 1));
    CHECK(!Diplomacy_DeclareWar(&world, 1));

    World_MakePeace(&world, 0, 1);
    CHECK(!World_AreAtWar(&world, 0, 1));
    CHECK(World_Relation(&world, 0, 1) == -20);

    World_ChangeRelation(&world, 0, 1, 70);
    CHECK(World_Relation(&world, 0, 1) == 50);
    World_SetTreaty(&world, 0, 1, WORLD_TREATY_NAP, 2);
    CHECK(World_HasTreaty(&world, 0, 1, WORLD_TREATY_NAP));
    CHECK(!World_CanDeclareWar(&world, 0, 1));
    CHECK(!Diplomacy_DeclareWar(&world, 1));

    Diplomacy_ProcessTurn(&world);
    CHECK(World_TreatyTurns(&world, 0, 1) == 1);
    Diplomacy_ProcessTurn(&world);
    CHECK(!World_HasTreaty(&world, 0, 1, WORLD_TREATY_NAP));
    CHECK(World_CanDeclareWar(&world, 0, 1));
}

static void test_gifts_and_diminishing_returns(void) {
    setup();
    world.treasury = 100;
    CHECK(Diplomacy_CanGift(&world, 1));
    CHECK(Diplomacy_GiftGain(&world, 1) == 12);
    CHECK(Diplomacy_SendGift(&world, 1) == 12);
    CHECK(world.treasury == 100 - DIPLO_GIFT_COST);
    CHECK(World_Relation(&world, 0, 1) == 12);
    CHECK(Diplomacy_GiftGain(&world, 1) == 10);
    CHECK(Diplomacy_SendGift(&world, 1) == 10);
    CHECK(world.treasury == 100 - DIPLO_GIFT_COST * 2);
    CHECK(World_Relation(&world, 0, 1) == 22);

    world.treasury = 0;
    CHECK(Diplomacy_SendGift(&world, 1) == DIPLO_ERR_GOLD);

    world.treasury = DIPLO_GIFT_COST;
    World_ChangeRelation(&world, 0, 1, 100);
    CHECK(World_Relation(&world, 0, 1) == 100);
    CHECK(!Diplomacy_CanGift(&world, 1));
    CHECK(Diplomacy_SendGift(&world, 1) == DIPLO_ERR_STATE);
}

static void test_open_borders_share_vision(void) {
    setup();
    int capital_index = world.civs[1].capital_index;
    CHECK(capital_index != WORLD_CIV_NONE);
    const City *capital = &world.cities.cities[capital_index];

    int visible_before = 0;
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            const MapTile *tile = Map_TileAtConst(&map, capital->x + dx,
                                                  capital->y + dy);
            if (tile && tile->visible) visible_before++;
        }
    }

    World_ChangeRelation(&world, 0, 1, DIPLO_OPEN_BORDERS_MIN);
    CHECK(!World_HasTreaty(&world, 0, 1, WORLD_TREATY_OPEN_BORDERS));
    CHECK(Diplomacy_WouldAcceptOpenBorders(&world, 1));
    CHECK(Diplomacy_RequestOpenBorders(&world, 1));
    CHECK(World_HasTreaty(&world, 0, 1, WORLD_TREATY_OPEN_BORDERS));
    CHECK(World_OpenBordersTurns(&world, 0, 1) == DIPLO_OPEN_BORDERS_TURNS);

    World_UpdateVision(&world, &map);
    int visible_after = 0;
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            const MapTile *tile = Map_TileAtConst(&map, capital->x + dx,
                                                  capital->y + dy);
            if (tile && tile->visible) visible_after++;
        }
    }
    CHECK(visible_after > visible_before);

    for (int i = 0; i < DIPLO_OPEN_BORDERS_TURNS; i++) {
        Diplomacy_ProcessTurn(&world);
    }
    CHECK(!World_HasTreaty(&world, 0, 1, WORLD_TREATY_OPEN_BORDERS));
}

static void test_trade_payments_and_expiry(void) {
    setup();
    world.treasury = 0;
    world.civs[1].treasury = 20;
    World_ChangeRelation(&world, 0, 1, DIPLO_TRADE_MIN);
    CHECK(Diplomacy_WouldAcceptTrade(&world, 1));
    CHECK(Diplomacy_RequestTrade(&world, 1));
    CHECK(World_HasTrade(&world, 0, 1));
    CHECK(World_TradeTurns(&world, 0, 1) == WORLD_TRADE_TURNS);
    CHECK(world.civs[1].treasury == 20);

    for (int i = 0; i < WORLD_TRADE_TURNS; i++) {
        Diplomacy_ProcessTurn(&world);
    }
    CHECK(world.treasury == WORLD_TRADE_TURNS * WORLD_TRADE_GOLD);
    CHECK(world.civs[1].treasury == 20 - WORLD_TRADE_TURNS * WORLD_TRADE_GOLD);
    CHECK(!World_HasTrade(&world, 0, 1));

    setup();
    world.civs[1].treasury = 0;
    World_SetTrade(&world, 0, 1, 2);
    Diplomacy_ProcessTurn(&world);
    CHECK(!World_HasTrade(&world, 0, 1));
    CHECK(World_Relation(&world, 0, 1) < 5);
    CHECK(world.treasury == WORLD_START_TREASURY);
}

static void test_peace_acceptance_and_alliance(void) {
    setup();
    CHECK(Diplomacy_DeclareWar(&world, 1));
    world.turn = 12;
    world.civs[1].war_since[0] = 1;
    CHECK(Diplomacy_WouldAcceptPeace(&world, 1));
    CHECK(Diplomacy_RequestPeace(&world, 1));
    CHECK(!World_AreAtWar(&world, 0, 1));

    World_ChangeRelation(&world, 0, 2, 60);
    CHECK(World_Relation(&world, 0, 2) == 60);
    CHECK(Diplomacy_WouldAcceptAlliance(&world, 2));
    CHECK(Diplomacy_RequestAlliance(&world, 2));
    CHECK(World_HasTreaty(&world, 0, 2, WORLD_TREATY_ALLIANCE));
    CHECK(!World_CanDeclareWar(&world, 0, 2));

    World_ChangeRelation(&world, 0, 3, 10);
    CHECK(!Diplomacy_RequestAlliance(&world, 3));
}

static void test_proposals(void) {
    setup();
    World_ChangeRelation(&world, 0, 1, 20);
    world.proposal_cooldown = 0;
    world.proposal_type = WORLD_PROPOSAL_NONE;
    world.proposal_civ = WORLD_CIV_NONE;
    Diplomacy_ProcessTurn(&world);
    CHECK(world.proposal_civ >= 1 && world.proposal_civ <= WORLD_AI_COUNT);
    CHECK(world.proposal_type != WORLD_PROPOSAL_NONE);
    int civ = world.proposal_civ;
    CHECK(Diplomacy_AcceptProposal(&world) != WORLD_PROPOSAL_NONE);
    CHECK(world.proposal_type == WORLD_PROPOSAL_NONE);
    CHECK(World_HasTreaty(&world, 0, civ,
                          WORLD_TREATY_NAP | WORLD_TREATY_ALLIANCE));

    setup();
    World_ChangeRelation(&world, 0, 1, 65);
    world.proposal_cooldown = 0;
    Diplomacy_ProcessTurn(&world);
    CHECK(world.proposal_civ == 1);
    CHECK(world.proposal_type == WORLD_PROPOSAL_ALLIANCE);
    CHECK(Diplomacy_RejectProposal(&world) == WORLD_PROPOSAL_ALLIANCE);
    CHECK(world.proposal_type == WORLD_PROPOSAL_NONE);
    CHECK(World_Relation(&world, 0, 1) < 65);
}

static void test_diplomatic_victory_countdown(void) {
    setup();
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        World_ChangeRelation(&world, 0, civ, 60);
        World_SetTreaty(&world, 0, civ, WORLD_TREATY_ALLIANCE, 0);
        CHECK(World_HasTreaty(&world, 0, civ, WORLD_TREATY_ALLIANCE));
    }
    CHECK(world.game_over == WORLD_GAME_NONE);
    for (int i = 0; i < WORLD_ALLIANCE_VICTORY_TURNS - 1; i++) {
        Diplomacy_ProcessTurn(&world);
        CHECK(world.game_over == WORLD_GAME_NONE);
        CHECK(world.alliance_victory_turns == i + 1);
    }
    Diplomacy_ProcessTurn(&world);
    CHECK(world.alliance_victory_turns == WORLD_ALLIANCE_VICTORY_TURNS);
    CHECK(world.game_over == WORLD_GAME_VICTORY);
    CHECK(world.victory_type == WORLD_VICTORY_DIPLOMATIC);
}

static void test_alliance_victory_resets_on_ai_war(void) {
    setup();
    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        World_ChangeRelation(&world, 0, civ, 60);
        World_SetTreaty(&world, 0, civ, WORLD_TREATY_ALLIANCE, 0);
    }
    Diplomacy_ProcessTurn(&world);
    CHECK(world.alliance_victory_turns == 1);

    World_DeclareWar(&world, 1, 2);
    CHECK(World_AreAtWar(&world, 1, 2));
    Diplomacy_ProcessTurn(&world);
    CHECK(world.alliance_victory_turns == 0);
}

static void test_treaty_blocks_combat(void) {
    setup();
    world.treasury = 25;
    World_ChangeRelation(&world, 0, 1, 20);
    CHECK(Diplomacy_RequestNAP(&world, 1));
    CHECK(World_HasTreaty(&world, 0, 1, WORLD_TREATY_NAP));

    int attacker = Unit_Spawn(&world.units, UNIT_WARRIOR, 0, 0, 0);
    int defender = Unit_Spawn(&world.units, UNIT_WARRIOR, 1, 1, 0);
    CHECK(attacker >= 0);
    CHECK(defender >= 0);
    CHECK(Combat_CanAttack(&world, &map, attacker, defender)
          == COMBAT_ERR_TREATY);

    World_ClearTreaty(&world, 0, 1, WORLD_TREATY_NAP);
    CHECK(Combat_CanAttack(&world, &map, attacker, defender) == COMBAT_OK);
}

int main(void) {
    test_initial_and_bands();
    test_war_peace_and_nap();
    test_gifts_and_diminishing_returns();
    test_open_borders_share_vision();
    test_trade_payments_and_expiry();
    test_peace_acceptance_and_alliance();
    test_proposals();
    test_diplomatic_victory_countdown();
    test_alliance_victory_resets_on_ai_war();
    test_treaty_blocks_combat();

    if (failures == 0) {
        printf("PASS: relations, treaties, gifts, AI proposals, and diplomatic victory\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
