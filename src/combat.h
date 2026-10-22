#ifndef CIVPSP_COMBAT_H
#define CIVPSP_COMBAT_H

#include "map.h"
#include "world.h"

typedef enum {
    COMBAT_OK = 0,
    COMBAT_ERR_INVALID = -1,
    COMBAT_ERR_FRIENDLY = -2,
    COMBAT_ERR_NOT_ADJACENT = -3,
    COMBAT_ERR_NO_MOVES = -4,
    COMBAT_ERR_CIVILIAN = -5,
    COMBAT_ERR_DEFENDED = -6,
    COMBAT_ERR_NO_CITY = -7,
    COMBAT_ERR_TREATY = -8
} CombatResultCode;

typedef struct {
    int attacker_damage;
    int defender_damage;
    int attacker_hp;
    int defender_hp;
    int attacker_died;
    int defender_died;
    int city_captured;
    int city_index;
    int city_defense_damage;
    int city_hp_damage;
} CombatResult;

/* Deterministic damage from strength difference, clamped to 5..95. */
int Combat_Damage(int attack_strength, int defense_strength);

/* Defensive strength includes forest/hills or the city/wall bonus. */
int Combat_DefenseStrength(const World *world, const Map *map, int x, int y,
                           int owner, int base_strength);

int Combat_CanAttack(const World *world, const Map *map, int attacker_index,
                     int defender_index);
int Combat_AttackUnit(World *world, Map *map, int attacker_index,
                      int defender_index, CombatResult *result);

/* V0.2 city assault: a city must be empty of garrison units before it can be
 * attacked.  Each attack drains the defense pool, then city HP; the city is
 * captured when HP reaches zero. */
int Combat_CanAttackCity(const World *world, const Map *map,
                         int attacker_index, int city_index);
int Combat_AttackCity(World *world, Map *map, int attacker_index,
                      int city_index, CombatResult *result);

/* Legacy names retained for the host tests and older call sites. */
int Combat_CanCaptureCity(const World *world, const Map *map,
                          int attacker_index, int city_index);
int Combat_CaptureCity(World *world, Map *map, int attacker_index,
                       int city_index, CombatResult *result);

#endif
