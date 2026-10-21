#include "cityview.h"

#include <pspctrl.h>

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "data.h"
#include "game.h"
#include "tech.h"
#include "gen/strings_gen.h"
#include "gen/terrain_gen.h"
#include "input.h"
#include "log.h"
#include "render.h"
#include "text.h"

#define TOP_H          24
#define TAB_Y          TOP_H
#define TAB_H          20
#define TAB_X0         8
#define TAB_W          92
#define TAB_STEP       94

#define GRID_X         8
#define GRID_Y         48
#define TILE_PX        36
#define GRID_PANEL_X   4
#define GRID_PANEL_Y   44
#define GRID_PANEL_W   188
#define GRID_PANEL_H   188

#define RIGHT_X        196
#define RIGHT_Y        48
#define RIGHT_W        276
#define RIGHT_H        198

#define FOOTER_Y       248
#define FOOTER_H       24
#define MESSAGE_FRAMES 150

#define CITY_SPRITE    3

static void clamp_view(CityView *view);

#define COLOR_BG       0xFF0E1820u
#define COLOR_TOP      0xFF14242Cu
#define COLOR_PANEL    0xF20A1116u
#define COLOR_PANEL_OP 0xFF0A1116u
#define COLOR_PANEL_SEL 0x882B4749u
#define COLOR_BORDER   0xFF3E5A5Cu
#define COLOR_GOLD     0xFFD8B25Cu
#define COLOR_TEXT     0xFFF5EEDCu
#define COLOR_MUTED    0xFF9FB0A8u
#define COLOR_FOOD     0xFF7ED37Eu
#define COLOR_PROD     0xFFE0A050u
#define COLOR_SCI      0xFF70C0E0u
#define COLOR_INF      0xFFB080D0u
#define COLOR_DENY     0xFFE06050u
#define COLOR_OWNER    0xFF4A90D9u
#define COLOR_DARK     0xFF070C10u
#define COLOR_DIM      0x66000000u

static const City *get_city(const CityView *view, const World *world) {
    if (!view || !world || view->city_index < 0
        || view->city_index >= world->cities.count) {
        return NULL;
    }
    const City *city = &world->cities.cities[view->city_index];
    if (city->owner != WORLD_CIV_PLAYER) return NULL;
    return city;
}

static StrId tab_name(CityTab tab) {
    switch (tab) {
        case CITY_TAB_PRODUCTION: return S_CITY_TAB_PRODUCTION;
        case CITY_TAB_CITIZENS:   return S_CITY_TAB_CITIZENS;
        case CITY_TAB_BUILDINGS:  return S_CITY_TAB_BUILDINGS;
        case CITY_TAB_SCIENCE:    return S_CITY_TAB_SCIENCE;
        default:                  return S_CITY_TAB_OVERVIEW;
    }
}

static StrId tab_footer(CityTab tab) {
    switch (tab) {
        case CITY_TAB_PRODUCTION: return S_CITY_FOOTER_PRODUCTION;
        case CITY_TAB_CITIZENS:   return S_CITY_FOOTER_CITIZENS;
        case CITY_TAB_BUILDINGS:  return S_CITY_FOOTER_BUILDINGS;
        case CITY_TAB_SCIENCE:    return S_CITY_FOOTER_SCIENCE;
        default:                  return S_CITY_FOOTER_OVERVIEW;
    }
}

static void set_message(CityView *view, const char *text) {
    snprintf(view->message, sizeof(view->message), "%s", text);
    view->message_timer = MESSAGE_FRAMES;
}

static void draw_atlas_tile(float x, float y, int size, int col, int row) {
    float u0 = (float)(col * TERRAIN_ATLAS_TILE_W);
    float v0 = (float)(row * TERRAIN_ATLAS_TILE_H);
    Render_DrawSprite(x, y, (float)size, (float)size, u0, v0,
                      u0 + TERRAIN_ATLAS_TILE_W,
                      v0 + TERRAIN_ATLAS_TILE_H, 0xFFFFFFFFu);
}

static void draw_frame(float x, float y, float w, float h, unsigned int color) {
    Render_DrawRect(x, y, w, 1, color);
    Render_DrawRect(x, y + h - 1, w, 1, color);
    Render_DrawRect(x, y, 1, h, color);
    Render_DrawRect(x + w - 1, y, 1, h, color);
}

static int tab_x(int index) {
    return TAB_X0 + index * TAB_STEP;
}

static void draw_backgrounds(void) {
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    Render_DrawRect(0, 0, SCREEN_WIDTH, TOP_H, COLOR_TOP);
    Render_DrawRect(0, TOP_H - 1, SCREEN_WIDTH, 1, COLOR_GOLD);
    Render_DrawRect(0, TAB_Y, SCREEN_WIDTH, TAB_H, COLOR_PANEL_OP);
    Render_DrawRect(0, TAB_Y + TAB_H - 1, SCREEN_WIDTH, 1, COLOR_BORDER);

    Render_DrawRect(GRID_PANEL_X, GRID_PANEL_Y, GRID_PANEL_W, GRID_PANEL_H,
                    COLOR_PANEL_OP);
    draw_frame(GRID_PANEL_X, GRID_PANEL_Y, GRID_PANEL_W, GRID_PANEL_H,
               COLOR_BORDER);

    Render_DrawRect(RIGHT_X, RIGHT_Y, RIGHT_W, RIGHT_H, COLOR_PANEL);
    Render_DrawRect(RIGHT_X, RIGHT_Y, RIGHT_W, 1, COLOR_GOLD);
    draw_frame(RIGHT_X, RIGHT_Y, RIGHT_W, RIGHT_H, COLOR_BORDER);

    Render_DrawRect(0, FOOTER_Y, SCREEN_WIDTH, FOOTER_H, COLOR_TOP);
    Render_DrawRect(0, FOOTER_Y, SCREEN_WIDTH, 1, COLOR_GOLD);
}

