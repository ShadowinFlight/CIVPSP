#include "mapview.h"

#include <pspctrl.h>
#include <pspkernel.h>

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "combat.h"
#include "data.h"
#include "game.h"
#include "gen/strings_gen.h"
#include "gen/terrain_gen.h"
#include "improvement.h"
#include "input.h"
#include "log.h"
#include "render.h"
#include "text.h"

#define MAP_VIEW_Y       24
#define MAP_VIEW_H       (SCREEN_HEIGHT - 48)
#define CAM_MAX_X        (MAP_PIXEL_WIDTH - SCREEN_WIDTH)
#define CAM_MAX_Y        (MAP_PIXEL_HEIGHT - MAP_VIEW_H)
#define CAMERA_MARGIN_X  72.0f
#define CAMERA_MARGIN_Y  40.0f
#define CAMERA_SPEED     7.0f
#define CAMERA_FOLLOW    0.28f
#define NAV_MASK         (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)
#define MESSAGE_FRAMES   120
#define DENY_FRAMES      14

#define CITY_SPRITE      3

#define COLOR_TOP        0xFF14242Cu
#define COLOR_BOTTOM     0xFF0E1A20u
#define COLOR_GOLD       0xFFD8B25Cu
#define COLOR_TEXT       0xFFF5EEDCu
#define COLOR_MUTED      0xFF9FB0A8u
#define COLOR_PANEL      0xF20A1116u
#define COLOR_PANEL_SEL  0x882B4749u
#define COLOR_BORDER     0xFF3E5A5Cu
#define COLOR_FOG_DARK   0xFF05090Cu
#define COLOR_FOG_DIM    0x99050A0Eu
#define COLOR_CURSOR     0xFFFFE080u
#define COLOR_REACH      0xFF7ED37Eu
#define COLOR_DENY       0xFFE06050u
#define COLOR_SELECT     0xFF7ED37Eu
#define COLOR_OWNER      0xFF4A90D9u
#define COLOR_ATTACK     0xFFE06050u
#define COLOR_HP_OK      0xFF7ED37Eu
#define COLOR_HP_MID     0xFFD8B25Cu
#define COLOR_HP_LOW     0xFFE06050u

static StrId unit_str(int type) {
    switch (type) {
        case UNIT_SETTLER: return S_UNIT_SETTLER;
        case UNIT_WARRIOR: return S_UNIT_WARRIOR;
        case UNIT_SCOUT:   return S_UNIT_SCOUT;
        case UNIT_SWORDSMAN: return S_UNIT_SWORDSMAN;
        case UNIT_HORSEMAN:  return S_UNIT_HORSEMAN;
        case UNIT_WORKER:    return S_UNIT_WORKER;
        default:           return S_UNIT_WARRIOR;
    }
}

static StrId fog_str(const MapTile *tile) {
    if (!tile->explored) return S_MAP_FOG_UNEXPLORED;
    if (tile->visible) return S_MAP_FOG_VISIBLE;
    return S_MAP_FOG_EXPLORED;
}

/* Cursor info must not reveal live rival positions through the HUD or the
 * tile panel.  Player units stay listed even if a vision update has not run
 * yet. */
static int visible_unit_index_at(const Map *map, const World *world,
                                 int x, int y) {
    int index = Unit_IndexOfAt(&world->units, x, y);
    if (index < 0) return -1;
    if (!map->tiles[y][x].visible
        && world->units.units[index].owner != WORLD_CIV_PLAYER) {
        return -1;
    }
    return index;
}

/* Cities are static and remain known once their tile has been explored. */
static const City *known_city_at(const Map *map, const World *world,
                                 int x, int y) {
    if (!map->tiles[y][x].explored) return NULL;
    return City_AtConst(&world->cities, x, y);
}

static float clampf(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void clamp_camera(MapView *view) {
    view->cam_x = clampf(view->cam_x, 0.0f, (float)CAM_MAX_X);
    view->cam_y = clampf(view->cam_y, 0.0f, (float)CAM_MAX_Y);
    view->target_x = clampf(view->target_x, 0.0f, (float)CAM_MAX_X);
    view->target_y = clampf(view->target_y, 0.0f, (float)CAM_MAX_Y);
}

static float approach(float value, float target) {
    float delta = target - value;
    if (delta > 0.5f || delta < -0.5f) {
        return value + delta * CAMERA_FOLLOW;
    }
    return target;
}

static void ensure_cursor_visible(MapView *view) {
    float cx = (float)(view->cursor_x * MAP_TILE_PX);
    float cy = (float)(view->cursor_y * MAP_TILE_PX);
    float screen_x = cx - view->cam_x;
    float screen_y = cy - view->cam_y;

    if (screen_x < CAMERA_MARGIN_X) {
        view->target_x = cx - CAMERA_MARGIN_X;
    } else if (screen_x + MAP_TILE_PX > SCREEN_WIDTH - CAMERA_MARGIN_X) {
        view->target_x = cx + MAP_TILE_PX - (SCREEN_WIDTH - CAMERA_MARGIN_X);
    }
    if (screen_y < CAMERA_MARGIN_Y) {
        view->target_y = cy - CAMERA_MARGIN_Y;
    } else if (screen_y + MAP_TILE_PX > MAP_VIEW_H - CAMERA_MARGIN_Y) {
        view->target_y = cy + MAP_TILE_PX - (MAP_VIEW_H - CAMERA_MARGIN_Y);
    }
    clamp_camera(view);
}

static void center_on_cursor(MapView *view) {
    view->target_x = (float)(view->cursor_x * MAP_TILE_PX)
                   + MAP_TILE_PX * 0.5f - SCREEN_WIDTH * 0.5f;
    view->target_y = (float)(view->cursor_y * MAP_TILE_PX)
                   + MAP_TILE_PX * 0.5f - MAP_VIEW_H * 0.5f;
    clamp_camera(view);
}

static void set_message(MapView *view, const char *text) {
    snprintf(view->message, sizeof(view->message), "%s", text);
    view->message_timer = MESSAGE_FRAMES;
}

static Unit *selected_unit(World *world, const MapView *view) {
    if (view->selected < 0 || view->selected >= world->units.count) return NULL;
    Unit *unit = &world->units.units[view->selected];
    if (unit->owner != WORLD_CIV_PLAYER) return NULL;
    return unit;
}

static const Unit *selected_unit_const(const World *world, const MapView *view) {
    if (view->selected < 0 || view->selected >= world->units.count) return NULL;
    const Unit *unit = &world->units.units[view->selected];
    if (unit->owner != WORLD_CIV_PLAYER) return NULL;
    return unit;
}

static void update_reachable(MapView *view, const World *world, const Map *map) {
    view->cursor_reachable = 0;
    view->cursor_attack = 0;
    const Unit *unit = selected_unit_const(world, view);
    if (!unit) return;

    int enemy_unit = Unit_IndexOfAt(&world->units,
                                    view->cursor_x, view->cursor_y);
    if (enemy_unit >= 0
        && world->units.units[enemy_unit].owner != unit->owner
        && Combat_CanAttack(world, map, view->selected, enemy_unit)
               == COMBAT_OK) {
        view->cursor_attack = 1;
        return;
    }

    int enemy_city = City_IndexOfAt(&world->cities,
                                    view->cursor_x, view->cursor_y);
    if (enemy_city >= 0
        && world->cities.cities[enemy_city].owner != unit->owner
        && Combat_CanCaptureCity(world, map, view->selected, enemy_city)
               == COMBAT_OK) {
        view->cursor_attack = 1;
        return;
    }

    if (unit->moves_left == 0) return;
    if (unit->x == view->cursor_x && unit->y == view->cursor_y) {
        view->cursor_reachable = 1;
        return;
    }
    if (Unit_PathCost(&world->units, map, unit,
                      view->cursor_x, view->cursor_y,
                      unit->moves_left) >= 0) {
        view->cursor_reachable = 1;
    }
}

static void select_unit(MapView *view, const World *world, const Map *map,
                        int index, int center) {
    if (index >= 0 && (index >= world->units.count
                       || world->units.units[index].owner != WORLD_CIV_PLAYER)) {
        index = -1;
    }
    view->selected = index;
    view->menu_open = 0;
    view->build_open = 0;
    if (index >= 0 && index < world->units.count
        && world->units.units[index].owner == WORLD_CIV_PLAYER) {
        const Unit *unit = &world->units.units[index];
        view->cursor_x = unit->x;
        view->cursor_y = unit->y;
        if (center) center_on_cursor(view);
        else ensure_cursor_visible(view);
    }
    update_reachable(view, world, map);
}

static void advance_after_action(MapView *view, World *world, Map *map) {
    view->selected = -1;
    int next = World_FirstUnitWithMoves(world);
    if (next >= 0) {
        select_unit(view, world, map, next, 1);
    } else {
        update_reachable(view, world, map);
        set_message(view, Str(S_MSG_END_TURN));
    }
}

static int tile_adjacent(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax;
    int dy = ay > by ? ay - by : by - ay;
    return dx <= 1 && dy <= 1 && (dx != 0 || dy != 0);
}

/* An empty friendly city adjacent to a military unit can be garrisoned from
 * the action menu.  Cross on a city tile still opens city management, so this
 * is the explicit way to put an existing field unit behind the walls. */
static int garrison_city_for(const World *world, const Map *map,
                             const Unit *unit) {
    if (!world || !map || !unit) return -1;
    if (unit->owner != WORLD_CIV_PLAYER || unit->moves_left == 0) return -1;
    if (g_unit_defs[unit->type].strength <= 0) return -1;
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner != WORLD_CIV_PLAYER) continue;
        if (!tile_adjacent(unit->x, unit->y, city->x, city->y)) continue;
        if (Unit_IndexOfAt(&world->units, city->x, city->y) >= 0) continue;
        if (Unit_PathCost(&world->units, map, unit, city->x, city->y,
                          unit->moves_left) >= 0) {
            return i;
        }
    }
    return -1;
}

