#include "diplomacyview.h"

#include <pspctrl.h>

#include <stdio.h>
#include <string.h>

#include "diplomacy.h"
#include "game.h"
#include "gen/strings_gen.h"
#include "input.h"
#include "log.h"
#include "render.h"
#include "text.h"

#define TOP_H        34
#define LIST_X       8
#define LIST_Y       56
#define LIST_W       206
#define LIST_H       56
#define LIST_GAP     4
#define DETAIL_X     222
#define DETAIL_Y     56
#define DETAIL_W     250
#define DETAIL_H     162
#define FOOTER_Y     248
#define MESSAGE_FRAMES 180

#define COLOR_BG       0xFF0E1820u
#define COLOR_TOP      0xFF14242Cu
#define COLOR_PANEL    0xF20A1116u
#define COLOR_PANEL_OP 0xFF0A1116u
#define COLOR_PANEL_SEL 0xCC2B4749u
#define COLOR_BORDER   0xFF3E5A5Cu
#define COLOR_GOLD     0xFFD8B25Cu
#define COLOR_TEXT     0xFFF5EEDCu
#define COLOR_MUTED    0xFF9FB0A8u
#define COLOR_GREEN    0xFF7ED37Eu
#define COLOR_RED      0xFFE06050u
#define COLOR_ORANGE   0xFFE0A050u
#define COLOR_BLUE     0xFF70C0E0u
#define COLOR_DIM      0x99050A0Eu

typedef enum {
    ACTION_ACCEPT = 0,
    ACTION_REJECT,
    ACTION_WAR,
    ACTION_PEACE,
    ACTION_NAP,
    ACTION_OPEN_BORDERS,
    ACTION_ALLIANCE,
    ACTION_TRADE,
    ACTION_GIFT,
    ACTION_DENOUNCE,
    ACTION_BACK,
    ACTION_COUNT
} DiploAction;

static int civ_for_selected(const DiplomacyView *view) {
    return view->selected + 1;
}

static int civ_alive(const World *world, int civ) {
    if (!world || civ < 1 || civ > WORLD_AI_COUNT) return 0;
    return world->civs[civ].active && World_CountCities(world, civ) > 0;
}

static StrId band_str(int band) {
    switch (band) {
        case DIPLO_BAND_HOSTILE:  return S_DIPLO_HOSTILE;
        case DIPLO_BAND_COLD:     return S_DIPLO_COLD;
        case DIPLO_BAND_NEUTRAL:  return S_DIPLO_NEUTRAL;
        case DIPLO_BAND_FRIENDLY: return S_DIPLO_FRIENDLY;
        default:                  return S_DIPLO_ALLIED;
    }
}

static unsigned int band_color(int band) {
    switch (band) {
        case DIPLO_BAND_HOSTILE:  return COLOR_RED;
        case DIPLO_BAND_COLD:     return COLOR_ORANGE;
        case DIPLO_BAND_NEUTRAL:  return COLOR_MUTED;
        case DIPLO_BAND_FRIENDLY: return COLOR_GREEN;
        default:                  return COLOR_GOLD;
    }
}

static StrId action_str(int action) {
    switch (action) {
        case ACTION_ACCEPT:   return S_DIPLO_ACCEPT;
        case ACTION_REJECT:   return S_DIPLO_REJECT;
        case ACTION_WAR:      return S_DIPLO_DECLARE_WAR;
        case ACTION_PEACE:    return S_DIPLO_PROPOSE_PEACE;
        case ACTION_NAP:      return S_DIPLO_NAP_ACTION;
        case ACTION_OPEN_BORDERS: return S_DIPLO_OPEN_BORDERS_ACTION;
        case ACTION_ALLIANCE: return S_DIPLO_ALLIANCE_ACTION;
        case ACTION_TRADE:    return S_DIPLO_TRADE_ACTION;
        case ACTION_GIFT:     return S_DIPLO_GIFT_ACTION;
        case ACTION_DENOUNCE: return S_DIPLO_DENOUNCE_ACTION;
        default:              return S_DIPLO_BACK;
    }
}