static void draw_grid_sprites(const CityView *view, const Map *map,
                              const World *world) {
    const City *city = get_city(view, world);
    if (!city) return;

    for (int i = 0; i < CITY_WORK_TILES; i++) {
        int col = i % CITY_WORK_DIAMETER;
        int row = i / CITY_WORK_DIAMETER;
        float gx = (float)(GRID_X + col * TILE_PX);
        float gy = (float)(GRID_Y + row * TILE_PX);

        int tx = 0;
        int ty = 0;
        City_TileCoord(city, i, &tx, &ty);
        if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT) {
            Render_DrawRect(gx, gy, TILE_PX, TILE_PX, COLOR_DARK);
            continue;
        }

        const MapTile *tile = &map->tiles[ty][tx];
        if (!tile->explored) {
            Render_DrawRect(gx, gy, TILE_PX, TILE_PX, COLOR_DARK);
            continue;
        }

        int terrain = (int)tile->terrain;
        if (terrain >= 0 && terrain < TERRAIN_COUNT) {
            draw_atlas_tile(gx, gy, TILE_PX, terrain % TERRAIN_ATLAS_COLS,
                            terrain / TERRAIN_ATLAS_COLS);
        } else {
            Render_DrawRect(gx, gy, TILE_PX, TILE_PX, COLOR_DARK);
        }

        if (tile->resource != RESOURCE_NONE) {
            int rcol = (int)tile->resource - 1;
            draw_atlas_tile(gx, gy, TILE_PX, rcol, TERRAIN_ATLAS_RESOURCE_ROW);
        }

        if (i == CITY_CENTER_INDEX) {
            draw_atlas_tile(gx, gy, TILE_PX, CITY_SPRITE, TERRAIN_ATLAS_UNIT_ROW);
        }
    }

    for (int i = 0; i < CITY_WORK_TILES; i++) {
        int col = i % CITY_WORK_DIAMETER;
        int row = i / CITY_WORK_DIAMETER;
        float gx = (float)(GRID_X + col * TILE_PX);
        float gy = (float)(GRID_Y + row * TILE_PX);

        int tx = 0;
        int ty = 0;
        City_TileCoord(city, i, &tx, &ty);
        if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT) continue;
        if (!map->tiles[ty][tx].explored) continue;

        if (City_OwnsIndex(city, i)) {
            if (!City_OwnsTile(city, tx, ty - 1)) {
                Render_DrawRect(gx, gy, TILE_PX, 1, COLOR_OWNER);
            }
            if (!City_OwnsTile(city, tx, ty + 1)) {
                Render_DrawRect(gx, gy + TILE_PX - 1, TILE_PX, 1, COLOR_OWNER);
            }
            if (!City_OwnsTile(city, tx - 1, ty)) {
                Render_DrawRect(gx, gy, 1, TILE_PX, COLOR_OWNER);
            }
            if (!City_OwnsTile(city, tx + 1, ty)) {
                Render_DrawRect(gx + TILE_PX - 1, gy, 1, TILE_PX, COLOR_OWNER);
            }
        } else {
            Render_DrawRect(gx, gy, TILE_PX, TILE_PX, COLOR_DIM);
            draw_frame(gx, gy, TILE_PX, TILE_PX, 0xFF26343Au);
        }
        if (City_IsWorkedIndex(city, i)) {
            Render_DrawRect(gx + 2, gy + TILE_PX - 3, TILE_PX - 4, 2,
                            COLOR_FOOD);
        }
        if (City_IsLockedIndex(city, i)) {
            Render_DrawRect(gx + TILE_PX - 7, gy + 2, 4, 4, COLOR_GOLD);
        }
    }

    if (view->tab == CITY_TAB_CITIZENS) {
        int col = view->tile_cursor % CITY_WORK_DIAMETER;
        int row = view->tile_cursor / CITY_WORK_DIAMETER;
        float gx = (float)(GRID_X + col * TILE_PX);
        float gy = (float)(GRID_Y + row * TILE_PX);
        unsigned int color = COLOR_GOLD;
        if (!City_OwnsIndex(city, view->tile_cursor)) {
            CityTileResult buy = City_CanBuyTile(city, map, view->tile_cursor);
            if (buy != CITY_TILE_OK) color = COLOR_MUTED;
        }
        Render_DrawRect(gx, gy, TILE_PX, 2, color);
        Render_DrawRect(gx, gy + TILE_PX - 2, TILE_PX, 2, color);
        Render_DrawRect(gx, gy, 2, TILE_PX, color);
        Render_DrawRect(gx + TILE_PX - 2, gy, 2, TILE_PX, color);
    }
}

static void draw_selection_bars(const CityView *view, const World *world) {
    (void)world;
    int tx = tab_x((int)view->tab);
    Render_DrawRect((float)(tx + 2), (float)(TAB_Y + 2),
                    (float)(TAB_W - 4), (float)(TAB_H - 4), COLOR_PANEL_SEL);
    Render_DrawRect((float)(tx + 2), (float)(TAB_Y + TAB_H - 3),
                    (float)(TAB_W - 4), 2, COLOR_GOLD);

    if (view->tab == CITY_TAB_PRODUCTION) {
        int row = view->list_cursor - view->list_scroll;
        if (row >= 0 && row < 4) {
            Render_DrawRect((float)(RIGHT_X + 4),
                            (float)(RIGHT_Y + 124 + row * 18),
                            (float)(RIGHT_W - 8), 18, COLOR_PANEL_SEL);
        }
    } else if (view->tab == CITY_TAB_BUILDINGS) {
        Render_DrawRect((float)(RIGHT_X + 4),
                        (float)(RIGHT_Y + 22 + view->building_cursor * 17),
                        (float)(RIGHT_W - 8), 16, COLOR_PANEL_SEL);
    } else if (view->tab == CITY_TAB_SCIENCE) {
        int sel = view->tech_cursor - view->tech_scroll - 1;
        if (sel >= 0 && sel < 6) {
            Render_DrawRect((float)(RIGHT_X + 4),
                            (float)(RIGHT_Y + 70 + sel * 18),
                            (float)(RIGHT_W - 8), 18, COLOR_PANEL_SEL);
        }
    }
}