/* A worker can repair a damaged friendly city while standing on or next to
 * it.  City defense does not regenerate automatically. */
static int repair_city_for(const World *world, const Map *map,
                           const Unit *unit) {
    if (!world || !map || !unit || unit->type != UNIT_WORKER
        || unit->moves_left == 0) {
        return -1;
    }
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner != WORLD_CIV_PLAYER) continue;
        int dx = unit->x > city->x ? unit->x - city->x : city->x - unit->x;
        int dy = unit->y > city->y ? unit->y - city->y : city->y - unit->y;
        if (dx > 1 || dy > 1) continue;
        if (city->defense < city->defense_max) return i;
    }
    return -1;
}

enum {
    MENU_FOUND = 0,
    MENU_REPAIR,
    MENU_BUILD,
    MENU_GARRISON,
    MENU_WAIT,
    MENU_CANCEL
};

static int menu_actions(const World *world, const Map *map, const Unit *unit,
                        int actions[4], StrId labels[4]) {
    if (unit->type == UNIT_SETTLER) {
        actions[0] = MENU_FOUND; labels[0] = S_ACTION_FOUND;
        actions[1] = MENU_WAIT;  labels[1] = S_ACTION_WAIT;
        actions[2] = MENU_CANCEL; labels[2] = S_ACTION_CANCEL;
        return 3;
    }
    if (unit->type == UNIT_WORKER) {
        int n = 0;
        if (repair_city_for(world, map, unit) >= 0) {
            actions[n] = MENU_REPAIR; labels[n++] = S_ACTION_REPAIR;
        }
        actions[n] = MENU_BUILD; labels[n++] = S_ACTION_BUILD;
        actions[n] = MENU_WAIT; labels[n++] = S_ACTION_WAIT;
        actions[n] = MENU_CANCEL; labels[n++] = S_ACTION_CANCEL;
        return n;
    }
    if (garrison_city_for(world, map, unit) >= 0) {
        actions[0] = MENU_GARRISON; labels[0] = S_ACTION_GARRISON;
        actions[1] = MENU_WAIT; labels[1] = S_ACTION_WAIT;
        actions[2] = MENU_CANCEL; labels[2] = S_ACTION_CANCEL;
        return 3;
    }
    actions[0] = MENU_WAIT;
    labels[0] = S_ACTION_WAIT;
    actions[1] = MENU_CANCEL;
    labels[1] = S_ACTION_CANCEL;
    return 2;
}

static StrId improvement_error_str(int result) {
    switch (result) {
        case IMPROVEMENT_ERR_LOCKED:      return S_TECH_REQUIRES;
        case IMPROVEMENT_ERR_NOT_OWNED:   return S_MSG_NEEDS_TERRITORY;
        case IMPROVEMENT_ERR_ALREADY:     return S_MSG_ALREADY_IMPROVED;
        case IMPROVEMENT_ERR_NO_MOVES:    return S_MSG_NO_MOVES;
        case IMPROVEMENT_ERR_NOT_WORKER:  return S_MSG_SELECT_UNIT;
        default:                          return S_MSG_CANNOT_BUILD;
    }
}

static void update_build_menu(MapView *view, World *world, Map *map) {
    Unit *unit = selected_unit(world, view);
    if (!unit || !Improvement_IsWorkerUnit(unit->type)) {
        view->build_open = 0;
        return;
    }

    int count = IMPROVEMENT_COUNT - 1;
    if (view->build_selection >= count) view->build_selection = 0;
    if (Input_Repeated(PSP_CTRL_UP)) {
        view->build_selection = (view->build_selection + count - 1) % count;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        view->build_selection = (view->build_selection + 1) % count;
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE) || Input_Pressed(PSP_CTRL_TRIANGLE)) {
        view->build_open = 0;
        return;
    }
    if (!Input_Pressed(PSP_CTRL_CROSS)) return;

    int improvement = view->build_selection + 1;
    int ux = unit->x;
    int uy = unit->y;
    int result = Improvement_Build(map, &world->cities, unit,
                                   world->researched, improvement);
    view->build_open = 0;
    if (result == IMPROVEMENT_OK) {
        Audio_PlaySfx(AUDIO_SFX_BUILD);
        char msg[72];
        snprintf(msg, sizeof(msg), "%s %s", Str(S_MSG_BUILT),
                 Str(Data_ImprovementName(improvement)));
        Log_Printf("unit: worker built %s at %d,%d",
                   Str(Data_ImprovementName(improvement)), ux, uy);
        advance_after_action(view, world, map);
        set_message(view, msg);
    } else {
        Log_Printf("unit: build %s failed at %d,%d result=%d",
                   Str(Data_ImprovementName(improvement)), ux, uy, result);
        set_message(view, Str(improvement_error_str(result)));
    }
}

static StrId combat_error_str(int result) {
    switch (result) {
        case COMBAT_ERR_NO_MOVES: return S_MSG_NO_MOVES;
        case COMBAT_ERR_CIVILIAN: return S_MSG_SELECT_UNIT;
        case COMBAT_ERR_FRIENDLY: return S_MSG_ATTACK_FAILED;
        case COMBAT_ERR_DEFENDED: return S_MSG_CITY_DEFENDED;
        case COMBAT_ERR_NOT_ADJACENT: return S_MSG_ATTACK_FAILED;
        case COMBAT_ERR_TREATY: return S_DIPLO_TREATY_BLOCKS_WAR;
        default: return S_MSG_ATTACK_FAILED;
    }
}

static void attack_unit_at(MapView *view, Map *map, World *world,
                           int target_index) {
    if (!selected_unit(world, view)) return;
    if (target_index < 0 || target_index >= world->units.count) return;

    int attacker_before = view->selected;
    int target_owner = world->units.units[target_index].owner;
    int target_type = world->units.units[target_index].type;
    CombatResult result;
    int code = Combat_AttackUnit(world, map, view->selected, target_index,
                                 &result);
    if (code != COMBAT_OK) {
        set_message(view, Str(combat_error_str(code)));
        return;
    }

    World_UpdateVision(world, map);
    Audio_PlaySfx(AUDIO_SFX_COMBAT);
    char msg[96];
    if (result.city_captured && result.city_index >= 0) {
        const City *city = &world->cities.cities[result.city_index];
        snprintf(msg, sizeof(msg), "%s %s",
                 Str(S_MSG_CITY_CAPTURED),
                 Str(Data_CityName(city->name_index)));
    } else if (result.defender_died) {
        snprintf(msg, sizeof(msg), "%s %d  %s",
                 Str(S_MSG_ATTACK_HIT), result.attacker_damage,
                 Str(S_MSG_DEFENDER_KILLED));
    } else if (result.attacker_died) {
        snprintf(msg, sizeof(msg), "%s %d  %s",
                 Str(S_MSG_ATTACK_HIT), result.attacker_damage,
                 Str(S_MSG_ATTACKER_KILLED));
    } else {
        snprintf(msg, sizeof(msg), "%s %d  %s %d",
                 Str(S_MSG_ATTACK_HIT), result.attacker_damage,
                 Str(S_MSG_ATTACK_TOOK), result.defender_damage);
    }

    Log_Printf("combat: player attack %d -> %d owner=%d type=%d dmg=%d "
               "counter=%d defender_dead=%d attacker_dead=%d city=%d",
               attacker_before, target_index, target_owner, target_type,
               result.attacker_damage, result.defender_damage,
               result.defender_died, result.attacker_died,
               result.city_captured);
    advance_after_action(view, world, map);
    set_message(view, msg);
}

