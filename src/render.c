#include "render.h"

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psputils.h>

#include "game.h"
#include "gen/font_en.h"
#include "gen/font_zh.h"
#include "gen/terrain_gen.h"
#include "gen/transition_gen.h"

#define MAX_RECTS 1024
#define MAX_SPRITES 2048
#define MAX_TRANSITION_SPRITES 4096
#define MAX_TEXT_QUADS 2048

unsigned int __attribute__((aligned(16))) render_list[524288]; /* 2MB */

struct RectV {
    unsigned int color;
    float x, y, z;
} __attribute__((packed));

_Static_assert(sizeof(struct RectV) == 16, "GU color vertex must be 16 bytes");

struct TextV {
    float u, v;
    unsigned int color;
    float x, y, z;
} __attribute__((packed));

_Static_assert(sizeof(struct TextV) == 24, "GU textured vertex must be 24 bytes");

struct SpriteV {
    float u, v;
    unsigned int color;
    float x, y, z;
} __attribute__((packed));

_Static_assert(sizeof(struct SpriteV) == 24, "GU sprite vertex must be 24 bytes");

/* Keep the vertex buffers on their own cache lines.  The CPU writes vertices
 * through the cached alias and writes the used range back before GU reads it.
 * Writing through the uncached alias is unsafe here: nearby cached variables
 * can share a cache line with the first vertices, and their writeback can
 * overwrite the vertices after the uncached store. */
static struct RectV rect_buf[MAX_RECTS * 2] __attribute__((aligned(64)));
static struct SpriteV sprite_buf[MAX_SPRITES * 2] __attribute__((aligned(64)));
static struct SpriteV transition_buf[MAX_TRANSITION_SPRITES * 2]
    __attribute__((aligned(64)));
static struct TextV text_buf[MAX_TEXT_QUADS * 2] __attribute__((aligned(64)));
static struct SpriteV minimap_buf[2] __attribute__((aligned(64)));
static int minimap_active;
static const unsigned short *minimap_pixels;
static int minimap_tex_w;
static int minimap_tex_h;

static int font_language;
static int text_smooth;

void Render_SetFontLanguage(int language) {
    font_language = language == 1 ? 1 : 0;
}

void Render_SetTextSmooth(int smooth) {
    text_smooth = smooth ? 1 : 0;
}

static void setup_font_texture(void) {
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    if (font_language) {
        sceGuTexImage(0, FONT_ZH_ATLAS_W, FONT_ZH_ATLAS_H,
                      FONT_ZH_ATLAS_W, font_zh_rgba);
        sceGuTexScale(1.0f / (float)FONT_ZH_ATLAS_W,
                      1.0f / (float)FONT_ZH_ATLAS_H);
    } else {
        sceGuTexImage(0, FONT_ATLAS_W, FONT_ATLAS_H, FONT_ATLAS_W,
                      font_en_rgba);
        sceGuTexScale(1.0f / (float)FONT_ATLAS_W,
                      1.0f / (float)FONT_ATLAS_H);
    }
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexEnvColor(0);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    int filter = text_smooth ? GU_LINEAR : GU_NEAREST;
    sceGuTexFilter(filter, filter);
}

static void setup_terrain_texture(void) {
    sceGuTexMode(GU_PSM_5551, 0, 0, 0);
    sceGuTexImage(0, TERRAIN_ATLAS_W, TERRAIN_ATLAS_H, TERRAIN_ATLAS_W,
                  terrain_atlas_5551);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexEnvColor(0);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexScale(1.0f / (float)TERRAIN_ATLAS_W,
                  1.0f / (float)TERRAIN_ATLAS_H);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
}

static void setup_transition_texture(void) {
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, TRANSITION_ATLAS_W, TRANSITION_ATLAS_H,
                  TRANSITION_ATLAS_W, transition_atlas_rgba);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexEnvColor(0);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexScale(1.0f / (float)TRANSITION_ATLAS_W,
                  1.0f / (float)TRANSITION_ATLAS_H);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    /* Linear filtering turns the generated alpha ramp into a soft edge at
     * 24px tile scale. */
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
}

static int rect_count;
static int rect_base;
static int sprite_count;
static int sprite_base;
static int transition_count;
static int transition_base;
static int text_count;
static int text_base;
static unsigned int clear_color = 0xFF141C24; /* AARRGGBB */

void Render_SetClearColor(unsigned int color) {
    clear_color = color;
}