static void draw_top_text(const World *world, const City *city) {
    Text_Draw(Str(Data_CityName(city->name_index)), 8, 5, 1, COLOR_GOLD);
    Text_Drawf(100, 5, 1, COLOR_TEXT, "%s %d", Str(S_MAP_TURN), world->turn);
    Text_Drawf(176, 5, 1, COLOR_GOLD, "%s %d",
               Str(S_MAP_GOLD), (int)world->treasury);
    Text_Drawf(252, 5, 1, COLOR_SCI, "%s %d",
               Str(S_MAP_SCI), (int)world->science);
}

static void draw_tab_text(const CityView *view) {
    for (int i = 0; i < CITY_TAB_COUNT; i++) {
        int x = tab_x(i);
        unsigned int color = (int)view->tab == i ? COLOR_GOLD : COLOR_MUTED;
        Text_DrawCentered(Str(tab_name((CityTab)i)),
                          (float)x + TAB_W / 2.0f,
                          (float)(TAB_Y + 3), 1, color);
    }
}

static void draw_grid_text(const CityView *view, const Map *map,
                           const World *world) {
    const City *city = get_city(view, world);
    if (!city) return;

    for (int i = 0; i < CITY_WORK_TILES; i++) {
        if (i == CITY_CENTER_INDEX) continue;
        int col = i % CITY_WORK_DIAMETER;
        int row = i / CITY_WORK_DIAMETER;
        float gx = (float)(GRID_X + col * TILE_PX);
        float gy = (float)(GRID_Y + row * TILE_PX);

        int tx = 0;
        int ty = 0;
        City_TileCoord(city, i, &tx, &ty);
        if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT) continue;
        if (!map->tiles[ty][tx].explored) continue;

        Yield yield;
        Data_TileYield(map, tx, ty, &yield);
        int owned = City_OwnsIndex(city, i);
        unsigned int food_color = owned ? COLOR_FOOD : COLOR_MUTED;
        unsigned int prod_color = owned ? COLOR_PROD : COLOR_MUTED;
        unsigned int gold_color = owned ? COLOR_GOLD : COLOR_MUTED;

        if (yield.food > 0) {
            Text_Drawf(gx + 2, gy + 2, 1, food_color, "%d", yield.food);
        }
        if (yield.production > 0) {
            Text_Drawf(gx + 25, gy + 2, 1, prod_color, "%d", yield.production);
        }
        if (yield.gold > 0) {
            Text_Drawf(gx + 2, gy + 18, 1, gold_color, "%d", yield.gold);
        }
        if (yield.science > 0) {
            Text_Drawf(gx + 25, gy + 18, 1, COLOR_SCI, "%d", yield.science);
        }
    }
}

static void append_effect(char *buf, size_t size, size_t *used,
                          int value, StrId short_name) {
    if (value <= 0 || *used >= size) return;
    int n = snprintf(buf + *used, size - *used, "%s+%d%s",
                     *used ? " " : "", value, Str(short_name));
    if (n > 0) *used += (size_t)n;
}

static void format_effect(const ProductionDef *def, char *buf, size_t size) {
    size_t used = 0;
    buf[0] = '\0';
    append_effect(buf, size, &used, def->food, S_STAT_FOOD_SHORT);
    append_effect(buf, size, &used, def->production, S_STAT_PROD_SHORT);
    append_effect(buf, size, &used, def->gold, S_STAT_GOLD_SHORT);
    append_effect(buf, size, &used, def->science, S_STAT_SCI_SHORT);
    append_effect(buf, size, &used, def->influence, S_STAT_INF_SHORT);
    append_effect(buf, size, &used, def->defense, S_STAT_DEF_SHORT);
    if (used == 0) snprintf(buf, size, "%s", Str(S_CITY_NO_EFFECT));
}

static void draw_overview(const City *city, const Map *map,
                          const World *world) {
    CityYield yield;
    City_ComputeYieldsWithTech(city, map, world->researched, &yield);
    int surplus = yield.food - (int)city->population * CITY_FOOD_PER_POP;
    int growth = City_GrowthThreshold(city);
    int border = City_BorderThreshold(city);
    float x = (float)(RIGHT_X + 10);
    float y = (float)(RIGHT_Y + 8);
    const float step = 16.0f;

    Text_Draw(Str(S_CITY_CITY_INFO), x, y, 1, COLOR_GOLD);
    y += 20.0f;

    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %d/%d",
               Str(S_STAT_POP), city->population, Str(S_STAT_WORKED),
               City_WorkedCount(city), city->population);
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %+d",
               Str(S_STAT_FOOD), yield.food, Str(S_STAT_NET), surplus);
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d/%d",
               Str(S_CITY_FOOD_STORED), city->food_stored, growth);
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %d  %s %d",
               Str(S_STAT_PROD), yield.production, Str(S_STAT_GOLD), yield.gold,
               Str(S_STAT_SCI), yield.science);
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d/%d  %s %d/%d",
               Str(S_STAT_INF), city->influence, border, Str(S_CITY_BORDER),
               City_OwnedCount(city), CITY_WORK_TILES);
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %d",
               Str(S_STAT_MAINT), yield.maintenance, Str(S_STAT_DEF),
               yield.defense);
    y += step;

    int net_gold = World_NetGold(world, map);
    Text_Drawf(x, y, 1, net_gold < 0 ? COLOR_DENY : COLOR_TEXT,
               "%s %+d %s  %s %+d", Str(S_STAT_NET), net_gold,
               Str(S_STAT_GOLD), Str(S_STAT_SCI), yield.science);
    y += step;

    const ProductionDef *front = city->queue_count > 0
        ? Data_ProductionDef((ProductionType)city->queue[0]) : NULL;
    if (front) {
        Text_Drawf(x, y, 1, COLOR_TEXT, "%s %s",
                   Str(S_CITY_QUEUE), Str(front->name_id));
    } else {
        Text_Draw(Str(S_CITY_NO_QUEUE), x, y, 1, COLOR_MUTED);
    }
    y += step;

    if (front) {
        int turns = City_TurnsLeftWithTech(city, map, world->researched);
        Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %d",
                   Str(S_STAT_TURNS), turns, Str(S_CITY_RUSH),
                   City_RushCost(city));
        y += step;
    }
    Text_Drawf(x, y, 1, COLOR_MUTED, "%s %d/%d",
               Str(S_CITY_TAB_BUILDINGS), City_BuildingCount(city),
               BUILDING_COUNT);
}