static void assault_city_at(MapView *view, Map *map, World *world,
                            int city_index) {
    if (!selected_unit(world, view)) return;
    const City *before = &world->cities.cities[city_index];
    int old_owner = before->owner;
    char city_name[24];
    snprintf(city_name, sizeof(city_name), "%s",
             Str(Data_CityName(before->name_index)));

    CombatResult result;
    int code = Combat_AttackCity(world, map, view->selected, city_index,
                                 &result);
    if (code != COMBAT_OK) {
        set_message(view, Str(combat_error_str(code)));
        return;
    }

    World_UpdateVision(world, map);
    Audio_PlaySfx(AUDIO_SFX_COMBAT);
    char msg[96];
    if (result.city_captured) {
        snprintf(msg, sizeof(msg), "%s %s",
                 Str(S_MSG_CITY_CAPTURED), city_name);
    } else {
        snprintf(msg, sizeof(msg), "%s  %s",
                 city_name, Str(S_MSG_ATTACK_HIT));
        /* Compact result: defense absorbed, HP damage, remaining values. */
        char detail[64];
        snprintf(detail, sizeof(detail), Str(S_MSG_CITY_ASSAULT_DETAIL),
                 result.city_defense_damage, result.city_hp_damage);
        strncat(msg, detail, sizeof(msg) - strlen(msg) - 1);
    }
    Log_Printf("combat: player assaulted %s owner=%d def_dmg=%d hp_dmg=%d "
               "captured=%d", city_name, old_owner,
               result.city_defense_damage, result.city_hp_damage,
               result.city_captured);
    advance_after_action(view, world, map);
    set_message(view, msg);
}

static void update_menu(MapView *view, World *world, Map *map) {
    Unit *unit = selected_unit(world, view);
    if (!unit) {
        view->menu_open = 0;
        return;
    }

    int actions[4];
    StrId labels[4];
    int count = menu_actions(world, map, unit, actions, labels);
    if (view->menu_selection >= count) view->menu_selection = 0;
    if (Input_Repeated(PSP_CTRL_UP)) {
        view->menu_selection = (view->menu_selection + count - 1) % count;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        view->menu_selection = (view->menu_selection + 1) % count;
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE) || Input_Pressed(PSP_CTRL_TRIANGLE)) {
        view->menu_open = 0;
        return;
    }
    if (!Input_Pressed(PSP_CTRL_CROSS)) return;

    int action = actions[view->menu_selection];
    if (action == MENU_GARRISON) {
        int garrison_city = garrison_city_for(world, map, unit);
        if (garrison_city < 0) {
            view->menu_open = 0;
            return;
        }
        const City *city = &world->cities.cities[garrison_city];
        int ux = unit->x;
        int uy = unit->y;
        int cost = Unit_MoveTo(&world->units, map, view->selected,
                               city->x, city->y);
        view->menu_open = 0;
        if (cost < 0) {
            set_message(view, Str(S_MSG_CANNOT_MOVE));
            Log_Printf("unit: garrison failed %d,%d -> city %d,%d", ux, uy,
                       city->x, city->y);
            return;
        }
        World_UpdateVision(world, map);
        char msg[96];
        snprintf(msg, sizeof(msg), "%s %s", Str(S_MSG_GARRISONED),
                 Str(Data_CityName(city->name_index)));
        Log_Printf("unit: garrisoned %s at %d,%d cost=%d moves_left=%d",
                   Str(Data_CityName(city->name_index)), city->x, city->y,
                   cost, unit->moves_left);
        view->cursor_x = city->x;
        view->cursor_y = city->y;
        update_reachable(view, world, map);
        set_message(view, msg);
        return;
    }

    if (action == MENU_REPAIR) {
        int city_index = repair_city_for(world, map, unit);
        if (city_index >= 0) {
            City *city = &world->cities.cities[city_index];
            int repaired = City_RepairDefense(city);
            unit->moves_left = 0;
            view->menu_open = 0;
            Audio_PlaySfx(AUDIO_SFX_BUILD);
            char msg[96];
            snprintf(msg, sizeof(msg), Str(S_MSG_CITY_REPAIRED), repaired);
            Log_Printf("city: worker repaired %s defense +%d now=%d/%d",
                       Str(Data_CityName(city->name_index)), repaired,
                       city->defense, city->defense_max);
            advance_after_action(view, world, map);
            set_message(view, msg);
        } else {
            view->menu_open = 0;
            set_message(view, Str(S_MSG_CITY_REPAIR_NONE));
        }
        return;
    }

    if (action == MENU_BUILD) {
        view->menu_open = 0;
        view->build_open = 1;
        view->build_selection = 0;
        Log_Printf("unit: build menu at %d,%d", unit->x, unit->y);
        return;
    }

    if (action == MENU_FOUND) {
        int ux = unit->x;
        int uy = unit->y;
        int result = World_FoundCity(world, map, view->selected);
        view->menu_open = 0;
        if (result >= 0) {
            Audio_PlaySfx(AUDIO_SFX_FOUND);
            const City *city = &world->cities.cities[result];
            snprintf(view->message, sizeof(view->message), "%s: %s",
                     Str(S_MSG_FOUNDED), Str(Data_CityName(city->name_index)));
            view->message_timer = MESSAGE_FRAMES;
            Log_Printf("city: founded %s at %d,%d cities=%d",
                       Str(Data_CityName(city->name_index)), city->x, city->y,
                       world->cities.count);
            advance_after_action(view, world, map);
        } else {
            set_message(view, result == WORLD_FOUND_TOO_CLOSE
                                  ? Str(S_MSG_TOO_CLOSE)
                                  : Str(S_MSG_CANNOT_FOUND));
            Log_Printf("city: found failed at %d,%d result=%d",
                       ux, uy, result);
        }
        return;
    }

    if (action == MENU_WAIT) {
        int ux = unit->x;
        int uy = unit->y;
        unit->moves_left = 0;
        view->menu_open = 0;
        Log_Printf("unit: wait at %d,%d", ux, uy);
        advance_after_action(view, world, map);
        return;
    }

    view->menu_open = 0;
}

void MapView_Init(MapView *view, Map *map, const World *world) {
    memset(view, 0, sizeof(*view));
    view->selected = -1;
    view->minimap_open = 0;
    view->fps = 60;
    view->fps_last_us = (uint32_t)sceKernelGetSystemTimeLow();

    int first = -1;
    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        if (unit->owner == WORLD_CIV_PLAYER
            && unit->type == UNIT_SETTLER
            && unit->moves_left > 0) {
            first = i;
            break;
        }
    }
    if (first < 0) first = World_FirstUnitWithMoves(world);

    if (first >= 0) {
        const Unit *unit = &world->units.units[first];
        view->selected = first;
        view->cursor_x = unit->x;
        view->cursor_y = unit->y;
    } else {
        view->cursor_x = map->start_x;
        view->cursor_y = map->start_y;
    }

    view->cam_x = (float)(view->cursor_x * MAP_TILE_PX)
                + MAP_TILE_PX * 0.5f - SCREEN_WIDTH * 0.5f;
    view->cam_y = (float)(view->cursor_y * MAP_TILE_PX)
                + MAP_TILE_PX * 0.5f - MAP_VIEW_H * 0.5f;
    view->target_x = view->cam_x;
    view->target_y = view->cam_y;
    clamp_camera(view);
    update_reachable(view, world, map);
}

void MapView_Restore(MapView *view, Map *map, const World *world,
                     int cursor_x, int cursor_y, int selected,
                     float cam_x, float cam_y) {
    MapView_Init(view, map, world);
    if (cursor_x >= 0 && cursor_x < MAP_WIDTH
        && cursor_y >= 0 && cursor_y < MAP_HEIGHT) {
        view->cursor_x = cursor_x;
        view->cursor_y = cursor_y;
    }
    if (selected >= 0 && selected < world->units.count) {
        view->selected = selected;
    } else {
        view->selected = -1;
    }
    view->cam_x = cam_x;
    view->cam_y = cam_y;
    view->target_x = cam_x;
    view->target_y = cam_y;
    clamp_camera(view);
    update_reachable(view, world, map);
}

