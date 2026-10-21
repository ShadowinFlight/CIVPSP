#include "app.h"

#include <pspctrl.h>
#include <pspkernel.h>

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "cityview.h"
#include "diplomacyview.h"
#include "game.h"
#include "gen/strings_gen.h"
#include "input.h"
#include "log.h"
#include "map.h"
#include "mapview.h"
#include "render.h"
#include "rng.h"
#include "save.h"
#include "text.h"
#include "world.h"

typedef enum {
    STATE_MAIN_MENU = 0,
    STATE_OPTIONS,
    STATE_HELP,
    STATE_NEW_GAME,
    STATE_MAP,
    STATE_CITY,
    STATE_DIPLOMACY,
    STATE_PAUSE,
    STATE_SAVE_MENU,
    STATE_LOAD_MENU,
    STATE_GAME_OVER,
    STATE_QUIT
} AppState;

typedef struct {
    AppState state;
    int selection;
    int toast_timer;
    const char *toast;
    unsigned int frame;
    int state_anim;
    int selection_anim;
    int last_selection;
    Map map;
    World world;
    MapView map_view;
    CityView city_view;
    DiplomacyView diplomacy_view;

    /* M7 game mode and wall-clock timer. */
    SaveMode mode;
    uint32_t limit_seconds;
    uint32_t remaining_ms;
    uint32_t play_ms;
    uint32_t clock_remainder_us;
    uint32_t last_tick_us;
    SaveMode new_mode;
    int new_time_index;
    int load_from_pause;

    int help_page;
    AppState help_return;

    SaveHeader slot_header[SAVE_SLOT_LAST + 1];
    SaveResult slot_status[SAVE_SLOT_LAST + 1];
} App;

static App app;

#define COLOR_BG        0xFF0E1820u
#define COLOR_BG_TILE_A 0xFF12222Bu
#define COLOR_BG_TILE_B 0xFF152730u
#define COLOR_PANEL     0xFF1C313Au
#define COLOR_PANEL_SEL 0xFF2B4749u
#define COLOR_BORDER    0xFF3E5A5Cu
#define COLOR_GOLD      0xFFD8B25Cu
#define COLOR_TEXT      0xFFF5EEDCu
#define COLOR_MUTED     0xFF9FB0A8u
#define COLOR_DARK      0xFF0A1116u

#define MENU_FADE_FRAMES   20
#define MENU_SLIDE_FRAMES  10
#define MENU_SLIDE_DELAY   2
#define MENU_SLIDE_PX      14.0f
#define MENU_GLOW_FRAMES   8

#define TIME_CHOICE_COUNT 3
static const uint32_t time_choices[TIME_CHOICE_COUNT] = {
    30u * 60u, 60u * 60u, 90u * 60u
};

static void show_toast(const char *text) {
    app.toast = text;
    app.toast_timer = 180;
}

static uint32_t remaining_seconds(void) {
    return (app.remaining_ms + 999u) / 1000u;
}

static void sync_map_clock(void) {
    app.map_view.time_active = app.mode == SAVE_MODE_TIMED;
    app.map_view.time_remaining_seconds = remaining_seconds();
    app.map_view.turn_limit = app.mode == SAVE_MODE_STANDARD
                                  ? WORLD_STANDARD_TURN_LIMIT : 0;
}

static SaveView capture_view(void) {
    SaveView view;
    view.cursor_x = app.map_view.cursor_x;
    view.cursor_y = app.map_view.cursor_y;
    view.selected = app.map_view.selected;
    view.cam_x = app.map_view.cam_x;
    view.cam_y = app.map_view.cam_y;
    return view;
}

static void enter_game_over(void) {
    app.state = STATE_GAME_OVER;
    Log_Printf("game over: result=%d type=%d turn=%d score=%d rival_best=%d "
               "play_seconds=%u",
               app.world.game_over, app.world.victory_type, app.world.turn,
               World_PlayerScore(&app.world),
               World_BestRivalScore(&app.world), app.play_ms / 1000u);
}

static void finish_timed_game(void) {
    if (app.world.game_over != WORLD_GAME_NONE) return;
    World_FinishScoreGame(&app.world);
    Log_Printf("time up: score=%d rival_best=%d result=%d",
               World_PlayerScore(&app.world),
               World_BestRivalScore(&app.world), app.world.game_over);
    enter_game_over();
}

static void finish_standard_game(void) {
    if (app.world.game_over != WORLD_GAME_NONE) return;
    World_FinishScoreGame(&app.world);
    Log_Printf("turn limit: turn=%d score=%d rival_best=%d result=%d",
               app.world.turn, World_PlayerScore(&app.world),
               World_BestRivalScore(&app.world), app.world.game_over);
    enter_game_over();
}

static void update_game_clock(void) {
    uint32_t now = (uint32_t)sceKernelGetSystemTimeLow();
    uint32_t delta = now - app.last_tick_us;
    app.last_tick_us = now;

    if (app.state != STATE_MAP && app.state != STATE_CITY
        && app.state != STATE_DIPLOMACY) {
        return;
    }
    if (app.world.game_over != WORLD_GAME_NONE) return;

    app.clock_remainder_us += delta;
    uint32_t elapsed_ms = app.clock_remainder_us / 1000u;
    app.clock_remainder_us %= 1000u;
    if (elapsed_ms == 0) return;

    app.play_ms += elapsed_ms;
    if (app.mode == SAVE_MODE_TIMED) {
        if (elapsed_ms >= app.remaining_ms) {
            app.remaining_ms = 0;
        } else {
            app.remaining_ms -= elapsed_ms;
        }
        sync_map_clock();
        if (app.remaining_ms == 0) {
            finish_timed_game();
        }
    }
}