static void draw_production(const City *city) {
    float x = (float)(RIGHT_X + 10);
    float right = (float)(RIGHT_X + RIGHT_W - 10);
    float y = (float)(RIGHT_Y + 6);
    char buf[32];

    Text_Draw(Str(S_CITY_QUEUE), x, y, 1, COLOR_GOLD);
    if (city->queue_count > 0) {
        snprintf(buf, sizeof(buf), "%s %d", Str(S_CITY_RUSH),
                 City_RushCost(city));
        Text_DrawRight(buf, right, y, 1, COLOR_GOLD);
    }
    y += 18.0f;

    if (city->queue_count == 0) {
        Text_Draw(Str(S_CITY_EMPTY), x, y, 1, COLOR_MUTED);
    }
    for (int i = 0; i < city->queue_count; i++) {
        const ProductionDef *def =
            Data_ProductionDef((ProductionType)city->queue[i]);
        if (!def) continue;
        float row_y = y + (float)i * 16.0f;
        if (i == 0) {
            Text_Drawf(x, row_y, 1, COLOR_GOLD, ">1 %s %d/%d",
                       Str(def->name_id), city->production_stored, def->cost);
        } else {
            Text_Drawf(x, row_y, 1, COLOR_TEXT, " %d %s %d", i + 1,
                       Str(def->name_id), def->cost);
        }
    }

    Render_DrawRect((float)(RIGHT_X + 10), (float)(RIGHT_Y + 106),
                    (float)(RIGHT_W - 20), 1, COLOR_BORDER);
    Text_Draw(Str(S_CITY_AVAILABLE), x, (float)(RIGHT_Y + 108), 1, COLOR_GOLD);
    Text_DrawRight(Str(S_CITY_PURCHASE_COST), right, (float)(RIGHT_Y + 108), 1,
                   COLOR_MUTED);
}

/* The production list renderer is split out so it can use the view state. */
static void draw_production_list(const CityView *view, const City *city,
                                 const World *world) {
    float x = (float)(RIGHT_X + 10);
    float right = (float)(RIGHT_X + RIGHT_W - 10);
    for (int row = 0; row < 4; row++) {
        int item = view->list_scroll + row;
        int type = PROD_SETTLER + item;
        if (type >= PROD_COUNT) break;
        const ProductionDef *def = Data_ProductionDef((ProductionType)type);
        if (!def) continue;
        float row_y = (float)(RIGHT_Y + 124 + row * 18);

        int built = def->kind == PROD_KIND_BUILDING
                    && City_HasBuilding(city, (BuildingType)def->building_type);
        int queued = City_QueueContains(city, (ProductionType)type);
        int locked = def->required_tech != TECH_NONE
                     && !Tech_IsKnown(world->researched, def->required_tech);
        char status[24];
        if (built) {
            snprintf(status, sizeof(status), "%s", Str(S_CITY_BUILT));
        } else if (queued) {
            snprintf(status, sizeof(status), "%s", Str(S_CITY_QUEUED));
        } else if (locked) {
            snprintf(status, sizeof(status), "%s", Str(S_TECH_REQUIRES));
        } else {
            snprintf(status, sizeof(status), "%d", def->cost);
        }

        unsigned int color = built ? COLOR_FOOD
                            : queued ? COLOR_GOLD
                            : locked ? COLOR_MUTED : COLOR_TEXT;
        Text_Draw(Str(def->name_id), x, row_y, 1, color);
        Text_DrawRight(status, right, row_y, 1, color);
    }
}

