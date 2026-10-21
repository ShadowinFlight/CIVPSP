#include "diplomacy.h"

#include <stdio.h>

#include "gen/strings_gen.h"
#include "log.h"

static int valid_rival(const World *world, int civ) {
    if (!world || civ < 1 || civ > WORLD_AI_COUNT) return 0;
    return world->civs[civ].active != 0;
}

static int rival_has_city(const World *world, int civ) {
    return valid_rival(world, civ) && World_CountCities(world, civ) > 0;
}

static void clear_proposal(World *world, int cooldown) {
    if (!world) return;
    world->proposal_civ = WORLD_CIV_NONE;
    world->proposal_type = WORLD_PROPOSAL_NONE;
    world->proposal_cooldown = (uint8_t)cooldown;
}

static void set_proposal(World *world, int civ, int type) {
    world->proposal_civ = (uint8_t)civ;
    world->proposal_type = (uint8_t)type;
    world->proposal_cooldown = 6;
    Log_Printf("diplo: proposal civ=%d type=%d turn=%d", civ, type,
               world->turn);

    if (world->notice[0] != '\0') return;
    StrId offer = S_DIPLO_OFFERS_PEACE;
    if (type == WORLD_PROPOSAL_NAP) offer = S_DIPLO_OFFERS_NAP;
    if (type == WORLD_PROPOSAL_ALLIANCE) offer = S_DIPLO_OFFERS_ALLIANCE;
    char text[96];
    snprintf(text, sizeof(text), "%s %s", Str(World_CivNameId(civ)),
             Str(offer));
    World_SetNotice(world, text);
}

int Diplomacy_RelationBand(int relation) {
    if (relation <= -40) return DIPLO_BAND_HOSTILE;
    if (relation <= -10) return DIPLO_BAND_COLD;
    if (relation < 10) return DIPLO_BAND_NEUTRAL;
    if (relation < DIPLO_ALLIANCE_MIN) return DIPLO_BAND_FRIENDLY;
    return DIPLO_BAND_ALLIED;
}

int Diplomacy_DeclareWar(World *world, int civ) {
    if (!World_CanDeclareWar(world, WORLD_CIV_PLAYER, civ)) return 0;
    World_DeclareWar(world, WORLD_CIV_PLAYER, civ);
    return 1;
}

int Diplomacy_WouldAcceptPeace(const World *world, int civ) {
    if (!world || !World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) return 0;

    int since = world->turn - (int)world->civs[civ].war_since[WORLD_CIV_PLAYER];
    if (since < 0) since = 0;
    int ai_military = World_MilitaryCount(world, civ);
    int player_military = World_MilitaryCount(world, WORLD_CIV_PLAYER);
    int score = World_Relation(world, WORLD_CIV_PLAYER, civ) / 2
              + since * 3
              + (player_military - ai_military) * 8;
    if (ai_military <= 1) score += 30;
    if (World_CountCities(world, civ) == 0) score += 60;
    return score >= 0;
}

int Diplomacy_WouldAcceptNAP(const World *world, int civ) {
    if (!world || !rival_has_city(world, civ)) return 0;
    if (World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) return 0;
    if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                        WORLD_TREATY_NAP | WORLD_TREATY_ALLIANCE)) {
        return 0;
    }
    return World_Relation(world, WORLD_CIV_PLAYER, civ) >= 0;
}

int Diplomacy_WouldAcceptAlliance(const World *world, int civ) {
    if (!world || !rival_has_city(world, civ)) return 0;
    if (World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) return 0;
    if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                        WORLD_TREATY_ALLIANCE)) {
        return 0;
    }
    return World_Relation(world, WORLD_CIV_PLAYER, civ)
           >= DIPLO_ALLIANCE_MIN;
}

int Diplomacy_RequestPeace(World *world, int civ) {
    if (!world || !World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) return 0;
    if (!Diplomacy_WouldAcceptPeace(world, civ)) {
        World_ChangeRelation(world, WORLD_CIV_PLAYER, civ, -3);
        return 0;
    }
    World_MakePeace(world, WORLD_CIV_PLAYER, civ);
    return 1;
}