int MapView_Update(MapView *view, Map *map, World *world) {
    uint32_t now = (uint32_t)sceKernelGetSystemTimeLow();
    view->fps_frames++;
    uint32_t elapsed = now - view->fps_last_us;
    if (elapsed >= 1000000u) {
        view->fps = (int)(((uint32_t)view->fps_frames * 1000000u) / elapsed);
        view->fps_frames = 0;
        view->fps_last_us = now;
    }
    if (view->message_timer > 0) view->message_timer--;
    if (view->deny_timer > 0) view->deny_timer--;

    if (view->build_open) {
        update_build_menu(view, world, map);
        view->cam_x = approach(view->cam_x, view->target_x);
        view->cam_y = approach(view->cam_y, view->target_y);
        clamp_camera(view);
        return 0;
    }
    if (view->menu_open) {
        update_menu(view, world, map);
        view->cam_x = approach(view->cam_x, view->target_x);
        view->cam_y = approach(view->cam_y, view->target_y);
        clamp_camera(view);
        return 0;
    }

    if (Input_Pressed(PSP_CTRL_LTRIGGER)) {
        view->minimap_open = !view->minimap_open;
        Audio_PlaySfx(AUDIO_SFX_SELECT);
        Log_Printf("map: minimap %s", view->minimap_open ? "open" : "closed");
    }

    int dx = 0;
    int dy = 0;
    if (Input_Repeated(PSP_CTRL_RIGHT)) dx += 1;
    if (Input_Repeated(PSP_CTRL_LEFT)) dx -= 1;
    if (Input_Repeated(PSP_CTRL_DOWN)) dy += 1;
    if (Input_Repeated(PSP_CTRL_UP)) dy -= 1;
    if (dx != 0 || dy != 0) {
        int nx = view->cursor_x + dx;
        int ny = view->cursor_y + dy;
        if (nx < 0) nx = 0;
        if (nx >= MAP_WIDTH) nx = MAP_WIDTH - 1;
        if (ny < 0) ny = 0;
        if (ny >= MAP_HEIGHT) ny = MAP_HEIGHT - 1;
        if (nx != view->cursor_x || ny != view->cursor_y) {
            view->cursor_x = nx;
            view->cursor_y = ny;
            ensure_cursor_visible(view);
            update_reachable(view, world, map);
        }
    }

    if (Input_Pressed(PSP_CTRL_CROSS)) {
        int city_index = City_IndexOfAt(&world->cities,
                                        view->cursor_x, view->cursor_y);
        int unit_index = Unit_IndexOfAt(&world->units,
                                        view->cursor_x, view->cursor_y);
        Unit *unit = selected_unit(world, view);

        /* Cross on one of the player's cities always opens city management,
         * matching the M4.1 rule that the city screen wins over garrison
         * selection. */
        if (city_index >= 0
            && world->cities.cities[city_index].owner == WORLD_CIV_PLAYER) {
            view->open_city = city_index + 1;
            Log_Printf("city: open requested %d at %d,%d", city_index,
                       view->cursor_x, view->cursor_y);
            return 0;
        }

        if (unit) {
            if (unit_index >= 0) {
                const Unit *target = &world->units.units[unit_index];
                if (target->owner != WORLD_CIV_PLAYER) {
                    if (Combat_CanAttack(world, map, view->selected,
                                         unit_index) == COMBAT_OK) {
                        attack_unit_at(view, map, world, unit_index);
                    } else {
                        set_message(view, Str(S_MSG_ATTACK_FAILED));
                    }
                    return 0;
                }
                if (unit_index != view->selected) {
                    select_unit(view, world, map, unit_index, 0);
                    Log_Printf("unit: selected type=%d at %d,%d moves=%d",
                               target->type, target->x, target->y,
                               target->moves_left);
                }
                return 0;
            }

            if (city_index >= 0) {
                int code = Combat_CanAttackCity(world, map, view->selected,
                                                city_index);
                if (code == COMBAT_OK) {
                    assault_city_at(view, map, world, city_index);
                } else {
                    set_message(view, Str(combat_error_str(code)));
                }
                return 0;
            }

            if (unit->moves_left == 0) {
                set_message(view, Str(S_MSG_NO_MOVES));
            } else if (unit->x != view->cursor_x
                       || unit->y != view->cursor_y) {
                int sx = unit->x;
                int sy = unit->y;
                int moves = unit->moves_left;
                int cost = Unit_MoveTo(&world->units, map, view->selected,
                                       view->cursor_x, view->cursor_y);
                if (cost < 0) {
                    set_message(view, Str(S_MSG_CANNOT_MOVE));
                    view->deny_timer = DENY_FRAMES;
                    Log_Printf("unit: move failed %d,%d -> %d,%d moves=%d",
                               sx, sy, view->cursor_x, view->cursor_y, moves);
                } else {
                    World_UpdateVision(world, map);
                    ensure_cursor_visible(view);
                    update_reachable(view, world, map);
                    Log_Printf("unit: moved to %d,%d cost=%d moves_left=%d",
                               unit->x, unit->y, cost, unit->moves_left);
                }
            }
            return 0;
        }

        if (unit_index >= 0) {
            const Unit *target = &world->units.units[unit_index];
            if (target->owner == WORLD_CIV_PLAYER) {
                select_unit(view, world, map, unit_index, 0);
                Log_Printf("unit: selected type=%d at %d,%d moves=%d",
                           target->type, target->x, target->y,
                           target->moves_left);
            } else {
                set_message(view, Str(S_MSG_ENEMY_UNIT));
            }
        } else if (city_index >= 0) {
            set_message(view, Str(S_MSG_ENEMY_CITY));
        }
    }

    if (Input_Pressed(PSP_CTRL_TRIANGLE)) {
        /* Triangle prefers the unit menu so a worker or garrison standing on
         * a city tile can still act.  Cross remains the city-open button. */
        int unit_index = Unit_IndexOfAt(&world->units,
                                        view->cursor_x, view->cursor_y);
        if (unit_index >= 0
            && world->units.units[unit_index].owner == WORLD_CIV_PLAYER) {
            select_unit(view, world, map, unit_index, 0);
            view->menu_open = 1;
            view->menu_selection = 0;
            Log_Printf("unit: actions at %d,%d", view->cursor_x,
                       view->cursor_y);
        } else if (unit_index >= 0) {
            set_message(view, Str(S_MSG_ENEMY_UNIT));
        } else if (view->selected >= 0 && selected_unit(world, view)) {
            view->menu_open = 1;
            view->menu_selection = 0;
        } else {
            int city_index = City_IndexOfAt(&world->cities,
                                            view->cursor_x, view->cursor_y);
            if (city_index >= 0
                && world->cities.cities[city_index].owner == WORLD_CIV_PLAYER) {
                view->open_city = city_index + 1;
                Log_Printf("city: open requested %d at %d,%d", city_index,
                           view->cursor_x, view->cursor_y);
                return 0;
            }
            if (city_index >= 0) {
                set_message(view, Str(S_MSG_ENEMY_CITY));
            } else {
                set_message(view, Str(S_MSG_SELECT_UNIT));
            }
        }
    }

    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        if (view->info_open) {
            view->info_open = 0;
        } else if (view->selected >= 0) {
            view->selected = -1;
            update_reachable(view, world, map);
            Log_Printf("unit: deselected");
        } else {
            return 1;
        }
    }

    if (Input_Pressed(PSP_CTRL_SQUARE)) {
        view->info_open = !view->info_open;
    }

    if (Input_Pressed(PSP_CTRL_RTRIGGER)) {
        int next = World_NextUnitWithMoves(world, view->selected);
        if (next >= 0) {
            select_unit(view, world, map, next, 1);
            Log_Printf("unit: next with moves -> %d at %d,%d", next,
                       world->units.units[next].x,
                       world->units.units[next].y);
        } else {
            set_message(view, Str(S_MSG_NO_MOVES));
        }
    }

    if (Input_Pressed(PSP_CTRL_SELECT)) {
        int keep = view->selected;
        uint32_t ai_start = (uint32_t)sceKernelGetSystemTimeLow();
        World_EndTurn(world, map);
        view->turn_ended = 1;
        uint32_t ai_elapsed = (uint32_t)sceKernelGetSystemTimeLow() - ai_start;

        if (keep >= 0 && keep < world->units.count
            && world->units.units[keep].owner == WORLD_CIV_PLAYER) {
            view->selected = keep;
            update_reachable(view, world, map);
        } else {
            int first = World_FirstUnitWithMoves(world);
            if (first >= 0) {
                select_unit(view, world, map, first, 1);
            } else {
                view->selected = -1;
                update_reachable(view, world, map);
            }
        }

        if (world->notice[0] != '\0'
            && world->notice_turn == world->turn) {
            snprintf(view->message, sizeof(view->message), "%s %d  %.60s",
                     Str(S_MAP_TURN), world->turn, world->notice);
        } else {
            snprintf(view->message, sizeof(view->message), "%s %d",
                     Str(S_MAP_TURN), world->turn);
        }
        view->message_timer = MESSAGE_FRAMES;
        Log_Printf("turn: %d units=%d cities=%d player_units=%d battles=%d "
                   "captures=%d ai_us=%u notice=%s",
                   world->turn, world->units.count, world->cities.count,
                   World_CountUnits(world, WORLD_CIV_PLAYER), world->battles,
                   world->cities_captured, ai_elapsed, world->notice);
    }

    if (Input_Pressed(PSP_CTRL_START)) {
        view->open_diplomacy = 1;
    }

    int dpad_held = Input_Held(NAV_MASK);
    int ax = g_input.analog_x;
    int ay = g_input.analog_y;
    if (!dpad_held && (ax != 0 || ay != 0)) {
        view->cam_x += (float)ax * (CAMERA_SPEED / 127.0f);
        view->cam_y += (float)ay * (CAMERA_SPEED / 127.0f);
        clamp_camera(view);
        view->target_x = view->cam_x;
        view->target_y = view->cam_y;

        /* Keep the selection square on the tile under the camera center while
         * the analog stick pans.  This makes the cursor follow the view
         * instead of leaving it behind at the screen edge. */
        int center_x = (int)((view->cam_x + SCREEN_WIDTH * 0.5f)
                             / (float)MAP_TILE_PX);
        int center_y = (int)((view->cam_y + MAP_VIEW_H * 0.5f)
                             / (float)MAP_TILE_PX);
        if (center_x < 0) center_x = 0;
        if (center_x >= MAP_WIDTH) center_x = MAP_WIDTH - 1;
        if (center_y < 0) center_y = 0;
        if (center_y >= MAP_HEIGHT) center_y = MAP_HEIGHT - 1;
        if (center_x != view->cursor_x || center_y != view->cursor_y) {
            view->cursor_x = center_x;
            view->cursor_y = center_y;
            update_reachable(view, world, map);
        }
    }

    view->cam_x = approach(view->cam_x, view->target_x);
    view->cam_y = approach(view->cam_y, view->target_y);
    clamp_camera(view);
    return 0;
}