void Render_Init(void) {
    void *fbp0 = (void *)0x40000000;
    void *fbp1 = (void *)(0x40000000 + BUFFER_WIDTH * SCREEN_HEIGHT * 4);
    void *zbp = (void *)(0x40000000 + BUFFER_WIDTH * SCREEN_HEIGHT * 8);

    sceGuInit();
    sceGuStart(GU_DIRECT, render_list);

    sceGuDrawBuffer(GU_PSM_8888, fbp0, BUFFER_WIDTH);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, fbp1, BUFFER_WIDTH);
    sceGuDepthBuffer(zbp, BUFFER_WIDTH);

    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(0xC350, 0x2710);
    sceGuShadeModel(GU_SMOOTH);

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_FOG);

    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    sceKernelDcacheWritebackRange((void *)font_en_rgba, sizeof(font_en_rgba));
    sceKernelDcacheWritebackRange((void *)font_zh_rgba, sizeof(font_zh_rgba));
    sceKernelDcacheWritebackRange((void *)terrain_atlas_5551, sizeof(terrain_atlas_5551));
    sceKernelDcacheWritebackRange((void *)transition_atlas_rgba,
                                  sizeof(transition_atlas_rgba));

    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFrontFace(GU_CW);

    sceGuClearColor(FIXC(clear_color));
    sceGuClearDepth(0);
    sceGuClearStencil(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumMatrixMode(GU_TEXTURE);
    sceGumLoadIdentity();
    sceGumUpdateMatrix();

    sceGuEnable(GU_TEXTURE_2D);
    setup_font_texture();

    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void Render_BeginFrame(void) {
    rect_count = rect_base = 0;
    sprite_count = sprite_base = 0;
    transition_count = transition_base = 0;
    text_count = text_base = 0;
    minimap_active = 0;

    sceGuStart(GU_DIRECT, render_list);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuClearColor(FIXC(clear_color));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

void Render_DrawRect(float x, float y, float w, float h, unsigned int color) {
    if (rect_count >= MAX_RECTS) return;
    struct RectV *v = &rect_buf[rect_count * 2];
    unsigned int c = FIXC(color);
    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.5f;
    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.5f;
    rect_count++;
}

void Render_DrawSprite(float x, float y, float w, float h,
                       float u0, float v0, float u1, float v1,
                       unsigned int color) {
    if (sprite_count >= MAX_SPRITES) return;
    struct SpriteV *v = &sprite_buf[sprite_count * 2];
    unsigned int c = FIXC(color);
    v[0].u = u0;
    v[0].v = v0;
    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.5f;
    v[1].u = u1;
    v[1].v = v1;
    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.5f;
    sprite_count++;
}

void Render_DrawTransitionSprite(float x, float y, float w, float h,
                                 float u0, float v0, float u1, float v1,
                                 unsigned int color) {
    if (transition_count >= MAX_TRANSITION_SPRITES) return;
    struct SpriteV *v = &transition_buf[transition_count * 2];
    unsigned int c = FIXC(color);
    v[0].u = u0;
    v[0].v = v0;
    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.5f;
    v[1].u = u1;
    v[1].v = v1;
    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.5f;
    transition_count++;
}

void Render_DrawTextQuad(float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1,
                         unsigned int color) {
    if (text_count >= MAX_TEXT_QUADS) return;
    struct TextV *v = &text_buf[text_count * 2];
    unsigned int c = FIXC(color);
    v[0].u = u0;
    v[0].v = v0;
    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.5f;
    v[1].u = u1;
    v[1].v = v1;
    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.5f;
    text_count++;
}

void Render_DrawMinimap(float x, float y, float w, float h,
                        const unsigned short *pixels, int tex_w, int tex_h,
                        float u1, float v1, unsigned int color) {
    if (!pixels || tex_w <= 0 || tex_h <= 0) return;
    unsigned int c = FIXC(color);
    struct SpriteV *v = minimap_buf;
    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[0].color = c;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.5f;
    v[1].u = u1;
    v[1].v = v1;
    v[1].color = c;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.5f;
    minimap_pixels = pixels;
    minimap_tex_w = tex_w;
    minimap_tex_h = tex_h;
    minimap_active = 1;
}

static void flush_rects(void) {
    if (rect_count <= rect_base) return;
    struct RectV *v = rect_buf + rect_base * 2;
    int count = (rect_count - rect_base) * 2;
    sceKernelDcacheWritebackRange(v, (unsigned int)(count * sizeof(*v)));
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_SPRITES,
                   GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   count, NULL, v);
    rect_base = rect_count;
}

static void flush_sprites(void) {
    if (sprite_count <= sprite_base) return;
    struct SpriteV *v = sprite_buf + sprite_base * 2;
    int count = (sprite_count - sprite_base) * 2;
    sceKernelDcacheWritebackRange(v, (unsigned int)(count * sizeof(*v)));
    sceGuEnable(GU_TEXTURE_2D);
    setup_terrain_texture();
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   count, NULL, v);
    sprite_base = sprite_count;
}

static void flush_transitions(void) {
    if (transition_count <= transition_base) return;
    struct SpriteV *v = transition_buf + transition_base * 2;
    int count = (transition_count - transition_base) * 2;
    sceKernelDcacheWritebackRange(v, (unsigned int)(count * sizeof(*v)));
    sceGuEnable(GU_TEXTURE_2D);
    setup_transition_texture();
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   count, NULL, v);
    transition_base = transition_count;
}

static void flush_text(void) {
    if (text_count <= text_base) return;
    struct TextV *v = text_buf + text_base * 2;
    int count = (text_count - text_base) * 2;
    sceKernelDcacheWritebackRange(v, (unsigned int)(count * sizeof(*v)));
    sceGuEnable(GU_TEXTURE_2D);
    setup_font_texture();
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   count, NULL, v);
    text_base = text_count;
}

static void flush_minimap(void) {
    if (!minimap_active || !minimap_pixels) return;
    sceKernelDcacheWritebackRange((void *)minimap_pixels,
                                  (unsigned int)(minimap_tex_w
                                                 * minimap_tex_h * 2));
    sceKernelDcacheWritebackRange(minimap_buf, sizeof(minimap_buf));
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_5551, 0, 0, 0);
    sceGuTexImage(0, minimap_tex_w, minimap_tex_h, minimap_tex_w,
                  minimap_pixels);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexEnvColor(0);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuTexScale(1.0f / (float)minimap_tex_w,
                  1.0f / (float)minimap_tex_h);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuDrawArray(GU_SPRITES,
                   GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, NULL, minimap_buf);
    minimap_active = 0;
}

void Render_FlushLayer(void) {
    flush_sprites();
    flush_transitions();
    flush_rects();
    flush_minimap();
    flush_text();
}

void Render_SetScissor(int x, int y, int w, int h) {
    sceGuScissor(x, y, w, h);
}

void Render_EndFrame(void) {
    Render_FlushLayer();
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
}