int Diplomacy_RequestNAP(World *world, int civ) {
    if (!Diplomacy_WouldAcceptNAP(world, civ)) return 0;
    World_SetTreaty(world, WORLD_CIV_PLAYER, civ, WORLD_TREATY_NAP,
                    DIPLO_NAP_TURNS);
    return 1;
}

int Diplomacy_WouldAcceptOpenBorders(const World *world, int civ) {
    if (!world || !rival_has_city(world, civ)) return 0;
    if (World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) return 0;
    if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                        WORLD_TREATY_OPEN_BORDERS | WORLD_TREATY_ALLIANCE)) {
        return 0;
    }
    return World_Relation(world, WORLD_CIV_PLAYER, civ)
           >= DIPLO_OPEN_BORDERS_MIN;
}

int Diplomacy_RequestOpenBorders(World *world, int civ) {
    if (!Diplomacy_WouldAcceptOpenBorders(world, civ)) return 0;
    World_SetTreaty(world, WORLD_CIV_PLAYER, civ,
                    WORLD_TREATY_OPEN_BORDERS, DIPLO_OPEN_BORDERS_TURNS);
    return 1;
}

int Diplomacy_WouldAcceptTrade(const World *world, int civ) {
    if (!world || !rival_has_city(world, civ)) return 0;
    if (World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) return 0;
    if (World_HasTrade(world, WORLD_CIV_PLAYER, civ)) return 0;
    if (world->civs[civ].treasury < 10) return 0;
    return World_Relation(world, WORLD_CIV_PLAYER, civ) >= DIPLO_TRADE_MIN;
}

int Diplomacy_RequestTrade(World *world, int civ) {
    if (!Diplomacy_WouldAcceptTrade(world, civ)) return 0;
    World_SetTrade(world, WORLD_CIV_PLAYER, civ, WORLD_TRADE_TURNS);
    return 1;
}

int Diplomacy_RequestAlliance(World *world, int civ) {
    if (!Diplomacy_WouldAcceptAlliance(world, civ)) return 0;
    World_SetTreaty(world, WORLD_CIV_PLAYER, civ, WORLD_TREATY_ALLIANCE, 0);
    return 1;
}

int Diplomacy_CanGift(const World *world, int civ) {
    if (!world || !rival_has_city(world, civ)) return 0;
    if (world->treasury < DIPLO_GIFT_COST) return 0;
    if (World_Relation(world, WORLD_CIV_PLAYER, civ) >= 100) return 0;
    return 1;
}

int Diplomacy_GiftGain(const World *world, int civ) {
    if (!world || !valid_rival(world, civ)) return 0;
    int count = world->gift_count[WORLD_CIV_PLAYER][civ];
    int gain = 12 - count * 2;
    if (gain < 4) gain = 4;
    if (World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) {
        gain = (gain + 1) / 2;
        if (gain < 1) gain = 1;
    }
    int relation = World_Relation(world, WORLD_CIV_PLAYER, civ);
    if (relation + gain > 100) gain = 100 - relation;
    if (gain < 0) gain = 0;
    return gain;
}

int Diplomacy_SendGift(World *world, int civ) {
    if (!world || civ < 1 || civ > WORLD_AI_COUNT) return DIPLO_ERR_RANGE;
    if (!rival_has_city(world, civ)) return DIPLO_ERR_STATE;
    if (world->treasury < DIPLO_GIFT_COST) return DIPLO_ERR_GOLD;
    int gain = Diplomacy_GiftGain(world, civ);
    if (gain <= 0) return DIPLO_ERR_STATE;

    world->treasury -= DIPLO_GIFT_COST;
    world->gift_count[WORLD_CIV_PLAYER][civ]++;
    world->gift_count[civ][WORLD_CIV_PLAYER]++;
    World_ChangeRelation(world, WORLD_CIV_PLAYER, civ, gain);
    return gain;
}

