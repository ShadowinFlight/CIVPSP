#ifndef CIVPSP_CITYVIEW_H
#define CIVPSP_CITYVIEW_H

#include "city.h"
#include "map.h"
#include "world.h"

typedef enum {
    CITY_TAB_OVERVIEW = 0,
    CITY_TAB_PRODUCTION,
    CITY_TAB_CITIZENS,
    CITY_TAB_BUILDINGS,
    CITY_TAB_SCIENCE,
    CITY_TAB_COUNT
} CityTab;

typedef struct {
    int city_index;
    CityTab tab;
    int tile_cursor;
    int list_cursor;
    int list_scroll;
    int building_cursor;
    int tech_cursor;
    int tech_scroll;
    int message_timer;
    char message[80];
} CityView;

void CityView_Init(CityView *view, int city_index);

/* Returns 1 when the player asked to return to the map. */
int CityView_Update(CityView *view, Map *map, World *world);
void CityView_Render(const CityView *view, const Map *map, const World *world);

#endif
