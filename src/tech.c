#include "tech.h"

const TechDef g_tech_defs[TECH_COUNT] = {
    [TECH_NONE] = {
        S_TECH_NONE, ERA_ANCIENT, 0, -1, -1, S_TECH_UNLOCK_NONE,
        TECH_EFFECT_NONE, 0
    },
    /* Ancient (8) */
    [TECH_POTTERY] = {
        S_TECH_POTTERY, ERA_ANCIENT, 10, -1, -1, S_TECH_UNLOCK_POTTERY,
        TECH_EFFECT_NONE, 0
    },
    [TECH_MINING] = {
        S_TECH_MINING, ERA_ANCIENT, 15, -1, -1, S_TECH_UNLOCK_MINING,
        TECH_EFFECT_NONE, 0
    },
    [TECH_MASONRY] = {
        S_TECH_MASONRY, ERA_ANCIENT, 15, -1, -1, S_TECH_UNLOCK_MASONRY,
        TECH_EFFECT_NONE, 0
    },
    [TECH_THE_WHEEL] = {
        S_TECH_THE_WHEEL, ERA_ANCIENT, 15, -1, -1, S_TECH_UNLOCK_WHEEL,
        TECH_EFFECT_NONE, 0
    },
    [TECH_WRITING] = {
        S_TECH_WRITING, ERA_ANCIENT, 20, TECH_POTTERY, -1,
        S_TECH_UNLOCK_WRITING, TECH_EFFECT_NONE, 0
    },
    [TECH_BRONZE_WORKING] = {
        S_TECH_BRONZE_WORKING, ERA_ANCIENT, 25, TECH_MINING, -1,
        S_TECH_UNLOCK_BRONZE, TECH_EFFECT_NONE, 0
    },
    [TECH_HORSEBACK] = {
        S_TECH_HORSEBACK, ERA_ANCIENT, 20, -1, -1, S_TECH_UNLOCK_HORSEBACK,
        TECH_EFFECT_NONE, 0
    },
    [TECH_SAILING] = {
        S_TECH_SAILING, ERA_ANCIENT, 18, TECH_POTTERY, -1,
        S_TECH_UNLOCK_SAILING, TECH_EFFECT_EMPIRE_GOLD, 1
    },
    /* Classical (8) */
    [TECH_IRRIGATION] = {
        S_TECH_IRRIGATION, ERA_CLASSICAL, 20, TECH_POTTERY, -1,
        S_TECH_UNLOCK_IRRIGATION, TECH_EFFECT_FARM_FOOD, 1
    },
    [TECH_MATHEMATICS] = {
        S_TECH_MATHEMATICS, ERA_CLASSICAL, 30, TECH_WRITING, -1,
        S_TECH_UNLOCK_MATH, TECH_EFFECT_NONE, 0
    },
    [TECH_CURRENCY] = {
        S_TECH_CURRENCY, ERA_CLASSICAL, 30, TECH_WRITING, -1,
        S_TECH_UNLOCK_CURRENCY, TECH_EFFECT_NONE, 0
    },
    [TECH_ENGINEERING] = {
        S_TECH_ENGINEERING, ERA_CLASSICAL, 35, TECH_MASONRY, TECH_MATHEMATICS,
        S_TECH_UNLOCK_ENGINEERING, TECH_EFFECT_NONE, 0
    },
    [TECH_IRON_WORKING] = {
        S_TECH_IRON_WORKING, ERA_CLASSICAL, 40, TECH_BRONZE_WORKING, -1,
        S_TECH_UNLOCK_IRON, TECH_EFFECT_NONE, 0
    },
    [TECH_CONSTRUCTION] = {
        S_TECH_CONSTRUCTION, ERA_CLASSICAL, 30, TECH_MASONRY, -1,
        S_TECH_UNLOCK_CONSTRUCTION, TECH_EFFECT_NONE, 0
    },
    [TECH_PHILOSOPHY] = {
        S_TECH_PHILOSOPHY, ERA_CLASSICAL, 30, TECH_WRITING, -1,
        S_TECH_UNLOCK_PHILOSOPHY, TECH_EFFECT_LIBRARY_SCIENCE, 1
    },
    [TECH_LAW] = {
        S_TECH_LAW, ERA_CLASSICAL, 30, TECH_WRITING, -1,
        S_TECH_UNLOCK_LAW, TECH_EFFECT_EMPIRE_GOLD, 1
    },
    /* Medieval (7) */
    [TECH_EDUCATION] = {
        S_TECH_EDUCATION, ERA_MEDIEVAL, 45, TECH_IRON_WORKING, -1,
        S_TECH_UNLOCK_EDUCATION, TECH_EFFECT_UNIVERSITY_SCIENCE, 1
    },
    [TECH_MACHINERY] = {
        S_TECH_MACHINERY, ERA_MEDIEVAL, 45, TECH_ENGINEERING, -1,
        S_TECH_UNLOCK_MACHINERY, TECH_EFFECT_WORKSHOP_PROD, 1
    },
    [TECH_FEUDALISM] = {
        S_TECH_FEUDALISM, ERA_MEDIEVAL, 50, TECH_IRON_WORKING, -1,
        S_TECH_UNLOCK_FEUDALISM, TECH_EFFECT_WALL_DEFENSE, 1
    },
    [TECH_CIVIL_SERVICE] = {
        S_TECH_CIVIL_SERVICE, ERA_MEDIEVAL, 45, TECH_IRRIGATION,
        TECH_PHILOSOPHY, S_TECH_UNLOCK_CIVIL_SERVICE,
        TECH_EFFECT_FARM_FOOD, 1
    },
    [TECH_GUILDS] = {
        S_TECH_GUILDS, ERA_MEDIEVAL, 45, TECH_CURRENCY, -1,
        S_TECH_UNLOCK_GUILDS, TECH_EFFECT_MARKET_GOLD, 1
    },
    [TECH_THEOLOGY] = {
        S_TECH_THEOLOGY, ERA_MEDIEVAL, 50, TECH_PHILOSOPHY, TECH_EDUCATION,
        S_TECH_UNLOCK_THEOLOGY, TECH_EFFECT_EMPIRE_SCIENCE, 1
    },
    [TECH_COMPASS] = {
        S_TECH_COMPASS, ERA_MEDIEVAL, 45, TECH_MACHINERY, TECH_SAILING,
        S_TECH_UNLOCK_COMPASS, TECH_EFFECT_EMPIRE_GOLD, 1
    },
    /* Renaissance (7) */
    [TECH_PRINTING] = {
        S_TECH_PRINTING, ERA_RENAISSANCE, 50, TECH_EDUCATION, -1,
        S_TECH_UNLOCK_PRINTING, TECH_EFFECT_RESEARCH_PCT, 10
    },
    [TECH_GUNPOWDER] = {
        S_TECH_GUNPOWDER, ERA_RENAISSANCE, 55, TECH_FEUDALISM, -1,
        S_TECH_UNLOCK_GUNPOWDER, TECH_EFFECT_NONE, 0
    },
    [TECH_ASTRONOMY] = {
        S_TECH_ASTRONOMY, ERA_RENAISSANCE, 55, TECH_COMPASS,
        TECH_MATHEMATICS, S_TECH_UNLOCK_ASTRONOMY,
        TECH_EFFECT_LIBRARY_SCIENCE, 1
    },
    [TECH_BANKING] = {
        S_TECH_BANKING, ERA_RENAISSANCE, 55, TECH_GUILDS, TECH_CURRENCY,
        S_TECH_UNLOCK_BANKING, TECH_EFFECT_MARKET_GOLD, 1
    },
    [TECH_SCIENTIFIC_METHOD] = {
        S_TECH_SCIENTIFIC_METHOD, ERA_RENAISSANCE, 60, TECH_PRINTING,
        TECH_PHILOSOPHY, S_TECH_UNLOCK_SCIENTIFIC_METHOD,
        TECH_EFFECT_RESEARCH_PCT, 10
    },
    [TECH_NAVIGATION] = {
        S_TECH_NAVIGATION, ERA_RENAISSANCE, 60, TECH_ASTRONOMY,
        TECH_SAILING, S_TECH_UNLOCK_NAVIGATION,
        TECH_EFFECT_EMPIRE_GOLD, 1
    },
    [TECH_MILITARY_SCIENCE] = {
        S_TECH_MILITARY_SCIENCE, ERA_RENAISSANCE, 60, TECH_GUNPOWDER,
        TECH_FEUDALISM, S_TECH_UNLOCK_MILITARY_SCIENCE,
        TECH_EFFECT_BARRACKS_PROD, 1
    },
};

