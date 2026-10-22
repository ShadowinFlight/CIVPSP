#include "turn_report.h"

#include <string.h>

#include "log.h"

static TurnReportEvent events[TURN_REPORT_MAX];
static int event_count;

static TurnReportEvent *append_event(TurnReportType type) {
    if (event_count >= TURN_REPORT_MAX) {
        Log_Printf("report: queue full, dropped event type=%d", (int)type);
        return NULL;
    }
    TurnReportEvent *event = &events[event_count++];
    memset(event, 0, sizeof(*event));
    event->type = (uint8_t)type;
    return event;
}

void TurnReport_Clear(void) {
    event_count = 0;
}

void TurnReport_AddUnitBattle(int attacker_owner, int attacker_type,
                              int defender_owner, int defender_type,
                              int x, int y, int attack_damage,
                              int counter_damage, int attacker_died,
                              int defender_died) {
    TurnReportEvent *event = append_event(TURN_REPORT_UNIT_BATTLE);
    if (!event) return;
    event->attacker_owner = (uint8_t)attacker_owner;
    event->attacker_type = (uint8_t)attacker_type;
    event->defender_owner = (uint8_t)defender_owner;
    event->defender_type = (uint8_t)defender_type;
    event->x = (int8_t)x;
    event->y = (int8_t)y;
    event->attack_damage = (int16_t)attack_damage;
    event->counter_damage = (int16_t)counter_damage;
    event->attacker_died = (uint8_t)(attacker_died ? 1 : 0);
    event->defender_died = (uint8_t)(defender_died ? 1 : 0);
}

void TurnReport_AddCityBattle(int attacker_owner, int attacker_type,
                              int defender_owner, int city_name_index,
                              int x, int y, int defense_damage,
                              int hp_damage, int captured) {
    TurnReportEvent *event = append_event(TURN_REPORT_CITY_BATTLE);
    if (!event) return;
    event->attacker_owner = (uint8_t)attacker_owner;
    event->attacker_type = (uint8_t)attacker_type;
    event->defender_owner = (uint8_t)defender_owner;
    event->city_name_index = (uint8_t)city_name_index;
    event->x = (int8_t)x;
    event->y = (int8_t)y;
    event->city_defense_damage = (int16_t)defense_damage;
    event->city_hp_damage = (int16_t)hp_damage;
    event->city_captured = (uint8_t)(captured ? 1 : 0);
}

void TurnReport_AddWar(int civ_a, int civ_b) {
    TurnReportEvent *event = append_event(TURN_REPORT_WAR);
    if (!event) return;
    event->attacker_owner = (uint8_t)civ_a;
    event->defender_owner = (uint8_t)civ_b;
}

void TurnReport_AddPeace(int civ_a, int civ_b) {
    TurnReportEvent *event = append_event(TURN_REPORT_PEACE);
    if (!event) return;
    event->attacker_owner = (uint8_t)civ_a;
    event->defender_owner = (uint8_t)civ_b;
}

void TurnReport_AddProposal(int civ, int target, int proposal_type) {
    TurnReportEvent *event = append_event(TURN_REPORT_PROPOSAL);
    if (!event) return;
    event->attacker_owner = (uint8_t)civ;
    event->defender_owner = (uint8_t)target;
    event->proposal_type = (uint8_t)proposal_type;
}

int TurnReport_Count(void) {
    return event_count;
}

const TurnReportEvent *TurnReport_At(int index) {
    if (index < 0 || index >= event_count) return NULL;
    return &events[index];
}