static int build_actions(const World *world, int civ, int out[ACTION_COUNT]) {
    int count = 0;
    int at_war = World_AreAtWar(world, WORLD_CIV_PLAYER, civ);

    if (!civ_alive(world, civ)) {
        out[count++] = ACTION_BACK;
        return count;
    }

    if (world->proposal_civ == civ
        && world->proposal_type != WORLD_PROPOSAL_NONE) {
        out[count++] = ACTION_ACCEPT;
        out[count++] = ACTION_REJECT;
    }

    if (at_war) {
        out[count++] = ACTION_PEACE;
    } else if (World_CanDeclareWar(world, WORLD_CIV_PLAYER, civ)) {
        out[count++] = ACTION_WAR;
    }

    if (Diplomacy_WouldAcceptNAP(world, civ)) {
        out[count++] = ACTION_NAP;
    }
    if (Diplomacy_WouldAcceptOpenBorders(world, civ)) {
        out[count++] = ACTION_OPEN_BORDERS;
    }
    if (Diplomacy_WouldAcceptAlliance(world, civ)) {
        out[count++] = ACTION_ALLIANCE;
    }
    if (Diplomacy_WouldAcceptTrade(world, civ)) {
        out[count++] = ACTION_TRADE;
    }
    out[count++] = ACTION_GIFT;
    if (Diplomacy_CanDenounce(world, civ)) {
        out[count++] = ACTION_DENOUNCE;
    }
    out[count++] = ACTION_BACK;
    return count;
}

static void clamp_menu(const World *world, DiplomacyView *view) {
    int actions[ACTION_COUNT];
    int count = build_actions(world, civ_for_selected(view), actions);
    if (view->menu_selection < 0) view->menu_selection = 0;
    if (view->menu_selection >= count) view->menu_selection = count - 1;
    if (view->menu_selection < 0) view->menu_selection = 0;
}

static void set_message(DiplomacyView *view, const char *text) {
    snprintf(view->message, sizeof(view->message), "%s",
             text ? text : "");
    view->message_timer = MESSAGE_FRAMES;
}

static void execute_action(DiplomacyView *view, Map *map, World *world,
                           int action) {
    int civ = civ_for_selected(view);
    if (civ < 1 || civ > WORLD_AI_COUNT) return;

    int relation_before = World_Relation(world, WORLD_CIV_PLAYER, civ);
    int treasury_before = (int)world->treasury;

    switch (action) {
        case ACTION_ACCEPT:
            if (world->proposal_civ == (uint8_t)civ
                && Diplomacy_AcceptProposal(world) != WORLD_PROPOSAL_NONE) {
                set_message(view, Str(S_DIPLO_PROPOSAL_ACCEPTED));
            } else {
                set_message(view, Str(S_DIPLO_NO_PROPOSAL));
            }
            break;
        case ACTION_REJECT:
            if (world->proposal_civ == (uint8_t)civ
                && Diplomacy_RejectProposal(world) != WORLD_PROPOSAL_NONE) {
                set_message(view, Str(S_DIPLO_PROPOSAL_REJECTED));
            } else {
                set_message(view, Str(S_DIPLO_NO_PROPOSAL));
            }
            break;
        case ACTION_WAR:
            if (Diplomacy_DeclareWar(world, civ)) {
                set_message(view, Str(S_DIPLO_WAR_DECLARED));
            } else {
                set_message(view, Str(S_DIPLO_TREATY_BLOCKS_WAR));
            }
            break;
        case ACTION_PEACE:
            if (Diplomacy_RequestPeace(world, civ)) {
                set_message(view, Str(S_DIPLO_PEACE_MADE));
            } else {
                set_message(view, Str(S_DIPLO_PROPOSAL_REJECTED));
            }
            break;
        case ACTION_NAP:
            if (Diplomacy_RequestNAP(world, civ)) {
                set_message(view, Str(S_DIPLO_NAP_SIGNED));
            } else {
                set_message(view, Str(S_DIPLO_PROPOSAL_REJECTED));
            }
            break;
        case ACTION_OPEN_BORDERS:
            if (Diplomacy_RequestOpenBorders(world, civ)) {
                set_message(view, Str(S_DIPLO_OPEN_BORDERS_SIGNED));
            } else {
                set_message(view, Str(S_DIPLO_RELATION_TOO_LOW));
            }
            break;
        case ACTION_TRADE:
            if (Diplomacy_RequestTrade(world, civ)) {
                set_message(view, Str(S_DIPLO_TRADE_SIGNED));
            } else {
                set_message(view, Str(S_DIPLO_TRADE_REJECTED));
            }
            break;
        case ACTION_ALLIANCE:
            if (Diplomacy_RequestAlliance(world, civ)) {
                set_message(view, Str(S_DIPLO_ALLIANCE_SIGNED));
            } else if (World_Relation(world, WORLD_CIV_PLAYER, civ)
                       < DIPLO_ALLIANCE_MIN) {
                set_message(view, Str(S_DIPLO_RELATION_TOO_LOW));
            } else {
                set_message(view, Str(S_DIPLO_PROPOSAL_REJECTED));
            }
            break;
        case ACTION_GIFT: {
            int gain = Diplomacy_SendGift(world, civ);
            if (gain >= 0) {
                char text[80];
                snprintf(text, sizeof(text), Str(S_DIPLO_GIFT_SENT), gain);
                set_message(view, text);
            } else if (gain == DIPLO_ERR_GOLD) {
                set_message(view, Str(S_DIPLO_NOT_ENOUGH_GOLD));
            } else if (gain == DIPLO_ERR_STATE
                       && World_Relation(world, WORLD_CIV_PLAYER, civ) >= 100) {
                set_message(view, Str(S_DIPLO_GIFT_MAX));
            } else {
                set_message(view, Str(S_DIPLO_PROPOSAL_REJECTED));
            }
            break;
        }
        case ACTION_DENOUNCE: {
            int penalty = Diplomacy_Denounce(world, civ);
            if (penalty > 0) {
                char text[80];
                snprintf(text, sizeof(text), Str(S_DIPLO_DENOUNCED),
                         penalty);
                set_message(view, text);
            } else {
                set_message(view, Str(S_DIPLO_DENOUNCE_USED));
            }
            break;
        }
        default:
            break;
    }

    Log_Printf("diplo: action=%d civ=%d relation=%d->%d gold=%d->%d",
               action, civ, relation_before,
               World_Relation(world, WORLD_CIV_PLAYER, civ),
               treasury_before, (int)world->treasury);
    if (map) World_UpdateVision(world, map);
    view->menu_open = 0;
    clamp_menu(world, view);
}