static void draw_citizens(const CityView *view, const City *city,
                          const Map *map) {
    float x = (float)(RIGHT_X + 10);
    float y = (float)(RIGHT_Y + 8);
    const float step = 18.0f;
    int index = view->tile_cursor;

    Text_Draw(Str(S_CITY_TILE_INFO), x, y, 1, COLOR_GOLD);
    y += 20.0f;

    int tx = 0;
    int ty = 0;
    City_TileCoord(city, index, &tx, &ty);
    int valid = tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT;
    if (!valid) {
        Text_Drawf(x, y, 1, COLOR_MUTED, "%s %d,%d", Str(S_MAP_TILE), tx, ty);
        y += step;
        Text_Draw(Str(S_CITY_OUT_OF_RANGE), x, y, 1, COLOR_MUTED);
        return;
    }

    const MapTile *tile = &map->tiles[ty][tx];
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d,%d  %s", Str(S_MAP_TILE), tx, ty,
               Str(Data_TerrainName(tile->terrain)));
    y += step;
    if (!tile->explored) {
        Text_Draw(Str(S_CITY_UNEXPLORED), x, y, 1, COLOR_MUTED);
        return;
    }
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %s", Str(S_MAP_RESOURCE),
               Str(Data_ResourceName(tile->resource)));
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %s", Str(S_MAP_IMPROVEMENT),
               Str(Data_ImprovementName(tile->improvement)));
    y += step;

    Yield yield;
    if (index == CITY_CENTER_INDEX) {
        yield = (Yield){ CITY_CENTER_FOOD, CITY_CENTER_PROD,
                         CITY_CENTER_GOLD, CITY_CENTER_SCIENCE };
    } else {
        Data_TileYield(map, tx, ty, &yield);
    }
    if (index == CITY_CENTER_INDEX) {
        Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %d  %s %d  %s %d",
                   Str(S_STAT_FOOD), yield.food, Str(S_STAT_PROD),
                   yield.production, Str(S_STAT_GOLD), yield.gold,
                   Str(S_STAT_SCI), yield.science);
    } else {
        Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %d  %s %d  %s %d",
                   Str(S_STAT_FOOD), yield.food, Str(S_STAT_PROD),
                   yield.production, Str(S_STAT_GOLD), yield.gold,
                   Str(S_STAT_SCI), yield.science);
    }
    y += step;

    int owned = City_OwnsIndex(city, index);
    if (owned) {
        char state[48];
        snprintf(state, sizeof(state), "%s", Str(S_STAT_OWNED));
        if (index == CITY_CENTER_INDEX) {
            strncat(state, "  ", sizeof(state) - strlen(state) - 1);
            strncat(state, Str(S_CITY_CENTER),
                    sizeof(state) - strlen(state) - 1);
        } else if (City_IsWorkedIndex(city, index)) {
            strncat(state, "  ", sizeof(state) - strlen(state) - 1);
            strncat(state, Str(S_STAT_WORKED),
                    sizeof(state) - strlen(state) - 1);
        }
        if (index != CITY_CENTER_INDEX
            && City_IsLockedIndex(city, index)) {
            strncat(state, "  ", sizeof(state) - strlen(state) - 1);
            strncat(state, Str(S_STAT_LOCKED),
                    sizeof(state) - strlen(state) - 1);
        }
        Text_Draw(state, x, y, 1, COLOR_TEXT);
    } else {
        CityTileResult buy = City_CanBuyTile(city, map, index);
        if (buy == CITY_TILE_OK) {
            Text_Drawf(x, y, 1, COLOR_GOLD, "%s  %s %d %s",
                       Str(S_CITY_UNOWNED), Str(S_CITY_BUY),
                       City_TilePurchaseCost(city), Str(S_STAT_GOLD));
        } else if (buy == CITY_TILE_NOT_ADJACENT) {
            Text_Drawf(x, y, 1, COLOR_MUTED, "%s  %s",
                       Str(S_CITY_UNOWNED), Str(S_CITY_NOT_ADJACENT));
        } else {
            Text_Draw(Str(S_CITY_UNOWNED), x, y, 1, COLOR_MUTED);
        }
    }
    y += step;

    CityYield city_yield;
    City_ComputeYields(city, map, &city_yield);
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d  %s %d/%d",
               Str(S_STAT_POP), city->population, Str(S_STAT_WORKED),
               City_WorkedCount(city), city->population);
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %+d  %s %d  %s %d",
               Str(S_STAT_FOOD), city_yield.food
                   - (int)city->population * CITY_FOOD_PER_POP,
               Str(S_STAT_PROD), city_yield.production, Str(S_STAT_GOLD),
               city_yield.gold);
    y += step;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %d/%d  %s %d/%d",
               Str(S_STAT_INF), city->influence, City_BorderThreshold(city),
               Str(S_CITY_BORDER), City_OwnedCount(city), CITY_WORK_TILES);
}

static void draw_buildings(const CityView *view, const City *city,
                           const World *world) {
    float x = (float)(RIGHT_X + 10);
    float status_right = (float)(RIGHT_X + RIGHT_W - 10);
    float effect_x = (float)(RIGHT_X + 84);
    float y = (float)(RIGHT_Y + 6);

    Text_Drawf(x, y, 1, COLOR_GOLD, "%s %d/%d",
               Str(S_CITY_TAB_BUILDINGS), City_BuildingCount(city),
               BUILDING_COUNT);

    for (int i = 0; i < BUILDING_COUNT; i++) {
        const ProductionDef *def = Data_BuildingDef((BuildingType)i);
        if (!def) continue;
        float row_y = (float)(RIGHT_Y + 22 + i * 17);
        int built = City_HasBuilding(city, (BuildingType)i);
        int queued = City_QueueContains(city, Data_ProductionForBuilding(
                                                   (BuildingType)i));
        int locked = def->required_tech != TECH_NONE
                     && !Tech_IsKnown(world->researched, def->required_tech);
        char effect[96];
        format_effect(def, effect, sizeof(effect));
        unsigned int color = built ? COLOR_FOOD
                            : queued ? COLOR_GOLD
                            : locked ? COLOR_MUTED : COLOR_TEXT;
        Text_Draw(Str(def->name_id), x, row_y, 1, color);
        Text_Draw(effect, effect_x, row_y, 1, COLOR_MUTED);
        if (built) {
            Text_DrawRight(Str(S_CITY_BUILT), status_right, row_y, 1, color);
        } else if (queued) {
            Text_DrawRight(Str(S_CITY_QUEUED), status_right, row_y, 1, color);
        } else if (locked) {
            Text_DrawRight(Str(S_TECH_LOCKED), status_right, row_y, 1, color);
        } else {
            char cost[8];
            snprintf(cost, sizeof(cost), "%d", def->cost);
            Text_DrawRight(cost, status_right, row_y, 1, color);
        }
    }
}