static void start_new_game(SaveMode mode, uint32_t limit_seconds) {
    uint32_t seed = Rng_Next();
    Map_Generate(&app.map, seed);
    World_Begin(&app.world, &app.map);
    Input_CalibrateStick();
    MapView_Init(&app.map_view, &app.map, &app.world);

    app.mode = mode;
    app.limit_seconds = mode == SAVE_MODE_TIMED ? limit_seconds : 0;
    app.remaining_ms = mode == SAVE_MODE_TIMED ? limit_seconds * 1000u : 0;
    app.play_ms = 0;
    app.clock_remainder_us = 0;
    app.last_tick_us = (uint32_t)sceKernelGetSystemTimeLow();
    app.state = STATE_MAP;
    app.selection = 0;
    sync_map_clock();

    Log_Printf("new game: seed=%08X start=%d,%d units=%d cities=%d "
               "treasury=%d science=%d mode=%s limit=%u analog=%d,%d "
               "center=%d,%d dz=%d",
               (unsigned)seed, app.map.start_x, app.map.start_y,
               app.world.units.count, app.world.cities.count,
               (int)app.world.treasury, (int)app.world.science,
               mode == SAVE_MODE_TIMED ? "timed" : "standard",
               app.limit_seconds, g_input.analog_x, g_input.analog_y,
               g_input.analog_center_x, g_input.analog_center_y,
               g_input.analog_deadzone);
    for (int i = 0; i < app.world.units.count; i++) {
        const Unit *unit = &app.world.units.units[i];
        Log_Printf("world: unit %d type=%d at %d,%d moves=%d", i,
                   unit->type, unit->x, unit->y, unit->moves_left);
    }
}

static SaveResult write_game_slot(int slot) {
    SaveView view = capture_view();
    SaveResult result = Save_WriteSlot(
        slot, &app.map, &app.world, &view, (uint32_t)app.mode,
        remaining_seconds(), app.play_ms / 1000u, app.limit_seconds,
        (uint32_t)World_PlayerScore(&app.world), Rng_GetState());
    if (result == SAVE_OK) {
        Log_Printf("save: write slot=%d turn=%d mode=%u score=%d",
                   slot, app.world.turn, (unsigned)app.mode,
                   World_PlayerScore(&app.world));
    } else {
        Log_Printf("save: write failed slot=%d result=%d", slot, result);
    }
    return result;
}

static void refresh_slot_headers(void) {
    for (int slot = 0; slot <= SAVE_SLOT_LAST; slot++) {
        memset(&app.slot_header[slot], 0, sizeof(app.slot_header[slot]));
        app.slot_status[slot] = Save_PeekSlot(slot, &app.slot_header[slot]);
    }
}

static int load_game_slot(int slot) {
    SaveView view;
    SaveHeader header;
    SaveResult result = Save_ReadSlot(slot, &app.map, &app.world, &view,
                                      &header);
    if (result != SAVE_OK) {
        Log_Printf("save: load failed slot=%d result=%d", slot, result);
        show_toast(result == SAVE_ERR_OPEN ? Str(S_EMPTY_SLOT)
                                           : Str(S_LOAD_FAILED));
        return 0;
    }

    Rng_SetState(header.rng_state);
    app.mode = header.mode == SAVE_MODE_TIMED ? SAVE_MODE_TIMED
                                              : SAVE_MODE_STANDARD;
    app.limit_seconds = header.limit_seconds;
    app.remaining_ms = header.remaining_seconds * 1000u;
    app.play_ms = header.play_seconds * 1000u;
    app.clock_remainder_us = 0;
    app.last_tick_us = (uint32_t)sceKernelGetSystemTimeLow();
    MapView_Restore(&app.map_view, &app.map, &app.world,
                    view.cursor_x, view.cursor_y, view.selected,
                    view.cam_x, view.cam_y);
    app.map_view.turn_ended = 0;
    app.load_from_pause = 0;
    app.selection = 0;
    sync_map_clock();

    Log_Printf("save: load slot=%d turn=%d mode=%u score=%u time=%u",
               slot, header.turn, (unsigned)header.mode,
               header.player_score, header.remaining_seconds);
    if (app.world.game_over != WORLD_GAME_NONE) {
        enter_game_over();
    } else {
        app.state = STATE_MAP;
    }
    return 1;
}

static void open_pause(void) {
    app.state = STATE_PAUSE;
    app.selection = 0;
    Log_Printf("pause: open turn=%d", app.world.turn);
}

static void open_save_menu(void) {
    refresh_slot_headers();
    app.state = STATE_SAVE_MENU;
    app.selection = 0;
    Log_Printf("save: menu open");
}

static void open_load_menu(int from_pause) {
    refresh_slot_headers();
    app.load_from_pause = from_pause;
    app.state = STATE_LOAD_MENU;
    app.selection = 0;
    Log_Printf("load: menu open from_pause=%d", from_pause);
}

#define HELP_PAGE_COUNT 12

static void open_help(int from_pause) {
    app.help_page = 0;
    app.help_return = from_pause ? STATE_PAUSE : STATE_MAIN_MENU;
    app.state = STATE_HELP;
    Log_Printf("help: open from_pause=%d", from_pause);
}

static void update_help(void) {
    if (Input_Pressed(PSP_CTRL_LEFT)) {
        app.help_page = (app.help_page + HELP_PAGE_COUNT - 1)
                      % HELP_PAGE_COUNT;
    }
    if (Input_Pressed(PSP_CTRL_RIGHT) || Input_Pressed(PSP_CTRL_CROSS)) {
        app.help_page = (app.help_page + 1) % HELP_PAGE_COUNT;
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        app.state = app.help_return;
        app.selection = 3;
        Log_Printf("help: close page=%d", app.help_page);
    }
}