int Diplomacy_AcceptProposal(World *world) {
    if (!world || world->proposal_type == WORLD_PROPOSAL_NONE) {
        return WORLD_PROPOSAL_NONE;
    }
    int civ = world->proposal_civ;
    int type = world->proposal_type;
    if (!rival_has_city(world, civ)) {
        clear_proposal(world, 2);
        return WORLD_PROPOSAL_NONE;
    }
    if (type != WORLD_PROPOSAL_PEACE
        && World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) {
        clear_proposal(world, 2);
        return WORLD_PROPOSAL_NONE;
    }

    if (type == WORLD_PROPOSAL_PEACE) {
        if (World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) {
            World_MakePeace(world, WORLD_CIV_PLAYER, civ);
        }
    } else if (type == WORLD_PROPOSAL_NAP) {
        World_SetTreaty(world, WORLD_CIV_PLAYER, civ, WORLD_TREATY_NAP,
                        DIPLO_NAP_TURNS);
    } else if (type == WORLD_PROPOSAL_ALLIANCE) {
        World_SetTreaty(world, WORLD_CIV_PLAYER, civ, WORLD_TREATY_ALLIANCE,
                        0);
    }
    clear_proposal(world, 5);
    return type;
}

int Diplomacy_RejectProposal(World *world) {
    if (!world || world->proposal_type == WORLD_PROPOSAL_NONE) {
        return WORLD_PROPOSAL_NONE;
    }
    int civ = world->proposal_civ;
    int type = world->proposal_type;
    if (valid_rival(world, civ)) {
        World_ChangeRelation(world, WORLD_CIV_PLAYER, civ, -5);
    }
    clear_proposal(world, 4);
    return type;
}

static void process_trade(World *world) {
    for (int a = 0; a < WORLD_CIV_COUNT; a++) {
        if (a == WORLD_CIV_BARBARIAN) continue;
        for (int b = a + 1; b < WORLD_CIV_COUNT; b++) {
            if (b == WORLD_CIV_BARBARIAN) continue;
            if (!World_HasTrade(world, a, b)) continue;

            if (World_AreAtWar(world, a, b)) {
                World_ClearTrade(world, a, b);
                continue;
            }

            int payer;
            int receiver;
            if (a == WORLD_CIV_PLAYER) {
                payer = b;
                receiver = a;
            } else if (b == WORLD_CIV_PLAYER) {
                payer = a;
                receiver = b;
            } else {
                /* AI-to-AI trade is not exposed yet. */
                World_ClearTrade(world, a, b);
                continue;
            }

            if (world->civs[payer].treasury < WORLD_TRADE_GOLD) {
                Log_Printf("diplo: trade ended payer=%d receiver=%d "
                           "treasury=%d", payer, receiver,
                           (int)world->civs[payer].treasury);
                World_ClearTrade(world, a, b);
                World_ChangeRelation(world, a, b, -5);
                if (payer == WORLD_CIV_PLAYER || receiver == WORLD_CIV_PLAYER) {
                    int civ = payer == WORLD_CIV_PLAYER ? receiver : payer;
                    char text[96];
                    snprintf(text, sizeof(text), "%s %s",
                             Str(World_CivNameId(civ)),
                             Str(S_DIPLO_TRADE_ENDED));
                    World_SetNotice(world, text);
                }
                continue;
            }

            world->civs[payer].treasury -= WORLD_TRADE_GOLD;
            Log_Printf("diplo: trade payer=%d receiver=%d gold=%d turns=%d",
                       payer, receiver, WORLD_TRADE_GOLD,
                       world->trade_turns[a][b]);
            if (receiver == WORLD_CIV_PLAYER) {
                world->treasury += WORLD_TRADE_GOLD;
                world->gold_per_turn += WORLD_TRADE_GOLD;
            } else {
                world->civs[receiver].treasury += WORLD_TRADE_GOLD;
            }

            uint8_t turns = (uint8_t)(world->trade_turns[a][b] - 1);
            world->trade_turns[a][b] = turns;
            world->trade_turns[b][a] = turns;
            if (turns == 0) World_ClearTrade(world, a, b);
        }
    }
}