void DiplomacyView_Init(DiplomacyView *view, const World *world) {
    memset(view, 0, sizeof(*view));
    view->selected = 0;
    if (world && world->proposal_civ >= 1
        && world->proposal_civ <= WORLD_AI_COUNT) {
        view->selected = world->proposal_civ - 1;
    } else if (world) {
        for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
            if (civ_alive(world, civ)) {
                view->selected = civ - 1;
                break;
            }
        }
    }
    view->menu_selection = 0;
}

int DiplomacyView_Update(DiplomacyView *view, Map *map, World *world) {
    if (!view || !world) return 1;
    if (view->message_timer > 0) view->message_timer--;

    if (view->menu_open) {
        int actions[ACTION_COUNT];
        int count = build_actions(world, civ_for_selected(view), actions);
        if (Input_Repeated(PSP_CTRL_UP)) view->menu_selection--;
        if (Input_Repeated(PSP_CTRL_DOWN)) view->menu_selection++;
        if (view->menu_selection < 0) view->menu_selection = count - 1;
        if (view->menu_selection >= count) view->menu_selection = 0;
        if (Input_Pressed(PSP_CTRL_CIRCLE)) {
            view->menu_open = 0;
            return 0;
        }
        if (Input_Pressed(PSP_CTRL_CROSS)) {
            if (view->menu_selection >= 0
                && view->menu_selection < count) {
                int action = actions[view->menu_selection];
                if (action == ACTION_BACK) {
                    view->menu_open = 0;
                } else {
                    execute_action(view, map, world, action);
                }
            }
        }
        return 0;
    }

    if (Input_Repeated(PSP_CTRL_UP)) {
        view->selected = (view->selected + WORLD_AI_COUNT - 1)
                       % WORLD_AI_COUNT;
        view->menu_selection = 0;
    }
    if (Input_Repeated(PSP_CTRL_DOWN)) {
        view->selected = (view->selected + 1) % WORLD_AI_COUNT;
        view->menu_selection = 0;
    }
    if (Input_Pressed(PSP_CTRL_CROSS)) {
        if (civ_alive(world, civ_for_selected(view))) {
            view->menu_open = 1;
            view->menu_selection = 0;
            Log_Printf("diplo: open civ=%d relation=%d",
                       civ_for_selected(view),
                       World_Relation(world, WORLD_CIV_PLAYER,
                                      civ_for_selected(view)));
        } else {
            set_message(view, Str(S_DIPLO_ELIMINATED));
        }
    }
    if (Input_Pressed(PSP_CTRL_CIRCLE)) {
        Log_Printf("diplo: back to map");
        return 1;
    }
    return 0;
}