static void update_map(void) {
    if (MapView_Update(&app.map_view, &app.map, &app.world)) {
        open_pause();
        return;
    }
    if (app.map_view.turn_ended) {
        app.map_view.turn_ended = 0;
        if (app.mode == SAVE_MODE_STANDARD
            && app.world.turn > WORLD_STANDARD_TURN_LIMIT
            && app.world.game_over == WORLD_GAME_NONE) {
            finish_standard_game();
        }
        (void)write_game_slot(SAVE_SLOT_AUTO);
        if (app.state == STATE_GAME_OVER) return;
    }
    if (app.world.game_over != WORLD_GAME_NONE) {
        enter_game_over();
        return;
    }
    if (app.map_view.open_diplomacy) {
        app.map_view.open_diplomacy = 0;
        DiplomacyView_Init(&app.diplomacy_view, &app.world);
        app.state = STATE_DIPLOMACY;
        Log_Printf("diplo: open screen turn=%d rivals=%d",
                   app.world.turn, World_LivingRivals(&app.world));
        return;
    }
    if (app.map_view.open_city > 0) {
        int index = app.map_view.open_city - 1;
        app.map_view.open_city = 0;
        if (index >= 0 && index < app.world.cities.count
            && app.world.cities.cities[index].owner == WORLD_CIV_PLAYER) {
            app.city_view.city_index = index;
            CityView_Init(&app.city_view, index);
            app.state = STATE_CITY;
            const City *city = &app.world.cities.cities[index];
            Log_Printf("city: open %s index=%d pop=%d owned=%d",
                       Str(Data_CityName(city->name_index)), index,
                       city->population, City_OwnedCount(city));
        }
    }
}

static void update_city(void) {
    int turn_before = app.world.turn;
    if (CityView_Update(&app.city_view, &app.map, &app.world)) {
        app.state = STATE_MAP;
        Log_Printf("city: back to map");
    }
    if (app.world.turn != turn_before) {
        /* Ending a turn from the city screen must use the same autosave and
         * standard-mode turn-limit path as ending it from the map. */
        if (app.mode == SAVE_MODE_STANDARD
            && app.world.turn > WORLD_STANDARD_TURN_LIMIT
            && app.world.game_over == WORLD_GAME_NONE) {
            finish_standard_game();
        }
        (void)write_game_slot(SAVE_SLOT_AUTO);
        if (app.state == STATE_GAME_OVER) return;
    }
    if (app.world.game_over != WORLD_GAME_NONE) {
        enter_game_over();
    }
}

static void update_diplomacy(void) {
    if (DiplomacyView_Update(&app.diplomacy_view, &app.map, &app.world)) {
        app.state = STATE_MAP;
        Log_Printf("diplo: back to map");
    }
    if (app.world.game_over != WORLD_GAME_NONE) {
        enter_game_over();
    }
}

static void update_game_over(void) {
    if (Input_Pressed(PSP_CTRL_CROSS) || Input_Pressed(PSP_CTRL_CIRCLE)) {
        app.state = STATE_MAIN_MENU;
        app.selection = 0;
        Log_Printf("game over: return to menu");
    }
}

static void update_main_menu(void) {
    if (Input_Repeated(PSP_CTRL_UP)) {
        app.selection = (app.selection + 4) % 5;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        app.selection = (app.selection + 1) % 5;
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        switch (app.selection) {
            case 0:
                app.selection = 0;
                app.new_mode = SAVE_MODE_STANDARD;
                app.new_time_index = 1;
                app.state = STATE_NEW_GAME;
                Log_Printf("menu: new game setup");
                break;
            case 1:
                open_load_menu(0);
                break;
            case 2:
                app.selection = 0;
                app.state = STATE_OPTIONS;
                Log_Printf("menu: options");
                break;
            case 3:
                open_help(0);
                break;
            default:
                Log_Printf("menu: quit");
                app.state = STATE_QUIT;
                break;
        }
    }
}

static void set_language(int language) {
    Str_SetLanguage(language);
    Render_SetFontLanguage(language);
}

static void toggle_language(void) {
    set_language(Str_GetLanguage() == LANG_ZH ? LANG_EN : LANG_ZH);
    Log_Printf("options: language=%s",
               Str_GetLanguage() == LANG_ZH ? "zh" : "en");
}

static void toggle_music(void) {
    Audio_SetMusicEnabled(!Audio_MusicEnabled());
    Log_Printf("options: music=%s", Audio_MusicEnabled() ? "on" : "off");
}

static void update_options(void) {
    if (Input_Repeated(PSP_CTRL_UP)) {
        app.selection = (app.selection + 3) % 4;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        app.selection = (app.selection + 1) % 4;
    }
    if (Input_Repeated(PSP_CTRL_LEFT) || Input_Repeated(PSP_CTRL_RIGHT)) {
        if (app.selection == 0) toggle_language();
        else if (app.selection == 1) toggle_music();
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        if (app.selection == 0) {
            toggle_language();
        } else if (app.selection == 1) {
            toggle_music();
        } else if (app.selection == 2) {
            Input_CalibrateStick();
            show_toast(Str(S_OPTIONS_CALIBRATED));
            Log_Printf("options: recalibrate stick");
        } else {
            app.selection = 0;
            app.state = STATE_MAIN_MENU;
            Log_Printf("options: back");
        }
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        app.selection = 0;
        app.state = STATE_MAIN_MENU;
        Log_Printf("options: back");
    }
}