static void tick_treaties(World *world) {
    for (int a = 0; a < WORLD_CIV_COUNT; a++) {
        if (a == WORLD_CIV_BARBARIAN) continue;
        for (int b = a + 1; b < WORLD_CIV_COUNT; b++) {
            if (b == WORLD_CIV_BARBARIAN) continue;
            uint8_t mask = world->treaty_mask[a][b];
            uint8_t expired = 0;

            if (mask & WORLD_TREATY_NAP) {
                if (world->treaty_turns[a][b] > 0) {
                    world->treaty_turns[a][b]--;
                    world->treaty_turns[b][a] = world->treaty_turns[a][b];
                }
                if (world->treaty_turns[a][b] == 0) {
                    world->treaty_mask[a][b] &= (uint8_t)~WORLD_TREATY_NAP;
                    world->treaty_mask[b][a] &= (uint8_t)~WORLD_TREATY_NAP;
                    expired |= WORLD_TREATY_NAP;
                }
            }
            if (mask & WORLD_TREATY_OPEN_BORDERS) {
                if (world->open_borders_turns[a][b] > 0) {
                    world->open_borders_turns[a][b]--;
                    world->open_borders_turns[b][a] =
                        world->open_borders_turns[a][b];
                }
                if (world->open_borders_turns[a][b] == 0) {
                    world->treaty_mask[a][b] &=
                        (uint8_t)~WORLD_TREATY_OPEN_BORDERS;
                    world->treaty_mask[b][a] &=
                        (uint8_t)~WORLD_TREATY_OPEN_BORDERS;
                    expired |= WORLD_TREATY_OPEN_BORDERS;
                }
            }
            if (expired) {
                Log_Printf("diplo: treaty expired a=%d b=%d mask=%d", a, b,
                           expired);
            }
        }
    }
}

static void decay_relations(World *world) {
    /* Relations drift slowly: a treaty or gift survives long enough to matter,
     * while wartime hostility deepens only every few turns. */
    if (world->turn % 3 != 0) return;

    for (int a = 0; a < WORLD_CIV_COUNT; a++) {
        if (a == WORLD_CIV_BARBARIAN) continue;
        for (int b = a + 1; b < WORLD_CIV_COUNT; b++) {
            if (b == WORLD_CIV_BARBARIAN) continue;
            int relation = world->relation[a][b];
            if (World_AreAtWar(world, a, b)) {
                World_ChangeRelation(world, a, b, -1);
            } else if (relation > 0) {
                World_ChangeRelation(world, a, b, -1);
            } else if (relation < 0) {
                World_ChangeRelation(world, a, b, 1);
            }
        }
    }
}

/* Rivals occasionally settle their own disputes.  This is deliberately
 * low-frequency so the player's diplomacy remains the focus, while the world
 * still changes without the player. */
static void ai_to_ai_diplomacy(World *world) {
    for (int a = 1; a <= WORLD_AI_COUNT; a++) {
        if (!world->civs[a].active) continue;
        for (int b = a + 1; b <= WORLD_AI_COUNT; b++) {
            if (!world->civs[b].active) continue;

            if (World_AreAtWar(world, a, b)) {
                int since = world->turn - (int)world->civs[a].war_since[b];
                int weak = World_MilitaryCount(world, a) <= 1
                        || World_MilitaryCount(world, b) <= 1;
                if (since >= 10 && (weak || (world->turn + a + b) % 5 == 0)) {
                    Log_Printf("diplo: ai peace a=%d b=%d turn=%d", a, b,
                               world->turn);
                    World_MakePeace(world, a, b);
                }
                continue;
            }

            if (world->turn < 24) continue;
            if (World_HasTreaty(world, a, b,
                                WORLD_TREATY_NAP | WORLD_TREATY_ALLIANCE)) {
                continue;
            }
            if (World_HasTreaty(world, a, WORLD_CIV_PLAYER,
                                WORLD_TREATY_ALLIANCE)
                || World_HasTreaty(world, b, WORLD_CIV_PLAYER,
                                   WORLD_TREATY_ALLIANCE)) {
                continue;
            }
            if (World_Relation(world, a, b) > 0) continue;
            if (World_MilitaryCount(world, a) < 4
                || World_MilitaryCount(world, b) < 4) {
                continue;
            }
            if ((world->turn + a * 7 + b * 11) % 31 == 0) {
                Log_Printf("diplo: ai war a=%d b=%d turn=%d", a, b,
                           world->turn);
                World_DeclareWar(world, a, b);
            }
        }
    }
}