static void draw_frame(float x, float y, float w, float h,
                       unsigned int color) {
    Render_DrawRect(x, y, w, 1, color);
    Render_DrawRect(x, y + h - 1, w, 1, color);
    Render_DrawRect(x, y, 1, h, color);
    Render_DrawRect(x + w - 1, y, 1, h, color);
}

static void draw_background(void) {
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BG);
    Render_DrawRect(0, 0, SCREEN_WIDTH, TOP_H, COLOR_TOP);
    Render_DrawRect(0, TOP_H - 1, SCREEN_WIDTH, 1, COLOR_GOLD);
    Render_DrawRect(0, FOOTER_Y, SCREEN_WIDTH, SCREEN_HEIGHT - FOOTER_Y,
                    COLOR_TOP);
    Render_DrawRect(0, FOOTER_Y, SCREEN_WIDTH, 1, COLOR_GOLD);

    Render_DrawRect(LIST_X, LIST_Y, LIST_W, DETAIL_H, COLOR_PANEL);
    draw_frame(LIST_X, LIST_Y, LIST_W, DETAIL_H, COLOR_BORDER);
    Render_DrawRect(DETAIL_X, DETAIL_Y, DETAIL_W, DETAIL_H, COLOR_PANEL);
    draw_frame(DETAIL_X, DETAIL_Y, DETAIL_W, DETAIL_H, COLOR_BORDER);
}

static void draw_relation_bar(float x, float y, int relation) {
    const float w = 220.0f;
    const float h = 7.0f;
    Render_DrawRect(x, y, w, h, 0xFF26343Au);
    float center = x + w * 0.5f;
    Render_DrawRect(center - 1, y - 1, 2, h + 2, COLOR_BORDER);

    int clamped = relation;
    if (clamped < -100) clamped = -100;
    if (clamped > 100) clamped = 100;
    float marker = x + (float)(clamped + 100) * (w / 200.0f);
    if (marker < x + 2) marker = x + 2;
    if (marker > x + w - 2) marker = x + w - 2;

    if (marker < center) {
        Render_DrawRect(marker, y + 1, center - marker, h - 2, COLOR_RED);
    } else {
        Render_DrawRect(center, y + 1, marker - center, h - 2, COLOR_GREEN);
    }
    Render_DrawRect(marker - 1, y - 2, 3, h + 4, COLOR_TEXT);
}

static void draw_civ_row(const DiplomacyView *view, const World *world,
                         int civ, float y) {
    int selected = (view->selected == civ - 1);
    Render_DrawRect(LIST_X + 1, y, LIST_W - 2, LIST_H, COLOR_PANEL);
    Render_DrawRect(LIST_X + 2, y + 2, 4, LIST_H - 4,
                    World_CivColor(civ));
    draw_frame(LIST_X + 1, y, LIST_W - 2, LIST_H,
               selected ? COLOR_GOLD : COLOR_BORDER);
    if (selected) {
        Render_DrawRect(LIST_X + 3, y + 2, 2, LIST_H - 4, COLOR_GOLD);
    }

    int relation = World_Relation(world, WORLD_CIV_PLAYER, civ);
    int band = Diplomacy_RelationBand(relation);
    int alive = civ_alive(world, civ);
    int at_war = World_AreAtWar(world, WORLD_CIV_PLAYER, civ);
    unsigned int row_color = alive ? COLOR_TEXT : COLOR_MUTED;

    Text_Draw(Str(World_CivNameId(civ)), (float)(LIST_X + 12),
              y + 4, 1, row_color);
    char value[16];
    snprintf(value, sizeof(value), "%+d", relation);
    Text_DrawRight(value, (float)(LIST_X + LIST_W - 8), y + 4, 1,
                   alive ? band_color(band) : COLOR_MUTED);

    StrId status = S_DIPLO_ELIMINATED;
    if (alive) {
        if (at_war) {
            status = S_DIPLO_AT_WAR;
        } else if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                                   WORLD_TREATY_ALLIANCE)) {
            status = S_DIPLO_ALLIANCE;
        } else if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                                   WORLD_TREATY_NAP)) {
            status = S_DIPLO_NAP;
        } else if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                                   WORLD_TREATY_OPEN_BORDERS)) {
            status = S_DIPLO_OPEN_BORDERS;
        } else if (World_HasTrade(world, WORLD_CIV_PLAYER, civ)) {
            status = S_DIPLO_TRADE;
        } else {
            status = S_DIPLO_PEACE;
        }
    }
    Text_Draw(Str(status), (float)(LIST_X + 12), y + 22, 1,
              alive ? (at_war ? COLOR_RED : COLOR_MUTED) : COLOR_MUTED);
    if (alive) {
        Text_Draw(Str(band_str(band)), (float)(LIST_X + 12), y + 40, 1,
                  band_color(band));
    }
    if (world->proposal_civ == (uint8_t)civ
        && world->proposal_type != WORLD_PROPOSAL_NONE) {
        Text_DrawRight("!", (float)(LIST_X + LIST_W - 8), y + 22, 1,
                       COLOR_GOLD);
    }
}