static void update_new_game(void) {
    if (Input_Repeated(PSP_CTRL_UP)) {
        app.selection = (app.selection + 3) % 4;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        app.selection = (app.selection + 1) % 4;
    }
    if (Input_Repeated(PSP_CTRL_LEFT) || Input_Repeated(PSP_CTRL_RIGHT)) {
        int dir = Input_Repeated(PSP_CTRL_RIGHT) ? 1 : -1;
        if (app.selection == 0) {
            app.new_mode = app.new_mode == SAVE_MODE_STANDARD
                               ? SAVE_MODE_TIMED : SAVE_MODE_STANDARD;
        } else if (app.selection == 1 && app.new_mode == SAVE_MODE_TIMED) {
            app.new_time_index = (app.new_time_index + dir
                                  + TIME_CHOICE_COUNT) % TIME_CHOICE_COUNT;
        }
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        switch (app.selection) {
            case 0:
                app.new_mode = app.new_mode == SAVE_MODE_STANDARD
                                   ? SAVE_MODE_TIMED : SAVE_MODE_STANDARD;
                break;
            case 1:
                if (app.new_mode == SAVE_MODE_TIMED) {
                    app.new_time_index = (app.new_time_index + 1)
                                         % TIME_CHOICE_COUNT;
                }
                break;
            case 2: {
                uint32_t limit = app.new_mode == SAVE_MODE_TIMED
                                     ? time_choices[app.new_time_index] : 0;
                start_new_game(app.new_mode, limit);
                break;
            }
            default:
                app.selection = 0;
                app.state = STATE_MAIN_MENU;
                Log_Printf("new game: back");
                break;
        }
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        app.selection = 0;
        app.state = STATE_MAIN_MENU;
        Log_Printf("new game: back");
    }
}

static void update_pause(void) {
    if (Input_Repeated(PSP_CTRL_UP)) {
        app.selection = (app.selection + 5) % 6;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        app.selection = (app.selection + 1) % 6;
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        app.state = STATE_MAP;
        Log_Printf("pause: resume");
        return;
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        switch (app.selection) {
            case 0:
                app.state = STATE_MAP;
                Log_Printf("pause: resume");
                break;
            case 1:
                open_save_menu();
                break;
            case 2:
                open_load_menu(1);
                break;
            case 3:
                open_help(1);
                break;
            case 4:
                Input_CalibrateStick();
                show_toast(Str(S_OPTIONS_CALIBRATED));
                Log_Printf("pause: recalibrate stick");
                break;
            default:
                app.selection = 0;
                app.state = STATE_MAIN_MENU;
                Log_Printf("pause: main menu");
                break;
        }
    }
}

static void update_save_menu(void) {
    if (Input_Repeated(PSP_CTRL_UP)) {
        app.selection = (app.selection + 2) % 3;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        app.selection = (app.selection + 1) % 3;
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        app.selection = 1;
        app.state = STATE_PAUSE;
        Log_Printf("save: menu cancel");
        return;
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        int slot = app.selection + SAVE_SLOT_FIRST;
        if (write_game_slot(slot) == SAVE_OK) {
            show_toast(Str(S_SAVE_OK));
            app.selection = 1;
            app.state = STATE_PAUSE;
        } else {
            show_toast(Str(S_SAVE_FAILED));
        }
    }
}

static void update_load_menu(void) {
    if (Input_Repeated(PSP_CTRL_UP)) {
        app.selection = (app.selection + 3) % 4;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        app.selection = (app.selection + 1) % 4;
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        app.selection = app.load_from_pause ? 2 : 0;
        app.state = app.load_from_pause ? STATE_PAUSE : STATE_MAIN_MENU;
        Log_Printf("load: menu cancel");
        return;
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        if (app.slot_status[app.selection] != SAVE_OK) {
            show_toast(app.slot_status[app.selection] == SAVE_ERR_OPEN
                           ? Str(S_EMPTY_SLOT) : Str(S_SLOT_CORRUPT));
            return;
        }
        (void)load_game_slot(app.selection);
    }
}

void App_Init(void) {
    memset(&app, 0, sizeof(app));
    app.state = STATE_MAIN_MENU;
    app.selection = 0;
    app.new_mode = SAVE_MODE_STANDARD;
    app.new_time_index = 1;
    app.mode = SAVE_MODE_STANDARD;
    app.state_anim = 0;
    app.selection_anim = 0;
    app.last_selection = 0;
    app.last_tick_us = (uint32_t)sceKernelGetSystemTimeLow();
    set_language(LANG_ZH);
    Save_Init();
    Render_SetClearColor(COLOR_BG);
    Log_Printf("app init: main menu");
}

int App_ShouldQuit(void) {
    return app.state == STATE_QUIT;
}

static void update_ui_audio(void) {
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        Audio_PlaySfx(AUDIO_SFX_BACK);
    } else if (Input_Pressed(PSP_CTRL_CROSS | PSP_CTRL_START)) {
        Audio_PlaySfx(AUDIO_SFX_SELECT);
    } else if (Input_Pressed(PSP_CTRL_SELECT)) {
        Audio_PlaySfx(AUDIO_SFX_TURN);
    } else if (Input_Pressed(PSP_CTRL_UP | PSP_CTRL_DOWN
                             | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
        Audio_PlaySfx(AUDIO_SFX_MOVE);
    }
}

