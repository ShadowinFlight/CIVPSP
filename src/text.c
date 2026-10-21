#include "text.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gen/font_en.h"
#include "gen/font_zh.h"
#include "gen/strings_gen.h"
#include "render.h"

/* The GU text sample uses texture coordinates in texels and sets
 * sceGuTexScale(1/atlas_width, 1/atlas_height).  Do the same here. */
static float glyph_u(int code) {
    int idx = code - FONT_FIRST;
    if (idx < 0) idx = (int)'?' - FONT_FIRST;
    int col = idx % FONT_COLS;
    return (float)(col * FONT_CELL_W);
}

static float glyph_v(int code) {
    int idx = code - FONT_FIRST;
    if (idx < 0) idx = (int)'?' - FONT_FIRST;
    int row = idx / FONT_COLS;
    return (float)(row * FONT_CELL_H);
}

static const FontGlyph *find_zh_glyph(unsigned int cp) {
    int lo = 0;
    int hi = FONT_ZH_GLYPH_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const FontGlyph *glyph = &font_zh_glyphs[mid];
        if (glyph->codepoint == cp) return glyph;
        if (glyph->codepoint < cp) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

/* Decode one UTF-8 code point.  The string table only contains well-formed
 * UTF-8, but malformed input falls back to '?' so a bad byte cannot overrun. */
static unsigned int utf8_next(const unsigned char **cursor) {
    const unsigned char *s = *cursor;
    unsigned char c = *s++;
    if (c < 0x80) {
        *cursor = s;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        if ((s[0] & 0xC0) != 0x80) {
            *cursor = s;
            return '?';
        }
        unsigned int cp = ((unsigned int)(c & 0x1F) << 6)
                        | (unsigned int)(s[0] & 0x3F);
        *cursor = s + 1;
        return cp;
    }
    if ((c & 0xF0) == 0xE0) {
        if ((s[0] & 0xC0) != 0x80 || (s[1] & 0xC0) != 0x80) {
            *cursor = s;
            return '?';
        }
        unsigned int cp = ((unsigned int)(c & 0x0F) << 12)
                        | ((unsigned int)(s[0] & 0x3F) << 6)
                        | (unsigned int)(s[1] & 0x3F);
        *cursor = s + 2;
        return cp;
    }
    if ((c & 0xF8) == 0xF0) {
        if ((s[0] & 0xC0) != 0x80 || (s[1] & 0xC0) != 0x80
            || (s[2] & 0xC0) != 0x80) {
            *cursor = s;
            return '?';
        }
        unsigned int cp = ((unsigned int)(c & 0x07) << 18)
                        | ((unsigned int)(s[0] & 0x3F) << 12)
                        | ((unsigned int)(s[1] & 0x3F) << 6)
                        | (unsigned int)(s[2] & 0x3F);
        *cursor = s + 3;
        return cp;
    }
    *cursor = s;
    return '?';
}

static const FontGlyph *zh_glyph_or_fallback(unsigned int cp) {
    const FontGlyph *glyph = find_zh_glyph(cp);
    if (!glyph) glyph = find_zh_glyph((unsigned int)'?');
    return glyph;
}

void Text_Init(void) {
}

static void draw_english(const char *s, float x, float y, int scale,
                         unsigned int color) {
    float cx = x;
    float cy = y;
    const float gw = (float)(FONT_CELL_W * scale);
    const float gh = (float)(FONT_CELL_H * scale);
    const float advance = (float)(FONT_CELL_W * scale);
    const float line_height = (float)(FONT_CELL_H * scale + 2 * scale);
    const float du = (float)FONT_CELL_W;
    const float dv = (float)FONT_CELL_H;

    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '\n') {
            cx = x;
            cy += line_height;
            continue;
        }
        if (ch == '\r') continue;
        if (ch < FONT_FIRST || ch > FONT_LAST) ch = (unsigned char)'?';
        if (ch != ' ') {
            float u = glyph_u(ch);
            float v = glyph_v(ch);
            Render_DrawTextQuad(cx, cy, gw, gh, u, v, u + du, v + dv, color);
        }
        cx += advance;
    }
}

static void draw_chinese(const char *s, float x, float y, int scale,
                         unsigned int color) {
    const unsigned char *cursor = (const unsigned char *)s;
    float cx = x;
    float cy = y;
    const float gh = (float)(FONT_ZH_CELL_H * scale);
    const float line_height = (float)(FONT_ZH_CELL_H * scale + 2 * scale);

    while (*cursor) {
        unsigned int cp = utf8_next(&cursor);
        if (cp == '\n') {
            cx = x;
            cy += line_height;
            continue;
        }
        if (cp == '\r') continue;
        const FontGlyph *glyph = zh_glyph_or_fallback(cp);
        if (!glyph) continue;
        if (cp != ' ') {
            float u = (float)glyph->u;
            float v = (float)glyph->v;
            Render_DrawTextQuad(cx, cy, (float)glyph->w * scale, gh,
                                u, v, u + (float)glyph->w,
                                v + (float)FONT_ZH_CELL_H, color);
        }
        cx += (float)glyph->advance * scale;
    }
}

void Text_Draw(const char *s, float x, float y, int scale, unsigned int color) {
    if (scale < 1) scale = 1;
    if (Str_GetLanguage() == LANG_ZH) {
        draw_chinese(s, x, y, scale, color);
    } else {
        draw_english(s, x, y, scale, color);
    }
}

void Text_DrawCentered(const char *s, float cx, float y, int scale,
                       unsigned int color) {
    float w = (float)Text_Width(s, scale);
    Text_Draw(s, cx - w * 0.5f, y, scale, color);
}

void Text_DrawRight(const char *s, float rx, float y, int scale,
                    unsigned int color) {
    float w = (float)Text_Width(s, scale);
    Text_Draw(s, rx - w, y, scale, color);
}

void Text_Drawf(float x, float y, int scale, unsigned int color,
                const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Text_Draw(buf, x, y, scale, color);
}

static int width_english(const char *s, int scale) {
    int max_w = 0;
    int line_w = 0;
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '\n') {
            if (line_w > max_w) max_w = line_w;
            line_w = 0;
        } else if (ch != '\r') {
            line_w += FONT_CELL_W * scale;
        }
    }
    if (line_w > max_w) max_w = line_w;
    return max_w;
}

static int width_chinese(const char *s, int scale) {
    const unsigned char *cursor = (const unsigned char *)s;
    int max_w = 0;
    int line_w = 0;
    while (*cursor) {
        unsigned int cp = utf8_next(&cursor);
        if (cp == '\n') {
            if (line_w > max_w) max_w = line_w;
            line_w = 0;
            continue;
        }
        if (cp == '\r') continue;
        const FontGlyph *glyph = zh_glyph_or_fallback(cp);
        if (glyph) line_w += glyph->advance * scale;
    }
    if (line_w > max_w) max_w = line_w;
    return max_w;
}

int Text_Width(const char *s, int scale) {
    if (scale < 1) scale = 1;
    if (Str_GetLanguage() == LANG_ZH) return width_chinese(s, scale);
    return width_english(s, scale);
}

int Text_Height(int scale) {
    if (scale < 1) scale = 1;
    if (Str_GetLanguage() == LANG_ZH) return FONT_ZH_CELL_H * scale;
    return FONT_CELL_H * scale;
}
