#ifndef CIVPSP_MAPVIEW_H
#define CIVPSP_MAPVIEW_H

#include <stdint.h>

#include "map.h"
#include "world.h"

typedef struct {
    float cam_x;
    float cam_y;
    float target_x;
    float target_y;
    int cursor_x;
    int cursor_y;
    int info_open;
    int minimap_open;
    int selected;          /* index into world->units, -1 when none */
    int open_city;         /* city index + 1 requested by the player */
    int open_diplomacy;    /* Start requested the diplomacy screen */
    int cursor_reachable;  /* selected unit can reach the cursor this turn */
    int cursor_attack;     /* selected unit can attack the cursor this turn */
    int menu_open;
    int menu_selection;
    int build_open;
    int build_selection;
    int deny_timer;
    int message_timer;
    char message[96];
    int fps;
    int fps_frames;
    uint32_t fps_last_us;
    /* M7: the map view asks the app for an autosave after each end turn and
     * displays the remaining time in timed mode. */
    int turn_ended;
    int time_active;
    uint32_t time_remaining_seconds;
    int turn_limit;            /* 0 = unlimited, otherwise standard limit */
} MapView;

void MapView_Init(MapView *view, Map *map, const World *world);
void MapView_Restore(MapView *view, Map *map, const World *world,
                     int cursor_x, int cursor_y, int selected,
                     float cam_x, float cam_y);

/* Returns 1 when the player asks for the pause menu. */
int MapView_Update(MapView *view, Map *map, World *world);
void MapView_Render(const MapView *view, const Map *map, const World *world);

#endif
