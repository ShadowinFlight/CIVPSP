#ifndef CIVPSP_DIPLOMACY_H
#define CIVPSP_DIPLOMACY_H

#include "world.h"

#define DIPLO_GIFT_COST       10
#define DIPLO_ALLIANCE_MIN    60
#define DIPLO_TRADE_MIN       20
#define DIPLO_OPEN_BORDERS_MIN 10
#define DIPLO_NAP_TURNS       10
#define DIPLO_OPEN_BORDERS_TURNS 10

typedef enum {
    DIPLO_BAND_HOSTILE = 0,
    DIPLO_BAND_COLD,
    DIPLO_BAND_NEUTRAL,
    DIPLO_BAND_FRIENDLY,
    DIPLO_BAND_ALLIED
} DiplomacyBand;

typedef enum {
    DIPLO_OK = 0,
    DIPLO_ERR_GOLD = -1,
    DIPLO_ERR_STATE = -2,
    DIPLO_ERR_RANGE = -3
} DiplomacyResult;

int Diplomacy_RelationBand(int relation);

/* Player-initiated actions.  The Request* functions return 1 when the AI
 * accepts and 0 when it rejects or the action is not currently valid. */
int Diplomacy_DeclareWar(World *world, int civ);
int Diplomacy_RequestPeace(World *world, int civ);
int Diplomacy_RequestNAP(World *world, int civ);
int Diplomacy_RequestOpenBorders(World *world, int civ);
int Diplomacy_RequestTrade(World *world, int civ);
int Diplomacy_RequestAlliance(World *world, int civ);

/* Gift a fixed amount of gold for a diminishing relation gain.  Returns the
 * relation gain (>= 0) or a negative DiplomacyResult. */
int Diplomacy_SendGift(World *world, int civ);
int Diplomacy_GiftGain(const World *world, int civ);
int Diplomacy_CanGift(const World *world, int civ);

/* Acceptance checks exposed for UI hints and host tests. */
int Diplomacy_WouldAcceptPeace(const World *world, int civ);
int Diplomacy_WouldAcceptNAP(const World *world, int civ);
int Diplomacy_WouldAcceptOpenBorders(const World *world, int civ);
int Diplomacy_WouldAcceptTrade(const World *world, int civ);
int Diplomacy_WouldAcceptAlliance(const World *world, int civ);

/* Respond to the current AI proposal.  Returns the proposal type handled, or
 * WORLD_PROPOSAL_NONE when nothing was pending. */
int Diplomacy_AcceptProposal(World *world);
int Diplomacy_RejectProposal(World *world);

/* End-turn processing: treaty expiry, relation decay, AI-to-AI diplomacy,
 * proposals to the player, and the diplomatic-victory countdown. */
void Diplomacy_ProcessTurn(World *world);

#endif