static void draw_science(const CityView *view, const World *world) {
    float x = (float)(RIGHT_X + 10);
    float right = (float)(RIGHT_X + RIGHT_W - 10);
    float y = (float)(RIGHT_Y + 6);

    char total[32];
    Text_Draw(Str(S_CITY_TAB_SCIENCE), x, y, 1, COLOR_GOLD);
    snprintf(total, sizeof(total), "%s %d", Str(S_MAP_SCI),
             (int)world->science);
    Text_DrawRight(total, right, y, 1, COLOR_SCI);
    y += 16.0f;

    snprintf(total, sizeof(total), Str(S_TECH_PROJECTS),
             World_ScienceGoalCount(world), WORLD_SCIENCE_GOAL_COUNT);
    Text_Draw(total, x, y, 1, COLOR_GOLD);
    y += 16.0f;

    int target = World_ResearchType(world);
    if (target > TECH_NONE) {
        Text_Drawf(x, y, 1, COLOR_GOLD, "%s %s  %d/%d",
                   Str(S_TECH_RESEARCHING), Str(Tech_Name(target)),
                   (int)world->research_progress, Tech_Cost(target));
    } else {
        Text_Draw(Str(S_TECH_NO_RESEARCH), x, y, 1, COLOR_MUTED);
    }

    for (int row = 0; row < 6; row++) {
        int tech = view->tech_scroll + 1 + row;
        if (tech >= TECH_COUNT) break;
        const TechDef *def = &g_tech_defs[tech];
        float row_y = (float)(RIGHT_Y + 70 + row * 18);

        int known = Tech_IsKnown(world->researched, tech);
        int is_curr = world->research_type == tech;
        int avail = Tech_CanSelect(world->researched, tech);
        char status[24];
        unsigned int color;
        if (known) {
            color = COLOR_SCI;
            snprintf(status, sizeof(status), "%s", Str(S_TECH_RESEARCHED));
        } else if (is_curr) {
            color = COLOR_GOLD;
            snprintf(status, sizeof(status), "%d/%d", (int)world->research_progress,
                     Tech_Cost(tech));
        } else if (avail) {
            color = COLOR_TEXT;
            snprintf(status, sizeof(status), "%d", Tech_Cost(tech));
        } else {
            color = COLOR_MUTED;
            snprintf(status, sizeof(status), "%s", Str(S_TECH_LOCKED));
        }
        Text_Draw(Str(def->name_id), x, row_y, 1, color);
        Text_DrawRight(status, right, row_y, 1, color);
    }

    if (view->tech_cursor > TECH_NONE && view->tech_cursor < TECH_COUNT) {
        const TechDef *selected = &g_tech_defs[view->tech_cursor];
        Text_Draw(Str(selected->unlock_id), x, (float)(RIGHT_Y + 180), 1,
                  COLOR_MUTED);
    }
}

static void update_science(CityView *view, World *world) {
    if (Input_Repeated(PSP_CTRL_UP)) view->tech_cursor--;
    if (Input_Repeated(PSP_CTRL_DOWN)) view->tech_cursor++;
    clamp_view(view);
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        int tech = view->tech_cursor;
        if (World_SetResearch(world, tech)) {
            view->message[0] = 0;
            snprintf(view->message, sizeof(view->message), "%s %s",
                     Str(S_TECH_RESEARCHING), Str(Tech_Name(tech)));
            view->message_timer = MESSAGE_FRAMES;
        } else {
            set_message(view, Str(S_TECH_LOCKED));
        }
    }
}

static void draw_panel_text(const CityView *view, const Map *map,
                            const World *world) {
    const City *city = get_city(view, world);
    if (!city) return;
    switch (view->tab) {
        case CITY_TAB_PRODUCTION:
            draw_production(city);
            draw_production_list(view, city, world);
            break;
        case CITY_TAB_CITIZENS:
            draw_citizens(view, city, map);
            break;
        case CITY_TAB_BUILDINGS:
            draw_buildings(view, city, world);
            break;
        case CITY_TAB_SCIENCE:
            draw_science(view, world);
            break;
        default:
            draw_overview(city, map, world);
            break;
    }
}

static void draw_footer(const CityView *view) {
    Text_Draw(Str(tab_footer(view->tab)), 8, (float)(FOOTER_Y + 5), 1,
              COLOR_MUTED);
    Text_DrawRight(Str(S_CITY_FOOTER_BACK), SCREEN_WIDTH - 8,
                   (float)(FOOTER_Y + 5), 1, COLOR_MUTED);
}

static void draw_message(const CityView *view) {
    if (view->message_timer <= 0 || view->message[0] == '\0') return;
    int text_w = Text_Width(view->message, 1);
    int text_h = Text_Height(1);
    float w = (float)text_w + 20.0f;
    float h = (float)text_h + 10.0f;
    float x = (SCREEN_WIDTH - w) / 2.0f;
    float y = (float)(FOOTER_Y - 6) - h;

    Render_DrawRect(x, y, w, h, COLOR_PANEL_OP);
    draw_frame(x, y, w, h, COLOR_GOLD);
    Render_FlushLayer();
    Text_DrawCentered(view->message, SCREEN_WIDTH / 2.0f, y + 5, 1,
                      COLOR_TEXT);
    Render_FlushLayer();
}

void CityView_Init(CityView *view, int city_index) {
    memset(view, 0, sizeof(*view));
    view->city_index = city_index;
    view->tab = CITY_TAB_OVERVIEW;
    view->tile_cursor = CITY_CENTER_INDEX;
    view->list_cursor = 0;
    view->list_scroll = 0;
    view->building_cursor = 0;
    view->tech_cursor = 1;
    view->tech_scroll = 0;
}

static void clamp_view(CityView *view) {
    if (view->tab < 0 || view->tab >= CITY_TAB_COUNT) {
        view->tab = CITY_TAB_OVERVIEW;
    }
    if (view->tile_cursor < 0) view->tile_cursor = 0;
    if (view->tile_cursor >= CITY_WORK_TILES) {
        view->tile_cursor = CITY_WORK_TILES - 1;
    }
    if (view->list_cursor < 0) view->list_cursor = 0;
    if (view->list_cursor >= PROD_COUNT - 1) {
        view->list_cursor = PROD_COUNT - 2;
    }
    if (view->list_cursor < view->list_scroll) {
        view->list_scroll = view->list_cursor;
    }
    if (view->list_cursor >= view->list_scroll + 4) {
        view->list_scroll = view->list_cursor - 3;
    }
    if (view->list_scroll < 0) view->list_scroll = 0;
    if (view->list_scroll > PROD_COUNT - 5) {
        view->list_scroll = PROD_COUNT - 5;
    }
    if (view->building_cursor < 0) view->building_cursor = 0;
    if (view->building_cursor >= BUILDING_COUNT) {
        view->building_cursor = BUILDING_COUNT - 1;
    }
    if (view->tech_cursor < 1) view->tech_cursor = 1;
    int tmax = TECH_COUNT - 1;
    if (view->tech_cursor > tmax) view->tech_cursor = tmax;
    if (view->tech_scroll < 0) view->tech_scroll = 0;
    if (view->tech_scroll > tmax - 6) view->tech_scroll = tmax - 6;
    if (view->tech_cursor < view->tech_scroll + 1) {
        view->tech_scroll = view->tech_cursor - 1;
    }
    if (view->tech_cursor > view->tech_scroll + 6) {
        view->tech_scroll = view->tech_cursor - 6;
    }
}