static void draw_detail(const DiplomacyView *view, const World *world) {
    int civ = civ_for_selected(view);
    int alive = civ_alive(world, civ);
    int relation = World_Relation(world, WORLD_CIV_PLAYER, civ);
    int band = Diplomacy_RelationBand(relation);
    float x = (float)(DETAIL_X + 10);
    float right = (float)(DETAIL_X + DETAIL_W - 10);
    float y = (float)(DETAIL_Y + 8);

    Text_Draw(Str(World_CivNameId(civ)), x, y, 1, COLOR_GOLD);
    y += 18;
    Text_Drawf(x, y, 1, COLOR_TEXT, "%s %+d", Str(S_DIPLO_RELATION),
               relation);
    Text_DrawRight(Str(band_str(band)), right, y, 1, band_color(band));
    y += 18;
    draw_relation_bar(x, y, relation);
    y += 18;

    int at_war = World_AreAtWar(world, WORLD_CIV_PLAYER, civ);
    if (!alive) {
        Text_Draw(Str(S_DIPLO_ELIMINATED), x, y, 1, COLOR_MUTED);
    } else if (at_war) {
        int since = world->turn
                  - (int)world->civs[civ].war_since[WORLD_CIV_PLAYER];
        if (since < 0) since = 0;
        Text_Drawf(x, y, 1, COLOR_RED, "%s", Str(S_DIPLO_AT_WAR));
        y += 18;
        Text_Drawf(x, y, 1, COLOR_MUTED, Str(S_DIPLO_WAR_TURNS), since);
    } else {
        if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                            WORLD_TREATY_ALLIANCE)) {
            Text_Drawf(x, y, 1, COLOR_GREEN, "%s: %s", Str(S_DIPLO_TREATY),
                       Str(S_DIPLO_ALLIANCE));
        } else if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                                   WORLD_TREATY_NAP)) {
            Text_Drawf(x, y, 1, COLOR_GREEN, "%s: %s %d",
                       Str(S_DIPLO_TREATY), Str(S_DIPLO_NAP),
                       World_TreatyTurns(world, WORLD_CIV_PLAYER, civ));
        } else if (World_HasTreaty(world, WORLD_CIV_PLAYER, civ,
                                   WORLD_TREATY_OPEN_BORDERS)) {
            Text_Drawf(x, y, 1, COLOR_GREEN, "%s: %s %d",
                       Str(S_DIPLO_TREATY), Str(S_DIPLO_OPEN_BORDERS),
                       World_OpenBordersTurns(world, WORLD_CIV_PLAYER, civ));
        } else {
            Text_Drawf(x, y, 1, COLOR_MUTED, "%s: %s", Str(S_DIPLO_TREATY),
                       Str(S_DIPLO_NO_TREATY));
        }
    }

    if (alive && !at_war) {
        y += 18;
        if (World_HasTrade(world, WORLD_CIV_PLAYER, civ)) {
            Text_Drawf(x, y, 1, COLOR_GREEN, Str(S_DIPLO_TRADE_STATUS),
                       WORLD_TRADE_GOLD,
                       World_TradeTurns(world, WORLD_CIV_PLAYER, civ));
        } else {
            Text_Drawf(x, y, 1, COLOR_MUTED, "%s: %s", Str(S_DIPLO_TRADE),
                       Str(S_DIPLO_NO_TRADE));
        }
    }

    y += 18;
    Text_Drawf(x, y, 1, COLOR_TEXT, Str(S_DIPLO_ARMIES),
               World_MilitaryCount(world, WORLD_CIV_PLAYER),
               World_MilitaryCount(world, civ));
    y += 18;

    if (world->proposal_civ == (uint8_t)civ
        && world->proposal_type != WORLD_PROPOSAL_NONE) {
        StrId offer = S_DIPLO_OFFERS_PEACE;
        if (world->proposal_type == WORLD_PROPOSAL_NAP) {
            offer = S_DIPLO_OFFERS_NAP;
        } else if (world->proposal_type == WORLD_PROPOSAL_ALLIANCE) {
            offer = S_DIPLO_OFFERS_ALLIANCE;
        }
        Text_Drawf(x, y, 1, COLOR_GOLD, "%s: %s", Str(S_DIPLO_PROPOSAL),
                   Str(offer));
    } else {
        Text_Draw(Str(S_DIPLO_NO_PROPOSAL), x, y, 1, COLOR_MUTED);
    }

    if (World_LivingRivals(world) >= 2) {
        Text_Drawf(x, (float)(DETAIL_Y + DETAIL_H - 16), 1, COLOR_MUTED,
                   Str(S_DIPLO_VICTORY_PROGRESS),
                   (int)world->alliance_victory_turns);
    }
}