void App_Update(void) {
    AppState before = app.state;
    app.frame++;
    if (app.toast_timer > 0) app.toast_timer--;
    update_game_clock();
    update_ui_audio();

    switch (app.state) {
        case STATE_MAIN_MENU:
            update_main_menu();
            break;
        case STATE_OPTIONS:
            update_options();
            break;
        case STATE_HELP:
            update_help();
            break;
        case STATE_NEW_GAME:
            update_new_game();
            break;
        case STATE_MAP:
            update_map();
            break;
        case STATE_CITY:
            update_city();
            break;
        case STATE_DIPLOMACY:
            update_diplomacy();
            break;
        case STATE_PAUSE:
            update_pause();
            break;
        case STATE_SAVE_MENU:
            update_save_menu();
            break;
        case STATE_LOAD_MENU:
            update_load_menu();
            break;
        case STATE_GAME_OVER:
            update_game_over();
            break;
        default:
            break;
    }

    if (app.state != before) {
        app.state_anim = 0;
    } else if (app.state_anim < 60) {
        app.state_anim++;
    }
    if (app.selection != app.last_selection) {
        app.selection_anim = MENU_GLOW_FRAMES;
        app.last_selection = app.selection;
    } else if (app.selection_anim > 0) {
        app.selection_anim--;
    }
}

static void draw_background(void) {
    for (int y = 0; y < SCREEN_HEIGHT; y += 24) {
        for (int x = 0; x < SCREEN_WIDTH; x += 24) {
            unsigned int color = (((x / 24) + (y / 24)) & 1)
                                     ? COLOR_BG_TILE_A
                                     : COLOR_BG_TILE_B;
            Render_DrawRect((float)x, (float)y, 24.0f, 24.0f, color);
        }
    }
    Render_DrawRect(0, 0, SCREEN_WIDTH, 64, 0xCC0A1116u);
    Render_DrawRect(0, SCREEN_HEIGHT - 42, SCREEN_WIDTH, 42, 0xCC0A1116u);
    Render_DrawRect(0, 63, SCREEN_WIDTH, 1, COLOR_GOLD);
    Render_DrawRect(0, SCREEN_HEIGHT - 43, SCREEN_WIDTH, 1, 0x663E5A5C);
}

static float menu_slide(int index) {
    int t = app.state_anim - index * MENU_SLIDE_DELAY;
    if (t <= 0) return MENU_SLIDE_PX;
    if (t >= MENU_SLIDE_FRAMES) return 0.0f;
    return MENU_SLIDE_PX * (1.0f - (float)t / (float)MENU_SLIDE_FRAMES);
}

static unsigned int selection_glow_color(void) {
    if (app.selection_anim <= 0) return 0;
    unsigned int alpha = (unsigned int)app.selection_anim * 12u;
    if (alpha > 96u) alpha = 96u;
    return (alpha << 24) | (COLOR_GOLD & 0x00FFFFFFu);
}

static void draw_menu_item(int index, const char *label, float y) {
    const float w = SCREEN_WIDTH - 184.0f;
    const float h = 25.0f;
    float slide = menu_slide(index);
    const float x = 92.0f + slide;
    int selected = (app.selection == index);
    unsigned int glow = selected ? selection_glow_color() : 0;

    if (selected) {
        if (glow) Render_DrawRect(x - 5, y - 4, w + 10, h + 8, glow);
        Render_DrawRect(x - 3, y - 3, w + 6, h + 6, 0x553E5A5C);
        Render_DrawRect(x, y, w, h, COLOR_PANEL_SEL);
        Render_DrawRect(x, y, 4, h, COLOR_GOLD);
    } else {
        Render_DrawRect(x, y, w, h, COLOR_PANEL);
        Render_DrawRect(x, y, w, 1, 0x553E5A5C);
        Render_DrawRect(x, y + h - 1, w, 1, 0x553E5A5C);
    }

    Text_DrawCentered(label, SCREEN_WIDTH / 2.0f + slide,
                      y + (h - Text_Height(1)) / 2.0f - 1, 1,
                      selected ? COLOR_TEXT : COLOR_MUTED);
}

static void draw_value_row(int index, const char *label, const char *value,
                           float y) {
    const float w = SCREEN_WIDTH - 156.0f;
    const float h = 24.0f;
    float slide = menu_slide(index);
    const float x = 78.0f + slide;
    int selected = (app.selection == index);
    unsigned int glow = selected ? selection_glow_color() : 0;

    if (glow) Render_DrawRect(x - 5, y - 4, w + 10, h + 8, glow);
    Render_DrawRect(x, y, w, h, selected ? COLOR_PANEL_SEL : COLOR_PANEL);
    Render_DrawRect(x, y, w, 1, 0x553E5A5C);
    Render_DrawRect(x, y + h - 1, w, 1, 0x553E5A5C);
    if (selected) Render_DrawRect(x, y, 4, h, COLOR_GOLD);
    Text_Draw(label, x + 10, y + 5, 1, selected ? COLOR_TEXT : COLOR_MUTED);
    Text_DrawRight(value, x + w - 10, y + 5, 1,
                   selected ? COLOR_GOLD : COLOR_MUTED);
}

static void draw_toast(void) {
    if (app.toast_timer <= 0 || !app.toast) return;

    Render_FlushLayer();
    int w = Text_Width(app.toast, 1) + 28;
    int h = Text_Height(1) + 12;
    float x = (SCREEN_WIDTH - w) / 2.0f;
    float y = SCREEN_HEIGHT - 68.0f;
    Render_DrawRect(x, y, (float)w, (float)h, 0xEE0A1116u);
    Render_DrawRect(x, y, (float)w, 1, COLOR_GOLD);
    Render_DrawRect(x, y + h - 1, (float)w, 1, COLOR_GOLD);
    Text_DrawCentered(app.toast, SCREEN_WIDTH / 2.0f, y + 6, 1, COLOR_TEXT);
}