static void update_production(CityView *view, City *city, World *world) {
    const int count = PROD_COUNT - 1;
    if (Input_Repeated(PSP_CTRL_UP)) {
        view->list_cursor = (view->list_cursor + count - 1) % count;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        view->list_cursor = (view->list_cursor + 1) % count;
    }
    clamp_view(view);

    if (Input_Pressed(PSP_CTRL_CROSS)) {
        ProductionType type = (ProductionType)(PROD_SETTLER
                                               + view->list_cursor);
        const ProductionDef *def = Data_ProductionDef(type);
        if (def && def->required_tech != TECH_NONE
            && !Tech_IsKnown(world->researched, def->required_tech)) {
            set_message(view, Str(S_TECH_REQUIRES));
            return;
        }
        CityQueueResult result = City_QueueAdd(city, type);
        if (result == CITY_QUEUE_OK) {
            set_message(view, Str(S_CITY_MSG_ADDED));
        } else if (result == CITY_QUEUE_FULL) {
            set_message(view, Str(S_CITY_MSG_QUEUE_FULL));
        } else if (result == CITY_QUEUE_ALREADY) {
            set_message(view, Str(S_CITY_MSG_ALREADY_QUEUED));
        } else {
            set_message(view, Str(S_CITY_MSG_ALREADY_BUILT));
        }
    }
    if (Input_Pressed(PSP_CTRL_SQUARE)) {
        if (City_QueueRemoveLast(city) == 0) {
            set_message(view, Str(S_CITY_MSG_REMOVED));
        } else {
            set_message(view, Str(S_CITY_MSG_NO_QUEUE));
        }
    }
}

/* Rush needs the map, so keep the input handler separate from the list
 * navigation. */
static void update_rush(CityView *view, City *city, Map *map, World *world) {
    if (!Input_Pressed(PSP_CTRL_TRIANGLE)) return;
    int treasury = (int)world->treasury;
    int cost = City_RushCost(city);
    CityActionResult result =
        City_RushProduction(city, map, &world->units, &treasury);
    switch (result) {
        case CITY_ACTION_OK:
            world->treasury = treasury;
            set_message(view, Str(S_CITY_MSG_BOUGHT));
            Log_Printf("city: rush %s cost=%d treasury=%d",
                       Str(Data_CityName(city->name_index)), cost,
                       (int)world->treasury);
            break;
        case CITY_ACTION_NO_GOLD:
            set_message(view, Str(S_CITY_MSG_NO_GOLD));
            break;
        case CITY_ACTION_NO_SPACE:
            set_message(view, Str(S_CITY_MSG_NO_SPACE));
            break;
        default:
            set_message(view, Str(S_CITY_MSG_NO_QUEUE));
            break;
    }
}

static void update_citizens(CityView *view, City *city, Map *map,
                            World *world) {
    int row = view->tile_cursor / CITY_WORK_DIAMETER;
    int col = view->tile_cursor % CITY_WORK_DIAMETER;
    if (Input_Repeated(PSP_CTRL_UP) && row > 0) row--;
    if (Input_Repeated(PSP_CTRL_DOWN) && row < CITY_WORK_DIAMETER - 1) row++;
    if (Input_Repeated(PSP_CTRL_LEFT) && col > 0) col--;
    if (Input_Repeated(PSP_CTRL_RIGHT) && col < CITY_WORK_DIAMETER - 1) col++;
    view->tile_cursor = row * CITY_WORK_DIAMETER + col;

    int index = view->tile_cursor;
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        if (index == CITY_CENTER_INDEX) {
            set_message(view, Str(S_CITY_CENTER));
        } else if (!City_OwnsIndex(city, index)) {
            set_message(view, Str(S_CITY_UNOWNED));
        } else {
            int result = City_ToggleWork(city, map, index);
            if (result == CITY_ACTION_NO_IDLE) {
                set_message(view, Str(S_CITY_MSG_NO_IDLE));
            } else if (result != CITY_ACTION_OK) {
                set_message(view, Str(S_CITY_MSG_TILE_INVALID));
            }
        }
    }
    if (Input_Pressed(PSP_CTRL_TRIANGLE)) {
        if (index == CITY_CENTER_INDEX) {
            set_message(view, Str(S_CITY_CENTER));
        } else if (City_OwnsIndex(city, index)) {
            int result = City_ToggleLock(city, map, index);
            if (result == CITY_ACTION_NO_IDLE) {
                set_message(view, Str(S_CITY_MSG_NO_IDLE));
            }
        } else {
            int treasury = (int)world->treasury;
            CityTileResult result = City_BuyTile(city, map, &treasury, index);
            if (result == CITY_TILE_OK) {
                world->treasury = treasury;
                set_message(view, Str(S_CITY_MSG_TILE_BOUGHT));
                Log_Printf("city: buy tile at %d,%d treasury=%d",
                           city->x + index % CITY_WORK_DIAMETER
                               - CITY_WORK_RADIUS,
                           city->y + index / CITY_WORK_DIAMETER
                               - CITY_WORK_RADIUS,
                           (int)world->treasury);
            } else if (result == CITY_TILE_NO_GOLD) {
                set_message(view, Str(S_CITY_MSG_NO_GOLD));
            } else if (result == CITY_TILE_NOT_ADJACENT) {
                set_message(view, Str(S_CITY_MSG_TILE_FAR));
            } else if (result == CITY_TILE_OWNED) {
                set_message(view, Str(S_CITY_MSG_TILE_OWNED));
            } else {
                set_message(view, Str(S_CITY_MSG_TILE_INVALID));
            }
        }
    }
    if (Input_Pressed(PSP_CTRL_SQUARE)) {
        City_AutoAssign(city, map);
        set_message(view, Str(S_CITY_MSG_AUTO_ASSIGNED));
    }
}

