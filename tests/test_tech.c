#include <stdio.h>
#include <string.h>

#include "data.h"
#include "map.h"
#include "tech.h"
#include "unit.h"
#include "world.h"

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static int research_until_known(World *world, Map *map, int tech,
                                int max_turns) {
    CHECK(World_SetResearch(world, tech) == 1);
    for (int i = 0; i < max_turns; i++) {
        if (Tech_IsKnown(world->researched, tech)) return 1;
        World_ProcessCities(world, map);
    }
    return Tech_IsKnown(world->researched, tech);
}

static void test_tech_data(void) {
    CHECK(TECH_COUNT > 10);
    CHECK(Tech_Cost(TECH_POTTERY) == 10);
    CHECK(Tech_Cost(TECH_WRITING) == 20);
    CHECK(Tech_Cost(TECH_IRON_WORKING) == 40);
    CHECK(Tech_Era(TECH_POTTERY) == ERA_ANCIENT);
    CHECK(Tech_Era(TECH_IRON_WORKING) == ERA_CLASSICAL);
    CHECK(Tech_Era(TECH_PRINTING) == ERA_RENAISSANCE);
    CHECK(!Tech_IsKnown(0, TECH_POTTERY));
    CHECK(Tech_IsKnown(0, TECH_NONE));
    CHECK(Tech_CanSelect(0, TECH_POTTERY));
    CHECK(Tech_CanSelect(0, TECH_MINING));
    CHECK(!Tech_CanSelect(0, TECH_WRITING));
    CHECK(Tech_HasPrereqs(1u << TECH_POTTERY, TECH_WRITING));
    CHECK(!Tech_HasPrereqs(0, TECH_WRITING));
    CHECK(!Tech_CanSelect(1u << TECH_WRITING, TECH_WRITING));
}

static void test_research_flow(void) {
    Map map;
    World world;
    Map_Generate(&map, 0x1234ABCDu);
    World_Begin(&world, &map);

    int city = World_FoundCity(&world, &map, 0);
    CHECK(city >= 0);
    CHECK(world.researched == 0);
    CHECK(World_ResearchType(&world) == TECH_NONE);

    /* Locked tech cannot be selected yet. */
    CHECK(World_SetResearch(&world, TECH_WRITING) == 0);
    CHECK(World_ResearchType(&world) == TECH_NONE);

    CHECK(World_SetResearch(&world, TECH_POTTERY) == 1);
    CHECK(World_ResearchType(&world) == TECH_POTTERY);
    for (int i = 0; i < 10 && !Tech_IsKnown(world.researched, TECH_POTTERY);
         i++) {
        World_ProcessCities(&world, &map);
    }
    CHECK(Tech_IsKnown(world.researched, TECH_POTTERY));
    CHECK(World_ResearchType(&world) == TECH_NONE);
    CHECK(World_ResearchProgress(&world) == 0);

    /* Writing requires Pottery and is now selectable. */
    CHECK(Tech_CanSelect(world.researched, TECH_WRITING));
    CHECK(!Tech_CanSelect(world.researched, TECH_CURRENCY));
    CHECK(research_until_known(&world, &map, TECH_WRITING, 30));
    CHECK(Tech_IsKnown(world.researched, TECH_WRITING));
    CHECK(Tech_CanSelect(world.researched, TECH_CURRENCY));

    /* City science continues to accumulate as a resource. */
    CHECK(world.science > 0);
}

static void test_unlock_data(void) {
    CHECK(Data_ProductionDef(PROD_GRANARY)->required_tech == TECH_POTTERY);
    CHECK(Data_ProductionDef(PROD_LIBRARY)->required_tech == TECH_WRITING);
    CHECK(Data_ProductionDef(PROD_MARKET)->required_tech == TECH_CURRENCY);
    CHECK(Data_ProductionDef(PROD_BARRACKS)->required_tech
          == TECH_BRONZE_WORKING);
    CHECK(Data_ProductionDef(PROD_WALLS)->required_tech == TECH_MASONRY);
    CHECK(Data_ProductionDef(PROD_WORKSHOP)->required_tech
          == TECH_ENGINEERING);
    CHECK(Data_ProductionDef(PROD_UNIVERSITY)->required_tech
          == TECH_IRON_WORKING);
    CHECK(Data_ProductionDef(PROD_AQUEDUCT)->required_tech
          == TECH_CONSTRUCTION);
    CHECK(Data_ProductionDef(PROD_MONUMENT)->required_tech == TECH_NONE);

    CHECK(Data_ProductionDef(PROD_SWORDSMAN)->required_tech
          == TECH_IRON_WORKING);
    CHECK(Data_ProductionDef(PROD_HORSEMAN)->required_tech
          == TECH_HORSEBACK);
    CHECK(Data_ProductionDef(PROD_SWORDSMAN)->kind == PROD_KIND_UNIT);
    CHECK(Data_ProductionDef(PROD_HORSEMAN)->kind == PROD_KIND_UNIT);

    CHECK(Unit_TypeMovePoints(UNIT_SWORDSMAN) == 5);
    CHECK(Unit_TypeMovePoints(UNIT_HORSEMAN) == 10);
    CHECK(Unit_TypeSight(UNIT_SWORDSMAN) == 2);
    CHECK(Unit_TypeSight(UNIT_HORSEMAN) == 2);

    CHECK(Data_ImprovementRequiredTech(IMPROVEMENT_FARM) == TECH_POTTERY);
    CHECK(Data_ImprovementRequiredTech(IMPROVEMENT_MINE) == TECH_MINING);
    CHECK(Data_ImprovementRequiredTech(IMPROVEMENT_PASTURE)
          == TECH_HORSEBACK);
    CHECK(Data_ImprovementRequiredTech(IMPROVEMENT_QUARRY) == TECH_MASONRY);
    CHECK(Data_ImprovementRequiredTech(IMPROVEMENT_ROAD) == TECH_THE_WHEEL);
    CHECK(Data_ImprovementRequiredTech(IMPROVEMENT_NONE) == TECH_NONE);
}

int main(void) {
    test_tech_data();
    test_research_flow();
    test_unlock_data();
    if (failures) {
        printf("%d tech test(s) failed\n", failures);
        return 1;
    }
    printf("PASS: tech tree, research flow, tech-gated production, and units\n");
    return 0;
}
