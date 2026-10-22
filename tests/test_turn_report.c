#include <stdio.h>

#include "turn_report.h"
#include "world.h"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_queue(void) {
    TurnReport_Clear();
    CHECK(TurnReport_Count() == 0);
    CHECK(TurnReport_At(0) == NULL);

    TurnReport_AddUnitBattle(1, 2, 0, 1, 14, 8, 25, 10, 0, 1);
    CHECK(TurnReport_Count() == 1);
    const TurnReportEvent *event = TurnReport_At(0);
    CHECK(event != NULL);
    if (event) {
        CHECK(event->type == TURN_REPORT_UNIT_BATTLE);
        CHECK(event->attacker_owner == 1);
        CHECK(event->attacker_type == 2);
        CHECK(event->defender_owner == 0);
        CHECK(event->defender_type == 1);
        CHECK(event->x == 14);
        CHECK(event->y == 8);
        CHECK(event->attack_damage == 25);
        CHECK(event->counter_damage == 10);
        CHECK(event->defender_died == 1);
        CHECK(event->attacker_died == 0);
    }

    TurnReport_AddCityBattle(1, 3, 0, 4, 9, 7, 30, 0, 1);
    CHECK(TurnReport_Count() == 2);
    event = TurnReport_At(1);
    CHECK(event != NULL);
    if (event) {
        CHECK(event->type == TURN_REPORT_CITY_BATTLE);
        CHECK(event->attacker_owner == 1);
        CHECK(event->attacker_type == 3);
        CHECK(event->defender_owner == 0);
        CHECK(event->city_name_index == 4);
        CHECK(event->x == 9);
        CHECK(event->y == 7);
        CHECK(event->city_defense_damage == 30);
        CHECK(event->city_hp_damage == 0);
        CHECK(event->city_captured == 1);
    }

    TurnReport_AddWar(2, 0);
    TurnReport_AddPeace(2, 3);
    TurnReport_AddProposal(1, 0, WORLD_PROPOSAL_NAP);
    CHECK(TurnReport_Count() == 5);
    event = TurnReport_At(2);
    CHECK(event != NULL && event->type == TURN_REPORT_WAR);
    CHECK(event != NULL && event->attacker_owner == 2
          && event->defender_owner == 0);
    event = TurnReport_At(3);
    CHECK(event != NULL && event->type == TURN_REPORT_PEACE);
    event = TurnReport_At(4);
    CHECK(event != NULL && event->type == TURN_REPORT_PROPOSAL);
    CHECK(event != NULL && event->proposal_type == WORLD_PROPOSAL_NAP);
}

static void test_capacity_and_clear(void) {
    TurnReport_Clear();
    for (int i = 0; i < TURN_REPORT_MAX + 5; i++) {
        TurnReport_AddWar(1, 0);
    }
    CHECK(TurnReport_Count() == TURN_REPORT_MAX);
    CHECK(TurnReport_At(TURN_REPORT_MAX - 1) != NULL);
    CHECK(TurnReport_At(TURN_REPORT_MAX) == NULL);

    TurnReport_Clear();
    CHECK(TurnReport_Count() == 0);
    CHECK(TurnReport_At(0) == NULL);
}

int main(void) {
    test_queue();
    test_capacity_and_clear();
    if (failures == 0) {
        printf("turn report tests passed\n");
        return 0;
    }
    return 1;
}