int Tech_IsResearchedType(int type) {
    return type > TECH_NONE && type < TECH_COUNT;
}

int Tech_IsKnown(uint32_t researched, int type) {
    if (!Tech_IsResearchedType(type)) return 1;
    return (researched & (1u << type)) != 0;
}

int Tech_HasPrereqs(uint32_t researched, int type) {
    if (!Tech_IsResearchedType(type)) return 1;
    const TechDef *t = &g_tech_defs[type];
    if (t->prereq1 >= 0 && !Tech_IsKnown(researched, t->prereq1)) return 0;
    if (t->prereq2 >= 0 && !Tech_IsKnown(researched, t->prereq2)) return 0;
    return 1;
}

int Tech_CanSelect(uint32_t researched, int type) {
    if (!Tech_IsResearchedType(type)) return 0;
    if (Tech_IsKnown(researched, type)) return 0;
    return Tech_HasPrereqs(researched, type);
}

int Tech_CountKnown(uint32_t researched) {
    int count = 0;
    for (int tech = 1; tech < TECH_COUNT; tech++) {
        if (researched & (1u << tech)) count++;
    }
    return count;
}

StrId Tech_Name(int type) {
    if (!Tech_IsResearchedType(type)) return S_TECH_NONE;
    return g_tech_defs[type].name_id;
}

StrId Tech_EraName(int era) {
    switch (era) {
        case ERA_ANCIENT:    return S_ERA_ANCIENT;
        case ERA_CLASSICAL:  return S_ERA_CLASSICAL;
        case ERA_MEDIEVAL:   return S_ERA_MEDIEVAL;
        case ERA_RENAISSANCE:return S_ERA_RENAISSANCE;
        default:             return S_ERA_ANCIENT;
    }
}

int Tech_Era(int type) {
    if (!Tech_IsResearchedType(type)) return ERA_ANCIENT;
    return g_tech_defs[type].era;
}

int Tech_Cost(int type) {
    if (!Tech_IsResearchedType(type)) return 0;
    return g_tech_defs[type].cost;
}

int Tech_Effect(int type) {
    if (!Tech_IsResearchedType(type)) return TECH_EFFECT_NONE;
    return g_tech_defs[type].effect;
}

int Tech_EffectValue(int type) {
    if (!Tech_IsResearchedType(type)) return 0;
    return g_tech_defs[type].effect_value;
}
