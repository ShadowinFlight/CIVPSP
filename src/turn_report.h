#ifndef CIVPSP_TURN_REPORT_H
#define CIVPSP_TURN_REPORT_H

#include <stdint.h>

#define TURN_REPORT_MAX 32

typedef enum {
    TURN_REPORT_NONE = 0,
    TURN_REPORT_UNIT_BATTLE,
    TURN_REPORT_CITY_BATTLE,
    TURN_REPORT_WAR,
    TURN_REPORT_PEACE,
    TURN_REPORT_PROPOSAL
} TurnReportType;

/* A small, non-persistent queue of events from the AI/end-turn phase.  The
 * player's own attacks still use the normal bottom toast; this queue feeds the
 * centered turn-start briefing. */
typedef struct {
    uint8_t type;
    uint8_t attacker_owner;
    uint8_t defender_owner;
    uint8_t attacker_type;
    uint8_t defender_type;
    uint8_t city_name_index;
    uint8_t attacker_died;
    uint8_t defender_died;
    uint8_t city_captured;
    uint8_t proposal_type;
    int8_t x;
    int8_t y;
    int16_t attack_damage;
    int16_t counter_damage;
    int16_t city_defense_damage;
    int16_t city_hp_damage;
} TurnReportEvent;

void TurnReport_Clear(void);

void TurnReport_AddUnitBattle(int attacker_owner, int attacker_type,
                              int defender_owner, int defender_type,
                              int x, int y, int attack_damage,
                              int counter_damage, int attacker_died,
                              int defender_died);
void TurnReport_AddCityBattle(int attacker_owner, int attacker_type,
                              int defender_owner, int city_name_index,
                              int x, int y, int defense_damage,
                              int hp_damage, int captured);
void TurnReport_AddWar(int civ_a, int civ_b);
void TurnReport_AddPeace(int civ_a, int civ_b);
void TurnReport_AddProposal(int civ, int target, int proposal_type);

int TurnReport_Count(void);
const TurnReportEvent *TurnReport_At(int index);

#endif