static void update_buildings(CityView *view, City *city, World *world) {
    if (Input_Repeated(PSP_CTRL_UP)) {
        view->building_cursor =
            (view->building_cursor + BUILDING_COUNT - 1) % BUILDING_COUNT;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        view->building_cursor = (view->building_cursor + 1) % BUILDING_COUNT;
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        ProductionType type = Data_ProductionForBuilding(
            (BuildingType)view->building_cursor);
        const ProductionDef *def = Data_BuildingDef(
            (BuildingType)view->building_cursor);
        if (def && def->required_tech != TECH_NONE
            && !Tech_IsKnown(world->researched, def->required_tech)) {
            set_message(view, Str(S_TECH_REQUIRES));
            return;
        }
        CityQueueResult result = City_QueueAdd(city, type);
        if (result == CITY_QUEUE_OK) {
            set_message(view, Str(S_CITY_MSG_ADDED));
        } else if (result == CITY_QUEUE_FULL) {
            set_message(view, Str(S_CITY_MSG_QUEUE_FULL));
        } else if (result == CITY_QUEUE_ALREADY) {
            set_message(view, City_HasBuilding(city,
                (BuildingType)view->building_cursor)
                       ? Str(S_CITY_MSG_ALREADY_BUILT)
                       : Str(S_CITY_MSG_ALREADY_QUEUED));
        } else {
            set_message(view, Str(S_CITY_MSG_ALREADY_BUILT));
        }
    }
}

int CityView_Update(CityView *view, Map *map, World *world) {
    if (!view || !map || !world) return 1;
    if (view->city_index < 0 || view->city_index >= world->cities.count) {
        return 1;
    }
    City *city = &world->cities.cities[view->city_index];
    if (city->owner != WORLD_CIV_PLAYER) return 1;
    if (view->message_timer > 0) view->message_timer--;

    if (Input_Pressed(PSP_CTRL_CIRCLE)) return 1;

    if (Input_Pressed(PSP_CTRL_LTRIGGER)) {
        view->tab = (CityTab)(((int)view->tab + CITY_TAB_COUNT - 1)
                              % CITY_TAB_COUNT);
        return 0;
    }
    if (Input_Pressed(PSP_CTRL_RTRIGGER)) {
        view->tab = (CityTab)(((int)view->tab + 1) % CITY_TAB_COUNT);
        return 0;
    }

    if (Input_Pressed(PSP_CTRL_SELECT)) {
        int old_pop = city->population;
        int old_owned = City_OwnedCount(city);
        int old_buildings = City_BuildingCount(city);
        int old_units = world->units.count;
        uint32_t old_researched = world->researched;
        int old_science = (int)world->science;
        World_EndTurn(world, map);
        if (city->owner != WORLD_CIV_PLAYER) return 1;
        char msg[80];
        snprintf(msg, sizeof(msg), "%s %d", Str(S_CITY_MSG_TURN),
                 world->turn);
        if (city->population > old_pop) {
            strncat(msg, "  ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, Str(S_CITY_MSG_GREW), sizeof(msg) - strlen(msg) - 1);
        } else if (city->population < old_pop) {
            strncat(msg, "  ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, Str(S_CITY_MSG_STARVED),
                    sizeof(msg) - strlen(msg) - 1);
        }
        if (City_OwnedCount(city) > old_owned) {
            strncat(msg, "  ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, Str(S_CITY_MSG_BORDER_GREW),
                    sizeof(msg) - strlen(msg) - 1);
        }
        int produced = City_BuildingCount(city) > old_buildings
                       || world->units.count > old_units;
        if (produced) {
            strncat(msg, "  ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, Str(S_CITY_MSG_PRODUCTION),
                    sizeof(msg) - strlen(msg) - 1);
        }
        if (world->researched != old_researched) {
            strncat(msg, "  ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, Str(S_TECH_COMPLETE),
                    sizeof(msg) - strlen(msg) - 1);
            Log_Printf("tech: completed mask=%08X science=%d->%d",
                       (unsigned)world->researched, old_science,
                       (int)world->science);
        }
        if (world->researched != old_researched) {
            Audio_PlaySfx(AUDIO_SFX_RESEARCH);
        } else if (produced) {
            Audio_PlaySfx(AUDIO_SFX_BUILD);
        }
        set_message(view, msg);
        Log_Printf("turn: %d (city) pop=%d owned=%d units=%d treasury=%d "
                   "science=%d",
                   world->turn, city->population, City_OwnedCount(city),
                   world->units.count, (int)world->treasury,
                   (int)world->science);
        if (city->population > old_pop) {
            Log_Printf("city: grew to %d", city->population);
        }
        if (City_OwnedCount(city) > old_owned) {
            Log_Printf("city: border expanded to %d tiles",
                       City_OwnedCount(city));
        }
        return 0;
    }

    switch (view->tab) {
        case CITY_TAB_PRODUCTION:
            update_production(view, city, world);
            update_rush(view, city, map, world);
            break;
        case CITY_TAB_CITIZENS:
            update_citizens(view, city, map, world);
            break;
        case CITY_TAB_BUILDINGS:
            update_buildings(view, city, world);
            break;
        case CITY_TAB_SCIENCE:
            update_science(view, world);
            break;
        default:
            break;
    }
    return 0;
}

void CityView_Render(const CityView *view, const Map *map,
                     const World *world) {
    const City *city = get_city(view, world);
    if (!city) return;

    Render_SetScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    draw_backgrounds();
    Render_FlushLayer();

    draw_grid_sprites(view, map, world);
    draw_selection_bars(view, world);
    Render_FlushLayer();

    draw_top_text(world, city);
    draw_tab_text(view);
    draw_grid_text(view, map, world);
    draw_panel_text(view, map, world);
    draw_footer(view);
    Render_FlushLayer();

    draw_message(view);
}