static void draw_message(const DiplomacyView *view) {
    if (view->message_timer <= 0 || view->message[0] == '\0') return;
    float w = (float)Text_Width(view->message, 1) + 20.0f;
    float h = (float)Text_Height(1) + 10.0f;
    float x = (SCREEN_WIDTH - w) / 2.0f;
    float y = (float)(FOOTER_Y - 8) - h;
    Render_DrawRect(x, y, w, h, COLOR_PANEL_OP);
    draw_frame(x, y, w, h, COLOR_GOLD);
    Render_FlushLayer();
    Text_DrawCentered(view->message, SCREEN_WIDTH / 2.0f, y + 5, 1,
                      COLOR_TEXT);
    Render_FlushLayer();
}

static void draw_action_menu(const DiplomacyView *view, const World *world) {
    int civ = civ_for_selected(view);
    int actions[ACTION_COUNT];
    int count = build_actions(world, civ, actions);
    float h = (float)(32 + count * 20 + 8);
    float w = 300.0f;
    float x = (SCREEN_WIDTH - w) / 2.0f;
    float y = (SCREEN_HEIGHT - h) / 2.0f;

    Render_FlushLayer();
    Render_DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xCC050A0Eu);
    Render_DrawRect(x, y, w, h, COLOR_PANEL_OP);
    draw_frame(x, y, w, h, COLOR_GOLD);
    Text_Drawf(x + 12, y + 7, 1, COLOR_GOLD, "%s  %s",
               Str(S_DIPLO_ACTIONS), Str(World_CivNameId(civ)));

    for (int i = 0; i < count; i++) {
        float row_y = y + 26 + i * 20;
        int selected = (i == view->menu_selection);
        if (selected) {
            Render_DrawRect(x + 6, row_y - 2, w - 12, 18, COLOR_PANEL_SEL);
            Render_DrawRect(x + 6, row_y - 2, 3, 18, COLOR_GOLD);
        }
        Text_Draw(Str(action_str(actions[i])), x + 16, row_y, 1,
                  selected ? COLOR_TEXT : COLOR_MUTED);
    }
    Text_DrawCentered(Str(S_DIPLO_MENU_FOOTER), SCREEN_WIDTH / 2.0f,
                      y + h - 14, 1, COLOR_MUTED);
    Render_FlushLayer();
}

void DiplomacyView_Render(const DiplomacyView *view, const World *world) {
    draw_background();

    Text_DrawCentered(Str(S_DIPLO_TITLE), SCREEN_WIDTH / 2.0f, 2, 2,
                      COLOR_GOLD);
    char header[64];
    snprintf(header, sizeof(header), Str(S_DIPLO_TURN_GOLD), world->turn,
             (int)world->treasury);
    Text_Draw(header, (float)LIST_X, 38, 1, COLOR_TEXT);
    Text_Drawf((float)DETAIL_X, 38, 1, COLOR_MUTED,
               Str(S_DIPLO_VICTORY_PROGRESS),
               (int)world->alliance_victory_turns);

    for (int civ = 1; civ <= WORLD_AI_COUNT; civ++) {
        float row_y = (float)(LIST_Y + (civ - 1) * (LIST_H + LIST_GAP));
        draw_civ_row(view, world, civ, row_y);
    }
    draw_detail(view, world);

    Text_Draw(Str(S_DIPLO_FOOTER), (float)LIST_X, (float)(FOOTER_Y + 5),
              1, COLOR_MUTED);
    Text_DrawRight(Str(S_VERSION), SCREEN_WIDTH - 8,
                   (float)(FOOTER_Y + 5), 1, COLOR_MUTED);

    draw_message(view);
    if (view->menu_open) {
        draw_action_menu(view, world);
    }
}
