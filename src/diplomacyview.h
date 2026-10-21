#ifndef CIVPSP_DIPLOMACYVIEW_H
#define CIVPSP_DIPLOMACYVIEW_H

#include "world.h"

typedef struct {
    int selected;       /* 0..WORLD_AI_COUNT-1; civ = selected + 1 */
    int menu_open;
    int menu_selection;
    int message_timer;
    char message[96];
} DiplomacyView;

void DiplomacyView_Init(DiplomacyView *view, const World *world);

/* Returns 1 when the player asked to return to the map.  Map is supplied so
 * open-borders/alliance vision can refresh immediately after signing. */
int DiplomacyView_Update(DiplomacyView *view, Map *map, World *world);
void DiplomacyView_Render(const DiplomacyView *view, const World *world);

#endif
