#ifndef CIVPSP_TEXT_H
#define CIVPSP_TEXT_H

void Text_Init(void);
void Text_Draw(const char *s, float x, float y, int scale, unsigned int color);
void Text_DrawCentered(const char *s, float cx, float y, int scale, unsigned int color);
void Text_DrawRight(const char *s, float rx, float y, int scale, unsigned int color);
void Text_Drawf(float x, float y, int scale, unsigned int color, const char *fmt, ...);
int Text_Width(const char *s, int scale);
int Text_Height(int scale);

#endif