static void draw_slot_row(int index, int slot, float y) {
    const float w = SCREEN_WIDTH - 128.0f;
    const float h = 28.0f;
    float slide = menu_slide(index);
    const float x = 64.0f + slide;
    int selected = (app.selection == index);
    unsigned int glow = selected ? selection_glow_color() : 0;
    char details[96];
    const char *label = Str(S_SLOT_AUTO);

    if (slot == 1) label = Str(S_SLOT_1);
    else if (slot == 2) label = Str(S_SLOT_2);
    else if (slot == 3) label = Str(S_SLOT_3);

    if (app.slot_status[slot] == SAVE_OK) {
        const SaveHeader *header = &app.slot_header[slot];
        if (header->mode == SAVE_MODE_TIMED) {
            unsigned int sec = header->remaining_seconds;
            snprintf(details, sizeof(details), "%s %u  %s %u  %02u:%02u",
                     Str(S_MAP_TURN), (unsigned)header->turn,
                     Str(S_GAME_RESULT_SCORE), (unsigned)header->player_score,
                     sec / 60u, sec % 60u);
        } else {
            snprintf(details, sizeof(details), "%s %u  %s %u",
                     Str(S_MAP_TURN), (unsigned)header->turn,
                     Str(S_GAME_RESULT_SCORE), (unsigned)header->player_score);
        }
    } else if (app.slot_status[slot] == SAVE_ERR_OPEN) {
        snprintf(details, sizeof(details), "%s", Str(S_SLOT_EMPTY));
    } else {
        snprintf(details, sizeof(details), "%s", Str(S_SLOT_CORRUPT));
    }

    if (glow) Render_DrawRect(x - 5, y - 4, w + 10, h + 8, glow);
    Render_DrawRect(x, y, w, h, selected ? COLOR_PANEL_SEL : COLOR_PANEL);
    Render_DrawRect(x, y, w, 1, 0x553E5A5C);
    Render_DrawRect(x, y + h - 1, w, 1, 0x553E5A5C);
    if (selected) Render_DrawRect(x, y, 4, h, COLOR_GOLD);
    Text_Draw(label, x + 10, y + 6, 1, selected ? COLOR_TEXT : COLOR_MUTED);
    Text_DrawRight(details, x + w - 10, y + 6, 1,
                   selected ? COLOR_GOLD : COLOR_MUTED);
}

typedef struct {
    StrId title;
    StrId lines[5];
} HelpPage;

static const HelpPage help_pages[HELP_PAGE_COUNT] = {
    {
        S_HELP_1_TITLE,
        { S_HELP_1_A, S_HELP_1_B, S_HELP_1_C, S_HELP_1_D, S_HELP_1_E }
    },
    {
        S_HELP_2_TITLE,
        { S_HELP_2_A, S_HELP_2_B, S_HELP_2_C, S_HELP_2_D, S_HELP_2_E }
    },
    {
        S_HELP_3_TITLE,
        { S_HELP_3_A, S_HELP_3_B, S_HELP_3_C, S_HELP_3_D, S_HELP_3_E }
    },
    {
        S_HELP_4_TITLE,
        { S_HELP_4_A, S_HELP_4_B, S_HELP_4_C, S_HELP_4_D, S_HELP_4_E }
    },
    {
        S_HELP_5_TITLE,
        { S_HELP_5_A, S_HELP_5_B, S_HELP_5_C, S_HELP_5_D, S_HELP_5_E }
    },
    {
        S_HELP_6_TITLE,
        { S_HELP_6_A, S_HELP_6_B, S_HELP_6_C, S_HELP_6_D, S_HELP_6_E }
    },
    {
        S_HELP_7_TITLE,
        { S_HELP_7_A, S_HELP_7_B, S_HELP_7_C, S_HELP_7_D, S_HELP_7_E }
    },
    {
        S_HELP_8_TITLE,
        { S_HELP_8_A, S_HELP_8_B, S_HELP_8_C, S_HELP_8_D, S_HELP_8_E }
    },
    {
        S_HELP_9_TITLE,
        { S_HELP_9_A, S_HELP_9_B, S_HELP_9_C, S_HELP_9_D, S_HELP_9_E }
    },
    {
        S_HELP_10_TITLE,
        { S_HELP_10_A, S_HELP_10_B, S_HELP_10_C, S_HELP_10_D, S_HELP_10_E }
    },
    {
        S_HELP_11_TITLE,
        { S_HELP_11_A, S_HELP_11_B, S_HELP_11_C, S_HELP_11_D, S_HELP_11_E }
    },
    {
        S_HELP_12_TITLE,
        { S_HELP_12_A, S_HELP_12_B, S_HELP_12_C, S_HELP_12_D, S_HELP_12_E }
    }
};

static void render_help(void) {
    draw_background();
    Text_DrawCentered(Str(S_HELP_TITLE), SCREEN_WIDTH / 2.0f, 32, 2,
                      COLOR_GOLD);
    Render_DrawRect(60, 66, SCREEN_WIDTH - 120, 1, 0x663E5A5C);

    Text_DrawCentered(Str(help_pages[app.help_page].title),
                      SCREEN_WIDTH / 2.0f, 76, 1, COLOR_TEXT);
    Render_DrawRect(80, 94, SCREEN_WIDTH - 160, 1, COLOR_BORDER);
    for (int i = 0; i < 5; i++) {
        Text_DrawCentered(Str(help_pages[app.help_page].lines[i]),
                          SCREEN_WIDTH / 2.0f, 108.0f + (float)i * 18.0f, 1,
                          i == 0 ? COLOR_GOLD : COLOR_MUTED);
    }

    char page[32];
    snprintf(page, sizeof(page), Str(S_HELP_PAGE), app.help_page + 1,
             HELP_PAGE_COUNT);
    Text_DrawCentered(page, SCREEN_WIDTH / 2.0f, 210, 1, COLOR_MUTED);
    Text_DrawCentered(Str(S_HELP_FOOTER), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 28, 1, COLOR_MUTED);
}