static void draw_improvement_marker(float sx, float sy, int improvement) {
    switch (improvement) {
        case IMPROVEMENT_FARM:
            /* Golden crop rows on dark soil, visible against grassland and
             * plains alike. */
            Render_DrawRect(sx + 2, sy + 14, 20, 9, 0xFF5A4520u);
            Render_DrawRect(sx + 3, sy + 15, 18, 2, 0xFFE2C15Au);
            Render_DrawRect(sx + 3, sy + 18, 18, 2, 0xFFF0D97Au);
            Render_DrawRect(sx + 3, sy + 21, 18, 2, 0xFFC29B3Au);
            break;
        case IMPROVEMENT_MINE:
            Render_DrawRect(sx + 3, sy + 14, 9, 8, 0xFF55524Cu);
            Render_DrawRect(sx + 5, sy + 16, 5, 5, 0xFF1E1C19u);
            Render_DrawRect(sx + 12, sy + 19, 7, 3, 0xFF9A968Eu);
            break;
        case IMPROVEMENT_PASTURE:
            Render_DrawRect(sx + 3, sy + 15, 18, 2, 0xFFB9A06Au);
            Render_DrawRect(sx + 4, sy + 17, 2, 6, 0xFFE3D3A0u);
            Render_DrawRect(sx + 11, sy + 17, 2, 6, 0xFFE3D3A0u);
            Render_DrawRect(sx + 18, sy + 17, 2, 6, 0xFFE3D3A0u);
            break;
        case IMPROVEMENT_QUARRY:
            Render_DrawRect(sx + 3, sy + 16, 9, 6, 0xFFB8B2A6u);
            Render_DrawRect(sx + 12, sy + 13, 7, 7, 0xFF948E82u);
            Render_DrawRect(sx + 4, sy + 20, 15, 2, 0xFF6E685Eu);
            break;
        case IMPROVEMENT_ROAD:
            Render_DrawRect(sx, sy + 11, MAP_TILE_PX, 4, 0xFF6F5B3Eu);
            Render_DrawRect(sx, sy + 12, MAP_TILE_PX, 2, 0xFFD0B47Eu);
            break;
        default:
            break;
    }
}

static void draw_map_tiles(const MapView *view, const Map *map) {
    int first_col = (int)(view->cam_x / (float)MAP_TILE_PX);
    int first_row = (int)(view->cam_y / (float)MAP_TILE_PX);
    int last_col = (int)((view->cam_x + SCREEN_WIDTH) / (float)MAP_TILE_PX);
    int last_row = (int)((view->cam_y + MAP_VIEW_H) / (float)MAP_TILE_PX);

    if (first_col < 0) first_col = 0;
    if (first_row < 0) first_row = 0;
    if (last_col >= MAP_WIDTH) last_col = MAP_WIDTH - 1;
    if (last_row >= MAP_HEIGHT) last_row = MAP_HEIGHT - 1;

    for (int y = first_row; y <= last_row; y++) {
        for (int x = first_col; x <= last_col; x++) {
            const MapTile *tile = &map->tiles[y][x];
            float sx = (float)(x * MAP_TILE_PX) - view->cam_x;
            float sy = (float)MAP_VIEW_Y + (float)(y * MAP_TILE_PX) - view->cam_y;

            if (!tile->explored) {
                Render_DrawRect(sx, sy, MAP_TILE_PX, MAP_TILE_PX, COLOR_FOG_DARK);
                continue;
            }

            int terrain = (int)tile->terrain;
            int col = terrain % TERRAIN_ATLAS_COLS;
            int row = terrain / TERRAIN_ATLAS_COLS;
            float u0 = (float)(col * TERRAIN_ATLAS_TILE_W);
            float v0 = (float)(row * TERRAIN_ATLAS_TILE_H);
            Render_DrawSprite(sx, sy, MAP_TILE_PX, MAP_TILE_PX,
                              u0, v0,
                              u0 + TERRAIN_ATLAS_TILE_W,
                              v0 + TERRAIN_ATLAS_TILE_H,
                              0xFFFFFFFFu);

            if (tile->resource != RESOURCE_NONE) {
                int rcol = (int)tile->resource - 1;
                float ru = (float)(rcol * TERRAIN_ATLAS_TILE_W);
                float rv = (float)(TERRAIN_ATLAS_RESOURCE_ROW * TERRAIN_ATLAS_TILE_H);
                Render_DrawSprite(sx, sy, MAP_TILE_PX, MAP_TILE_PX,
                                  ru, rv,
                                  ru + TERRAIN_ATLAS_TILE_W,
                                  rv + TERRAIN_ATLAS_TILE_H,
                                  0xFFFFFFFFu);
            }

            draw_improvement_marker(sx, sy, tile->improvement);

            if (!tile->visible) {
                Render_DrawRect(sx, sy, MAP_TILE_PX, MAP_TILE_PX, COLOR_FOG_DIM);
            }
        }
    }
}

static void draw_city_borders(const MapView *view, const Map *map,
                             const World *world) {
    for (int c = 0; c < world->cities.count; c++) {
        const City *city = &world->cities.cities[c];
        for (int i = 0; i < CITY_WORK_TILES; i++) {
            if (!City_OwnsIndex(city, i)) continue;
            int x = 0;
            int y = 0;
            City_TileCoord(city, i, &x, &y);
            if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) continue;
            const MapTile *tile = &map->tiles[y][x];
            if (!tile->explored) continue;

            float sx = (float)(x * MAP_TILE_PX) - view->cam_x;
            float sy = (float)MAP_VIEW_Y + (float)(y * MAP_TILE_PX) - view->cam_y;
            if (sx <= -MAP_TILE_PX || sx >= SCREEN_WIDTH) continue;
            if (sy <= MAP_VIEW_Y - MAP_TILE_PX || sy >= MAP_VIEW_Y + MAP_VIEW_H) {
                continue;
            }
            unsigned int owner_color = World_CivColor(city->owner);
            unsigned int color = (owner_color & 0x00FFFFFFu)
                               | (tile->visible ? 0xAA000000u : 0x55000000u);
            if (!City_OwnsTile(city, x, y - 1)) {
                Render_DrawRect(sx, sy, MAP_TILE_PX, 1, color);
            }
            if (!City_OwnsTile(city, x, y + 1)) {
                Render_DrawRect(sx, sy + MAP_TILE_PX - 1, MAP_TILE_PX, 1, color);
            }
            if (!City_OwnsTile(city, x - 1, y)) {
                Render_DrawRect(sx, sy, 1, MAP_TILE_PX, color);
            }
            if (!City_OwnsTile(city, x + 1, y)) {
                Render_DrawRect(sx + MAP_TILE_PX - 1, sy, 1, MAP_TILE_PX, color);
            }
        }
    }
}

