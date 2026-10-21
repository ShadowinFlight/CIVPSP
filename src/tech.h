#ifndef CIVPSP_TECH_H
#define CIVPSP_TECH_H

#include <stdint.h>

#include "gen/strings_gen.h"

/* Four tech eras. Ancient is reachable immediately; later techs chain from
 * earlier prerequisites. */
typedef enum {
    ERA_ANCIENT = 0,
    ERA_CLASSICAL,
    ERA_MEDIEVAL,
    ERA_RENAISSANCE,
    ERA_COUNT
} EraType;

/* The original 18 techs keep their historical bit positions so version-2 M7
 * saves remain readable. New M8 techs are appended after them. */
typedef enum {
    TECH_NONE = 0,
    TECH_POTTERY,
    TECH_MINING,
    TECH_MASONRY,
    TECH_THE_WHEEL,
    TECH_WRITING,
    TECH_BRONZE_WORKING,
    TECH_HORSEBACK,
    TECH_IRRIGATION,
    TECH_MATHEMATICS,
    TECH_CURRENCY,
    TECH_ENGINEERING,
    TECH_IRON_WORKING,
    TECH_CONSTRUCTION,
    TECH_EDUCATION,
    TECH_MACHINERY,
    TECH_FEUDALISM,
    TECH_PRINTING,
    TECH_GUNPOWDER,
    /* M8: 12 new techs, appended to preserve old save bits. */
    TECH_SAILING,
    TECH_PHILOSOPHY,
    TECH_LAW,
    TECH_CIVIL_SERVICE,
    TECH_GUILDS,
    TECH_THEOLOGY,
    TECH_COMPASS,
    TECH_ASTRONOMY,
    TECH_BANKING,
    TECH_SCIENTIFIC_METHOD,
    TECH_NAVIGATION,
    TECH_MILITARY_SCIENCE,
    TECH_COUNT
} TechType;

/* One small, data-driven effect per tech.  Effects are applied by the city
 * and world yield code and are intentionally simple enough to explain on the
 * Science tab. */
typedef enum {
    TECH_EFFECT_NONE = 0,
    TECH_EFFECT_FARM_FOOD,          /* +N food per worked farm */
    TECH_EFFECT_LIBRARY_SCIENCE,    /* +N science with a library */
    TECH_EFFECT_UNIVERSITY_SCIENCE, /* +N science with a university */
    TECH_EFFECT_MARKET_GOLD,        /* +N gold with a market */
    TECH_EFFECT_WORKSHOP_PROD,      /* +N production with a workshop */
    TECH_EFFECT_BARRACKS_PROD,      /* +N production with barracks */
    TECH_EFFECT_WALL_DEFENSE,       /* +N defense with walls */
    TECH_EFFECT_EMPIRE_GOLD,        /* +N gold in every city */
    TECH_EFFECT_EMPIRE_SCIENCE,     /* +N science in every city */
    TECH_EFFECT_RESEARCH_PCT        /* +N% empire science */
} TechEffectType;

typedef struct {
    StrId name_id;
    uint8_t era;
    uint8_t cost;       /* total science points to finish */
    int8_t prereq1;     /* TechType or -1 */
    int8_t prereq2;     /* TechType or -1 */
    StrId unlock_id;    /* short description of what the tech unlocks */
    uint8_t effect;     /* TechEffectType */
    uint8_t effect_value;
} TechDef;

extern const TechDef g_tech_defs[TECH_COUNT];

/* Bitmask of known techs (TECH_COUNT <= 32). */
int Tech_IsKnown(uint32_t researched, int type);
int Tech_HasPrereqs(uint32_t researched, int type);
/* Selectable = prereqs known and not yet known. */
int Tech_CanSelect(uint32_t researched, int type);
int Tech_IsResearchedType(int type);
int Tech_CountKnown(uint32_t researched);

StrId Tech_Name(int type);
StrId Tech_EraName(int era);
int Tech_Era(int type);
int Tech_Cost(int type);
int Tech_Effect(int type);
int Tech_EffectValue(int type);

#endif