static void render_main_menu(void) {
    draw_background();

    Text_DrawCentered(Str(S_TITLE), SCREEN_WIDTH / 2.0f, 34, 3, COLOR_GOLD);
    Text_DrawCentered(Str(S_SUBTITLE), SCREEN_WIDTH / 2.0f, 76, 1, COLOR_MUTED);

    draw_menu_item(0, Str(S_MENU_NEW_GAME), 98);
    draw_menu_item(1, Str(S_MENU_LOAD_GAME), 126);
    draw_menu_item(2, Str(S_MENU_OPTIONS), 154);
    draw_menu_item(3, Str(S_MENU_HELP), 182);
    draw_menu_item(4, Str(S_MENU_QUIT), 210);

    Text_DrawCentered(Str(S_MENU_FOOTER), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 28, 1, COLOR_MUTED);
    Text_DrawRight(Str(S_VERSION), SCREEN_WIDTH - 12,
                   SCREEN_HEIGHT - 16, 1, 0x667A8A84u);

    draw_toast();
}

static void render_options(void) {
    draw_background();
    Text_DrawCentered(Str(S_OPTIONS_TITLE), SCREEN_WIDTH / 2.0f, 34, 2,
                      COLOR_GOLD);
    Render_DrawRect(60, 68, SCREEN_WIDTH - 120, 1, 0x663E5A5C);

    draw_value_row(0, Str(S_OPTIONS_LANGUAGE),
                   Str(Str_GetLanguage() == LANG_ZH
                           ? S_OPTIONS_LANGUAGE_ZH : S_OPTIONS_LANGUAGE_EN),
                   84);
    draw_value_row(1, Str(S_OPTIONS_MUSIC),
                   Str(Audio_MusicEnabled() ? S_OPTIONS_MUSIC_ON
                                            : S_OPTIONS_MUSIC_OFF),
                   110);
    const StrId hint = app.selection == 0 ? S_OPTIONS_LANGUAGE_HINT
                     : app.selection == 1 ? S_OPTIONS_MUSIC_HINT
                                         : S_OPTIONS_CALIBRATE_HINT;
    Text_DrawCentered(Str(hint), SCREEN_WIDTH / 2.0f, 136, 1, COLOR_MUTED);

    draw_menu_item(2, Str(S_OPTIONS_CALIBRATE), 158);
    draw_menu_item(3, Str(S_OPTIONS_BACK), 188);
    Text_DrawCentered(Str(S_MENU_FOOTER), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 28, 1, COLOR_MUTED);
    draw_toast();
}

static void render_new_game(void) {
    char value[48];
    draw_background();
    Text_DrawCentered(Str(S_NEW_GAME_TITLE), SCREEN_WIDTH / 2.0f, 32, 2,
                      COLOR_GOLD);
    Render_DrawRect(60, 66, SCREEN_WIDTH - 120, 1, 0x663E5A5C);

    draw_value_row(0, Str(S_NEW_GAME_MODE),
                   Str(app.new_mode == SAVE_MODE_TIMED
                           ? S_NEW_GAME_TIMED : S_NEW_GAME_STANDARD), 78);

    if (app.new_mode == SAVE_MODE_TIMED) {
        unsigned int minutes = time_choices[app.new_time_index] / 60u;
        snprintf(value, sizeof(value), Str(S_NEW_GAME_MINUTES), (int)minutes);
    } else {
        snprintf(value, sizeof(value), "%s", Str(S_NEW_GAME_TURNS_200));
    }
    draw_value_row(1, Str(S_NEW_GAME_LIMIT), value, 110);

    draw_menu_item(2, Str(S_NEW_GAME_START), 146);
    draw_menu_item(3, Str(S_NEW_GAME_BACK), 174);
    Text_DrawCentered(Str(S_MENU_FOOTER), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 28, 1, COLOR_MUTED);
    draw_toast();
}

static void render_pause(void) {
    MapView_Render(&app.map_view, &app.map, &app.world);
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xCC0A1116u);
    Render_DrawRect(64, 26, SCREEN_WIDTH - 128, 208, COLOR_PANEL);
    Render_DrawRect(64, 26, SCREEN_WIDTH - 128, 1, COLOR_GOLD);
    /* Cover the map HUD so the pause footer stays readable. */
    Render_DrawRect(0, SCREEN_HEIGHT - 38, SCREEN_WIDTH, 38, 0xFF0A1116u);
    Text_DrawCentered(Str(S_PAUSE_TITLE), SCREEN_WIDTH / 2.0f, 28, 2,
                      COLOR_GOLD);

    draw_menu_item(0, Str(S_PAUSE_RESUME), 68);
    draw_menu_item(1, Str(S_PAUSE_SAVE), 94);
    draw_menu_item(2, Str(S_PAUSE_LOAD), 120);
    draw_menu_item(3, Str(S_PAUSE_HELP), 146);
    draw_menu_item(4, Str(S_PAUSE_CALIBRATE), 172);
    draw_menu_item(5, Str(S_PAUSE_MAIN_MENU), 198);
    Text_DrawCentered(Str(S_MENU_FOOTER), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 20, 1, COLOR_MUTED);
    draw_toast();
}

static void render_save_menu(void) {
    draw_background();
    Text_DrawCentered(Str(S_SAVE_TITLE), SCREEN_WIDTH / 2.0f, 34, 2,
                      COLOR_GOLD);
    Render_DrawRect(60, 68, SCREEN_WIDTH - 120, 1, 0x663E5A5C);
    for (int i = 0; i < 3; i++) {
        draw_slot_row(i, i + SAVE_SLOT_FIRST, 86.0f + (float)i * 38.0f);
    }
    Text_DrawCentered(Str(S_MENU_FOOTER), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 28, 1, COLOR_MUTED);
    draw_toast();
}