static void draw_atlas_sprite(const MapView *view, int map_x, int map_y,
                              int sprite_col, int owner) {
    float sx = (float)(map_x * MAP_TILE_PX) - view->cam_x;
    float sy = (float)MAP_VIEW_Y + (float)(map_y * MAP_TILE_PX) - view->cam_y;
    if (sx <= -MAP_TILE_PX || sx >= SCREEN_WIDTH) return;
    if (sy <= MAP_VIEW_Y - MAP_TILE_PX || sy >= MAP_VIEW_Y + MAP_VIEW_H) return;

    if (owner < 0 || owner >= TERRAIN_ATLAS_UNIT_ROWS) {
        owner = TERRAIN_ATLAS_UNIT_ROWS - 1;
    }
    float u0 = (float)(sprite_col * TERRAIN_ATLAS_TILE_W);
    float v0 = (float)((TERRAIN_ATLAS_UNIT_ROW + owner)
                       * TERRAIN_ATLAS_TILE_H);
    Render_DrawSprite(sx, sy, MAP_TILE_PX, MAP_TILE_PX,
                      u0, v0,
                      u0 + TERRAIN_ATLAS_TILE_W,
                      v0 + TERRAIN_ATLAS_TILE_H,
                      0xFFFFFFFFu);
}

static void draw_entities(const MapView *view, const Map *map,
                          const World *world) {
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        const MapTile *tile = &map->tiles[city->y][city->x];
        if (!tile->visible) continue;
        draw_atlas_sprite(view, city->x, city->y, CITY_SPRITE, city->owner);
        float sx = (float)(city->x * MAP_TILE_PX) - view->cam_x;
        float sy = (float)MAP_VIEW_Y + (float)(city->y * MAP_TILE_PX) - view->cam_y;
        Render_DrawRect(sx + 2, sy + MAP_TILE_PX - 4, MAP_TILE_PX - 4, 3,
                        World_CivColor(city->owner));
    }

    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        const MapTile *tile = &map->tiles[unit->y][unit->x];
        if (!tile->visible) continue;

        int sprite = 1;
        if (unit->type == UNIT_SETTLER) sprite = 0;
        else if (unit->type == UNIT_SCOUT) sprite = 2;
        else if (unit->type == UNIT_WORKER) sprite = 4;

        draw_atlas_sprite(view, unit->x, unit->y, sprite, unit->owner);
        float sx = (float)(unit->x * MAP_TILE_PX) - view->cam_x;
        float sy = (float)MAP_VIEW_Y + (float)(unit->y * MAP_TILE_PX) - view->cam_y;
        Render_DrawRect(sx + 2, sy + MAP_TILE_PX - 4, MAP_TILE_PX - 4, 3,
                        World_CivColor(unit->owner));

        int hp = unit->hp;
        if (hp <= 0 || hp > UNIT_HP_MAX) hp = UNIT_HP_MAX;
        if (hp < UNIT_HP_MAX || unit->owner != WORLD_CIV_PLAYER
            || view->selected == i) {
            unsigned int hp_color = hp > 60 ? COLOR_HP_OK
                                  : hp > 30 ? COLOR_HP_MID : COLOR_HP_LOW;
            Render_DrawRect(sx + 2, sy + MAP_TILE_PX - 8, MAP_TILE_PX - 4, 3,
                            0xFF0A1116u);
            float fill = (float)(MAP_TILE_PX - 4) * (float)hp
                       / (float)UNIT_HP_MAX;
            Render_DrawRect(sx + 2, sy + MAP_TILE_PX - 8, fill, 3, hp_color);
        }

        if (view->selected == i) {
            Render_DrawRect(sx + 2, sy + MAP_TILE_PX - 3, MAP_TILE_PX - 4, 2,
                            COLOR_SELECT);
            Render_DrawRect(sx + 1, sy + 1, 7, 2, COLOR_SELECT);
            Render_DrawRect(sx + MAP_TILE_PX - 8, sy + 1, 7, 2, COLOR_SELECT);
        }
    }
}

static void draw_cursor(const MapView *view, const World *world) {
    float x = (float)(view->cursor_x * MAP_TILE_PX) - view->cam_x;
    float y = (float)MAP_VIEW_Y + (float)(view->cursor_y * MAP_TILE_PX) - view->cam_y;

    unsigned int color = COLOR_CURSOR;
    if (view->deny_timer > 0) {
        color = COLOR_DENY;
    } else if (view->cursor_attack && view->selected >= 0) {
        color = COLOR_ATTACK;
    } else if (view->cursor_reachable && view->selected >= 0) {
        const Unit *unit = selected_unit_const(world, view);
        if (unit && (unit->x != view->cursor_x || unit->y != view->cursor_y)) {
            color = COLOR_REACH;
        }
    }

    Render_DrawRect(x, y, MAP_TILE_PX, 2, color);
    Render_DrawRect(x, y + MAP_TILE_PX - 2, MAP_TILE_PX, 2, color);
    Render_DrawRect(x, y, 2, MAP_TILE_PX, color);
    Render_DrawRect(x + MAP_TILE_PX - 2, y, 2, MAP_TILE_PX, color);
}

static void draw_hud(const MapView *view, const Map *map, const World *world) {
    const MapTile *tile = &map->tiles[view->cursor_y][view->cursor_x];
    char buf[96];

    Render_DrawRect(0, 0, SCREEN_WIDTH, 24, COLOR_TOP);
    Render_DrawRect(0, 23, SCREEN_WIDTH, 1, COLOR_GOLD);
    Text_Draw(Str(S_TITLE), 8, 5, 1, COLOR_GOLD);
    Text_Drawf(70, 5, 1, COLOR_TEXT, "%s %d,%d",
               Str(S_MAP_TILE), view->cursor_x, view->cursor_y);
    Text_Draw(Str(Data_TerrainName(tile->terrain)), 154, 5, 1, COLOR_TEXT);
    Text_Draw(Str(Data_ResourceName(tile->resource)), 230, 5, 1, COLOR_MUTED);
    Text_Drawf(268, 5, 1, COLOR_TEXT, "%s %d",
               Str(S_MAP_TURN), world->turn);
    Text_Drawf(342, 5, 1, COLOR_GOLD, "%s %d",
               Str(S_MAP_GOLD), (int)world->treasury);
    Text_Drawf(416, 5, 1, COLOR_TEXT, "%s %d",
               Str(S_MAP_SCI), (int)world->science);

    Render_DrawRect(0, SCREEN_HEIGHT - 24, SCREEN_WIDTH, 24, COLOR_BOTTOM);
    Render_DrawRect(0, SCREEN_HEIGHT - 24, SCREEN_WIDTH, 1, COLOR_GOLD);

    int unit_index = visible_unit_index_at(map, world,
                                           view->cursor_x, view->cursor_y);
    const City *city = known_city_at(map, world,
                                     view->cursor_x, view->cursor_y);
    const Unit *selected = selected_unit_const(world, view);

    if (unit_index >= 0) {
        const Unit *unit = &world->units.units[unit_index];
        snprintf(buf, sizeof(buf), "%s%s %s %d/%d  %s %d  %s",
                 view->selected == unit_index ? "> " : "",
                 Str(unit_str(unit->type)), Str(S_MAP_MOVES),
                 unit->moves_left, Unit_TypeMovePoints(unit->type),
                 Str(S_MAP_HP), unit->hp,
                 Str(World_CivNameId(unit->owner)));
        Text_Draw(buf, 8, SCREEN_HEIGHT - 19, 1,
                  view->selected == unit_index
                      ? COLOR_GOLD : World_CivColor(unit->owner));
    } else if (city) {
        snprintf(buf, sizeof(buf), "%s  %s %d  %s %s",
                 Str(Data_CityName(city->name_index)), Str(S_MAP_POP),
                 city->population, Str(S_MAP_OWNER),
                 Str(World_CivNameId(city->owner)));
        Text_Draw(buf, 8, SCREEN_HEIGHT - 19, 1,
                  World_CivColor(city->owner));
    } else if (selected) {
        snprintf(buf, sizeof(buf), "%s %s %d/%d  %s %d", Str(unit_str(selected->type)),
                 Str(S_MAP_MOVES), selected->moves_left,
                 Unit_TypeMovePoints(selected->type), Str(S_MAP_HP),
                 selected->hp);
        Text_Draw(buf, 8, SCREEN_HEIGHT - 19, 1, COLOR_GOLD);
    } else {
        Text_Draw(Str(S_MAP_CONTROLS), 8, SCREEN_HEIGHT - 19, 1, COLOR_MUTED);
    }

    if (view->turn_limit > 0) {
        snprintf(buf, sizeof(buf), "%s %d/%d", Str(S_MAP_TURN),
                 world->turn, view->turn_limit);
        Text_DrawRight(buf, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 19, 1,
                       COLOR_GOLD);
    } else {
        Text_DrawRight(Str(S_NEW_GAME_ENDLESS), SCREEN_WIDTH - 8,
                       SCREEN_HEIGHT - 19, 1, COLOR_GOLD);
    }
}