static void generate_ai_proposals(World *world) {
    if (world->game_over != WORLD_GAME_NONE) return;
    if (World_PlayerCityCount(world) == 0) return;

    if (world->proposal_type != WORLD_PROPOSAL_NONE) {
        int civ = world->proposal_civ;
        int type = world->proposal_type;
        if (!rival_has_city(world, civ)) {
            clear_proposal(world, 2);
        } else if (type == WORLD_PROPOSAL_PEACE
                   && !World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) {
            clear_proposal(world, 2);
        } else if (type != WORLD_PROPOSAL_PEACE
                   && World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) {
            clear_proposal(world, 2);
        } else {
            return;
        }
    }
    if (world->proposal_cooldown > 0) {
        world->proposal_cooldown--;
        return;
    }

    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        if (!rival_has_city(world, civ)) continue;

        if (World_AreAtWar(world, WORLD_CIV_PLAYER, civ)) {
            int since = world->turn
                      - (int)world->civs[civ].war_since[WORLD_CIV_PLAYER];
            if (since >= 6
                && World_Relation(world, WORLD_CIV_PLAYER, civ) > -80
                && Diplomacy_WouldAcceptPeace(world, civ)) {
                set_proposal(world, civ, WORLD_PROPOSAL_PEACE);
                return;
            }
            continue;
        }

        if (World_Relation(world, WORLD_CIV_PLAYER, civ)
                >= DIPLO_ALLIANCE_MIN
            && !World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                                WORLD_TREATY_ALLIANCE)) {
            set_proposal(world, civ, WORLD_PROPOSAL_ALLIANCE);
            return;
        }
        if (World_Relation(world, WORLD_CIV_PLAYER, civ) >= 10
            && !World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                                WORLD_TREATY_NAP | WORLD_TREATY_ALLIANCE)) {
            set_proposal(world, civ, WORLD_PROPOSAL_NAP);
            return;
        }
    }
}

static void update_diplomatic_victory(World *world) {
    int living = World_LivingRivals(world);
    int all_allied = living >= 2;

    for (int civ = 1; civ <= WORLD_AI_COUNT && all_allied; civ++) {
        if (!world->civs[civ].active) continue;
        if (World_CountCities(world, civ) == 0) continue;
        if (!World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                             WORLD_TREATY_ALLIANCE)) {
            all_allied = 0;
        }
    }

    /* A diplomatic victory requires a peaceful alliance network, not merely
     * separate alliances with rivals who are still fighting one another. */
    for (int a = 1; a <= WORLD_AI_COUNT && all_allied; a++) {
        if (!world->civs[a].active || World_CountCities(world, a) == 0) {
            continue;
        }
        for (int b = a + 1; b <= WORLD_AI_COUNT; b++) {
            if (!world->civs[b].active || World_CountCities(world, b) == 0) {
                continue;
            }
            if (World_AreAtWar(world, a, b)) all_allied = 0;
        }
    }

    if (!all_allied) {
        world->alliance_victory_turns = 0;
        return;
    }

    if (world->alliance_victory_turns < 255) {
        world->alliance_victory_turns++;
    }
    if (world->alliance_victory_turns >= WORLD_ALLIANCE_VICTORY_TURNS
        && world->game_over == WORLD_GAME_NONE) {
        world->game_over = WORLD_GAME_VICTORY;
        world->victory_type = WORLD_VICTORY_DIPLOMATIC;
        World_SetNotice(world, Str(S_GAME_VICTORY_DIPLOMATIC));
    }
}

void Diplomacy_ProcessTurn(World *world) {
    if (!world || world->game_over != WORLD_GAME_NONE) return;

    tick_treaties(world);
    process_trade(world);
    decay_relations(world);
    ai_to_ai_diplomacy(world);
    generate_ai_proposals(world);
    update_diplomatic_victory(world);
}