static void render_load_menu(void) {
    draw_background();
    Text_DrawCentered(Str(S_LOAD_TITLE), SCREEN_WIDTH / 2.0f, 34, 2,
                      COLOR_GOLD);
    Render_DrawRect(60, 68, SCREEN_WIDTH - 120, 1, 0x663E5A5C);
    for (int slot = 0; slot <= SAVE_SLOT_LAST; slot++) {
        draw_slot_row(slot, slot, 80.0f + (float)slot * 34.0f);
    }
    Text_DrawCentered(Str(S_MENU_FOOTER), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 28, 1, COLOR_MUTED);
    draw_toast();
}

static void render_game_over(void) {
    draw_background();
    int victory = app.world.game_over == WORLD_GAME_VICTORY;
    unsigned int title_color = victory ? COLOR_GOLD : 0xFFE06050u;
    StrId title = S_GAME_DEFEAT;
    StrId body = S_GAME_DEFEAT_BODY;

    if (victory) {
        switch (app.world.victory_type) {
            case WORLD_VICTORY_DIPLOMATIC:
                title = S_GAME_VICTORY_DIPLOMATIC;
                body = S_GAME_VICTORY_DIPLOMATIC_BODY;
                break;
            case WORLD_VICTORY_SCIENCE:
                title = S_GAME_VICTORY_SCIENCE;
                body = S_GAME_VICTORY_SCIENCE_BODY;
                break;
            case WORLD_VICTORY_SCORE:
                title = S_GAME_VICTORY_SCORE;
                body = S_GAME_VICTORY_SCORE_BODY;
                break;
            default:
                title = S_GAME_VICTORY_CONQUEST;
                body = S_GAME_VICTORY_CONQUEST_BODY;
                break;
        }
    } else if (app.world.victory_type == WORLD_VICTORY_SCORE) {
        title = app.mode == SAVE_MODE_TIMED ? S_GAME_TIME_UP
                                            : S_GAME_TURN_LIMIT;
        body = S_GAME_SCORE_DEFEAT_BODY;
    }

    int turns = app.world.turn;
    if (app.mode == SAVE_MODE_STANDARD
        && app.world.victory_type == WORLD_VICTORY_SCORE
        && turns > WORLD_STANDARD_TURN_LIMIT) {
        turns = WORLD_STANDARD_TURN_LIMIT;
    }

    Text_DrawCentered(Str(title), SCREEN_WIDTH / 2.0f, 44, 3, title_color);
    Text_DrawCentered(Str(body), SCREEN_WIDTH / 2.0f, 100, 1, COLOR_TEXT);

    char line[128];
    snprintf(line, sizeof(line), "%s %d  %s %d",
             Str(S_GAME_RESULT_SCORE), World_PlayerScore(&app.world),
             Str(S_GAME_TURNS), turns);
    Text_DrawCentered(line, SCREEN_WIDTH / 2.0f, 138, 1, COLOR_GOLD);

    snprintf(line, sizeof(line), "%s %d  %s %d  %s %d",
             Str(S_GAME_RESULT_CITIES), World_PlayerCityCount(&app.world),
             Str(S_GAME_RESULT_POP), World_PlayerPopulation(&app.world),
             Str(S_GAME_RESULT_TECHS), World_PlayerTechCount(&app.world));
    Text_DrawCentered(line, SCREEN_WIDTH / 2.0f, 158, 1, COLOR_MUTED);

    snprintf(line, sizeof(line), "%s %02u:%02u  %s %d",
             Str(S_GAME_RESULT_TIME),
             (unsigned)(app.play_ms / 60000u),
             (unsigned)((app.play_ms / 1000u) % 60u),
             Str(S_GAME_RESULT_BEST), World_BestRivalScore(&app.world));
    Text_DrawCentered(line, SCREEN_WIDTH / 2.0f, 178, 1, COLOR_MUTED);
    Text_DrawCentered(Str(S_GAME_RETURN), SCREEN_WIDTH / 2.0f,
                      SCREEN_HEIGHT - 44, 1, COLOR_GOLD);
}

static void draw_screen_transition(void) {
    if (app.state_anim >= MENU_FADE_FRAMES) return;
    unsigned int alpha = (unsigned int)(255 * (MENU_FADE_FRAMES - app.state_anim)
                                        / MENU_FADE_FRAMES);
    Render_FlushLayer();
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, alpha << 24);
    Render_FlushLayer();
}

void App_Render(void) {
    Render_SetFontLanguage(Str_GetLanguage());
    switch (app.state) {
        case STATE_MAIN_MENU:
            render_main_menu();
            break;
        case STATE_OPTIONS:
            render_options();
            break;
        case STATE_HELP:
            render_help();
            break;
        case STATE_NEW_GAME:
            render_new_game();
            break;
        case STATE_MAP:
            MapView_Render(&app.map_view, &app.map, &app.world);
            break;
        case STATE_CITY:
            CityView_Render(&app.city_view, &app.map, &app.world);
            break;
        case STATE_DIPLOMACY:
            DiplomacyView_Render(&app.diplomacy_view, &app.world);
            break;
        case STATE_PAUSE:
            render_pause();
            break;
        case STATE_SAVE_MENU:
            render_save_menu();
            break;
        case STATE_LOAD_MENU:
            render_load_menu();
            break;
        case STATE_GAME_OVER:
            render_game_over();
            break;
        default:
            render_main_menu();
            break;
    }
    draw_screen_transition();
}