static void draw_info_panel(const MapView *view, const Map *map,
                            const World *world) {
    const MapTile *tile = &map->tiles[view->cursor_y][view->cursor_x];
    int unit_index = visible_unit_index_at(map, world,
                                           view->cursor_x, view->cursor_y);
    const Unit *unit = unit_index >= 0 ? &world->units.units[unit_index]
                                       : NULL;
    const City *city = known_city_at(map, world,
                                     view->cursor_x, view->cursor_y);
    const float x = 12.0f;
    const float y = 36.0f;
    const float w = 208.0f;
    float h = 121.0f;
    if (unit) h += 54.0f;
    if (city) h += 54.0f;

    Render_DrawRect(x, y, w, h, COLOR_PANEL);
    Render_DrawRect(x, y, w, 1, COLOR_GOLD);
    Render_DrawRect(x, y + h - 1, w, 1, COLOR_GOLD);
    Render_DrawRect(x, y, 1, h, COLOR_BORDER);
    Render_DrawRect(x + w - 1, y, 1, h, COLOR_BORDER);

    Text_Draw(Str(S_MAP_INFO), x + 10, y + 7, 1, COLOR_GOLD);
    Render_DrawRect(x + 10, y + 24, w - 20, 1, COLOR_BORDER);
    Text_Drawf(x + 10, y + 31, 1, COLOR_TEXT, "%s %d,%d",
               Str(S_MAP_TILE), view->cursor_x, view->cursor_y);
    Text_Draw(Str(Data_TerrainName(tile->terrain)), x + 10, y + 48, 1, COLOR_TEXT);
    Text_Drawf(x + 10, y + 65, 1, COLOR_TEXT, "%s %s",
               Str(S_MAP_RESOURCE), Str(Data_ResourceName(tile->resource)));
    Text_Drawf(x + 10, y + 82, 1, COLOR_TEXT, "%s %s",
               Str(S_MAP_IMPROVEMENT), Str(Data_ImprovementName(tile->improvement)));
    Text_Drawf(x + 10, y + 99, 1, COLOR_TEXT, "%s %s",
               Str(S_MAP_FOG), Str(fog_str(tile)));

    float line = y + 116;
    if (unit) {
        int hp = unit->hp;
        if (hp <= 0 || hp > UNIT_HP_MAX) hp = UNIT_HP_MAX;
        Text_Drawf(x + 10, line, 1, World_CivColor(unit->owner), "%s %s",
                   Str(S_MAP_UNIT), Str(unit_str(unit->type)));
        Text_Drawf(x + 10, line + 18, 1, COLOR_TEXT, "%s %s  %s %d/%d",
                   Str(S_MAP_OWNER), Str(World_CivNameId(unit->owner)),
                   Str(S_MAP_HP), hp, UNIT_HP_MAX);
        Text_Drawf(x + 10, line + 36, 1, COLOR_MUTED, "%s %d/%d",
                   Str(S_MAP_MOVES), unit->moves_left,
                   Unit_TypeMovePoints(unit->type));
        line += 54.0f;
    }
    if (city) {
        int hp = city->hp > 0 ? city->hp : CITY_HP_MAX;
        int defense_max = city->defense_max > 0
                              ? city->defense_max : City_DefenseMax(city);
        int defense = city->defense_max > 0 ? city->defense : defense_max;
        Text_Drawf(x + 10, line, 1, World_CivColor(city->owner), "%s %s",
                   Str(S_MAP_CITY), Str(Data_CityName(city->name_index)));
        Text_Drawf(x + 10, line + 18, 1, COLOR_MUTED, "%s %s  %s %d",
                   Str(S_MAP_OWNER), Str(World_CivNameId(city->owner)),
                   Str(S_MAP_POP), city->population);
        Text_Drawf(x + 10, line + 36, 1, COLOR_TEXT,
                   Str(S_CITY_COMBAT_STATUS), hp, CITY_HP_MAX,
                   defense, defense_max);
    }
}

/* The minimap is built at one texel per map tile (64x48, padded to 64x64)
 * and drawn as a single 5551 quad.  Drawing 3072 individual rectangles would
 * exceed the renderer's rect budget and add too many GU vertices per frame. */
#define MINIMAP_TEX_W 64
#define MINIMAP_TEX_H 64
#define MINIMAP_W 114
#define MINIMAP_H 86
#define MINIMAP_X (SCREEN_WIDTH - MINIMAP_W - 8)
#define MINIMAP_Y (SCREEN_HEIGHT - 24 - MINIMAP_H - 6)
#define MINIMAP_MAP_X (MINIMAP_X + 1)
#define MINIMAP_MAP_Y (MINIMAP_Y + 1)
#define MINIMAP_MAP_W 112
#define MINIMAP_MAP_H 84
#define MINIMAP_BUTTON_W 96
#define MINIMAP_BUTTON_H 20
#define MINIMAP_BUTTON_X (SCREEN_WIDTH - MINIMAP_BUTTON_W - 8)
#define MINIMAP_BUTTON_Y (SCREEN_HEIGHT - 24 - MINIMAP_BUTTON_H - 6)

static unsigned short minimap_pixels[MINIMAP_TEX_W * MINIMAP_TEX_H]
    __attribute__((aligned(16)));

static unsigned short minimap_rgb(int r, int g, int b) {
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (unsigned short)(0x8000u | ((b >> 3) << 10)
                            | ((g >> 3) << 5) | (r >> 3));
}

static unsigned short minimap_terrain_color(int terrain, int visible) {
    int r = 40, g = 70, b = 70;
    switch (terrain) {
        case TERRAIN_GRASSLAND: r = 86; g = 132; b = 70; break;
        case TERRAIN_PLAINS:    r = 166; g = 152; b = 88; break;
        case TERRAIN_FOREST:    r = 42; g = 94; b = 54; break;
        case TERRAIN_HILLS:     r = 122; g = 126; b = 82; break;
        case TERRAIN_MOUNTAIN:  r = 118; g = 118; b = 112; break;
        case TERRAIN_DESERT:    r = 190; g = 170; b = 96; break;
        case TERRAIN_TUNDRA:    r = 158; g = 168; b = 158; break;
        case TERRAIN_COAST:     r = 58; g = 120; b = 150; break;
        case TERRAIN_OCEAN:     r = 24; g = 54; b = 82; break;
        default: break;
    }
    if (!visible) {
        r = r * 55 / 100;
        g = g * 55 / 100;
        b = b * 55 / 100;
    }
    return minimap_rgb(r, g, b);
}

static void minimap_put(int x, int y, unsigned short color) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return;
    minimap_pixels[y * MINIMAP_TEX_W + x] = color;
}

static void draw_minimap_button(void) {
    Render_DrawRect(MINIMAP_BUTTON_X, MINIMAP_BUTTON_Y,
                    MINIMAP_BUTTON_W, MINIMAP_BUTTON_H, COLOR_PANEL);
    Render_DrawRect(MINIMAP_BUTTON_X, MINIMAP_BUTTON_Y,
                    MINIMAP_BUTTON_W, 1, COLOR_GOLD);
    Render_DrawRect(MINIMAP_BUTTON_X, MINIMAP_BUTTON_Y + MINIMAP_BUTTON_H - 1,
                    MINIMAP_BUTTON_W, 1, COLOR_GOLD);
    Render_DrawRect(MINIMAP_BUTTON_X, MINIMAP_BUTTON_Y,
                    1, MINIMAP_BUTTON_H, COLOR_BORDER);
    Render_DrawRect(MINIMAP_BUTTON_X + MINIMAP_BUTTON_W - 1, MINIMAP_BUTTON_Y,
                    1, MINIMAP_BUTTON_H, COLOR_BORDER);
    Text_Drawf(MINIMAP_BUTTON_X + 8, MINIMAP_BUTTON_Y + 2, 1, COLOR_GOLD,
               "%s L", Str(S_MINIMAP_TITLE));
}

static void draw_minimap(const MapView *view, const Map *map,
                         const World *world) {
    if (!view->minimap_open) {
        draw_minimap_button();
        return;
    }

    memset(minimap_pixels, 0, sizeof(minimap_pixels));
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const MapTile *tile = &map->tiles[y][x];
            if (!tile->explored) continue;
            minimap_put(x, y,
                        minimap_terrain_color((int)tile->terrain,
                                              tile->visible));
        }
    }

    /* Cities are visible once their tile has been explored; units only while
     * they are currently in sight. */
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (!map->tiles[city->y][city->x].explored) continue;
        unsigned int c = World_CivColor(city->owner);
        unsigned short color = minimap_rgb((c >> 16) & 0xFF,
                                           (c >> 8) & 0xFF, c & 0xFF);
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                minimap_put(city->x + dx, city->y + dy, color);
            }
        }
    }
    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        const MapTile *tile = &map->tiles[unit->y][unit->x];
        if (!tile->visible) continue;
        unsigned int c = World_CivColor(unit->owner);
        minimap_put(unit->x, unit->y,
                    minimap_rgb((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF));
    }

    minimap_put(view->cursor_x, view->cursor_y,
                minimap_rgb(255, 224, 128));

    /* Camera viewport: the ratio between the full map and the visible part. */
    int vx0 = (int)(view->cam_x * (float)MAP_WIDTH / (float)MAP_PIXEL_WIDTH);
    int vy0 = (int)(view->cam_y * (float)MAP_HEIGHT / (float)MAP_PIXEL_HEIGHT);
    int vx1 = (int)((view->cam_x + SCREEN_WIDTH) * (float)MAP_WIDTH
                    / (float)MAP_PIXEL_WIDTH);
    int vy1 = (int)((view->cam_y + MAP_VIEW_H) * (float)MAP_HEIGHT
                    / (float)MAP_PIXEL_HEIGHT);
    if (vx0 < 0) vx0 = 0;
    if (vy0 < 0) vy0 = 0;
    if (vx1 >= MAP_WIDTH) vx1 = MAP_WIDTH - 1;
    if (vy1 >= MAP_HEIGHT) vy1 = MAP_HEIGHT - 1;
    unsigned short view_color = minimap_rgb(255, 224, 128);
    for (int x = vx0; x <= vx1; x++) {
        minimap_put(x, vy0, view_color);
        minimap_put(x, vy1, view_color);
    }
    for (int y = vy0; y <= vy1; y++) {
        minimap_put(vx0, y, view_color);
        minimap_put(vx1, y, view_color);
    }

    Render_DrawRect(MINIMAP_X, MINIMAP_Y, MINIMAP_W, MINIMAP_H, COLOR_PANEL);
    Render_DrawRect(MINIMAP_X, MINIMAP_Y, MINIMAP_W, 1, COLOR_GOLD);
    Render_DrawRect(MINIMAP_X, MINIMAP_Y + MINIMAP_H - 1,
                    MINIMAP_W, 1, COLOR_GOLD);
    Render_DrawRect(MINIMAP_X, MINIMAP_Y, 1, MINIMAP_H, COLOR_BORDER);
    Render_DrawRect(MINIMAP_X + MINIMAP_W - 1, MINIMAP_Y,
                    1, MINIMAP_H, COLOR_BORDER);
    Render_DrawRect(MINIMAP_MAP_X, MINIMAP_MAP_Y,
                    MINIMAP_MAP_W, MINIMAP_MAP_H, 0xFF05090Cu);
    Render_DrawMinimap((float)MINIMAP_MAP_X, (float)MINIMAP_MAP_Y,
                       (float)MINIMAP_MAP_W, (float)MINIMAP_MAP_H,
                       minimap_pixels, MINIMAP_TEX_W, MINIMAP_TEX_H,
                       (float)MINIMAP_TEX_W, (float)MAP_HEIGHT,
                       0xFFFFFFFFu);
}

static void draw_message(const MapView *view) {
    if (view->message_timer <= 0 || view->message[0] == '\0') return;

    int text_w = Text_Width(view->message, 1);
    int text_h = Text_Height(1);
    float w = (float)text_w + 20.0f;
    float h = (float)text_h + 10.0f;
    float x = (SCREEN_WIDTH - w) / 2.0f;
    float y = SCREEN_HEIGHT - 24.0f - h - 6.0f;

    Render_DrawRect(x, y, w, h, COLOR_PANEL);
    Render_DrawRect(x, y, w, 1, COLOR_GOLD);
    Render_DrawRect(x, y + h - 1, w, 1, COLOR_GOLD);
    Text_DrawCentered(view->message, SCREEN_WIDTH / 2.0f, y + 5, 1, COLOR_TEXT);
}

static void draw_action_menu(const MapView *view, const Map *map,
                             const World *world) {
    const Unit *unit = selected_unit_const(world, view);
    if (!view->menu_open || !unit) return;

    int actions[4];
    StrId items[4];
    int count = menu_actions(world, map, unit, actions, items);
    const float w = 156.0f;
    const float item_h = 18.0f;
    const float title_h = 20.0f;
    float h = title_h + count * item_h + 7.0f;
    float x = SCREEN_WIDTH - w - 8.0f;
    float y = SCREEN_HEIGHT - 24.0f - h - 8.0f;

    Render_DrawRect(x, y, w, h, COLOR_PANEL);
    Render_DrawRect(x, y, w, 1, COLOR_GOLD);
    Render_DrawRect(x, y + h - 1, w, 1, COLOR_GOLD);
    Render_DrawRect(x, y, 1, h, COLOR_BORDER);
    Render_DrawRect(x + w - 1, y, 1, h, COLOR_BORDER);
    Text_Draw(Str(S_ACTION_TITLE), x + 8, y + 5, 1, COLOR_GOLD);

    for (int i = 0; i < count; i++) {
        float iy = y + title_h + (float)i * item_h;
        if (i == view->menu_selection) {
            Render_DrawRect(x + 4, iy, w - 8, item_h - 2, COLOR_PANEL_SEL);
            Render_DrawRect(x + 4, iy, 3, item_h - 2, COLOR_GOLD);
        }
        Text_Draw(Str(items[i]), x + 12, iy + 2, 1,
                  i == view->menu_selection ? COLOR_TEXT : COLOR_MUTED);
    }
}

static void draw_build_menu(const MapView *view, const World *world) {
    const Unit *unit = selected_unit_const(world, view);
    if (!view->build_open || !unit) return;

    const float w = 224.0f;
    const float item_h = 18.0f;
    const float title_h = 20.0f;
    const float hint_h = 16.0f;
    int count = IMPROVEMENT_COUNT - 1;
    float h = title_h + count * item_h + hint_h + 5.0f;
    float x = SCREEN_WIDTH - w - 8.0f;
    float y = SCREEN_HEIGHT - 24.0f - h - 8.0f;

    Render_DrawRect(x, y, w, h, COLOR_PANEL);
    Render_DrawRect(x, y, w, 1, COLOR_GOLD);
    Render_DrawRect(x, y + h - 1, w, 1, COLOR_GOLD);
    Render_DrawRect(x, y, 1, h, COLOR_BORDER);
    Render_DrawRect(x + w - 1, y, 1, h, COLOR_BORDER);
    Text_Draw(Str(S_ACTION_BUILD), x + 8, y + 5, 1, COLOR_GOLD);

    for (int i = 0; i < count; i++) {
        int improvement = i + 1;
        int locked = !Tech_IsKnown(world->researched,
                                   Data_ImprovementRequiredTech(improvement));
        float iy = y + title_h + (float)i * item_h;
        if (i == view->build_selection) {
            Render_DrawRect(x + 4, iy, w - 8, item_h - 2, COLOR_PANEL_SEL);
            Render_DrawRect(x + 4, iy, 3, item_h - 2, COLOR_GOLD);
        }
        Text_Draw(Str(Data_ImprovementName(improvement)), x + 12, iy + 2, 1,
                  i == view->build_selection
                      ? COLOR_TEXT : (locked ? COLOR_MUTED : COLOR_TEXT));
        if (locked) {
            Text_DrawRight(Str(S_TECH_LOCKED), x + w - 10, iy + 2, 1,
                           COLOR_MUTED);
        }
    }

    Text_Draw(Str(S_ACTION_BUILD_HINT), x + 10,
              y + title_h + count * item_h + 2, 1, COLOR_MUTED);
}

void MapView_Render(const MapView *view, const Map *map, const World *world) {
    Render_SetScissor(0, MAP_VIEW_Y, SCREEN_WIDTH, MAP_VIEW_H);
    draw_map_tiles(view, map);
    draw_city_borders(view, map, world);
    draw_entities(view, map, world);
    draw_cursor(view, world);
    Render_FlushLayer();
    Render_SetScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    draw_hud(view, map, world);
    if (view->info_open) {
        draw_info_panel(view, map, world);
    }
    draw_minimap(view, map, world);
    draw_message(view);
    Render_FlushLayer();

    /* The menu gets its own layer so its panel always covers the message. */
    draw_action_menu(view, map, world);
    draw_build_menu(view, world);
    Render_FlushLayer();
}
