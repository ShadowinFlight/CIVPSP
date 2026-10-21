#ifndef CIVPSP_RENDER_H
#define CIVPSP_RENDER_H

void Render_Init(void);
void Render_BeginFrame(void);
void Render_EndFrame(void);
void Render_FlushLayer(void);
void Render_DrawRect(float x, float y, float w, float h, unsigned int color);
void Render_DrawSprite(float x, float y, float w, float h,
                       float u0, float v0, float u1, float v1,
                       unsigned int color);
void Render_DrawTextQuad(float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1,
                         unsigned int color);
void Render_DrawMinimap(float x, float y, float w, float h,
                        const unsigned short *pixels, int tex_w, int tex_h,
                        float u1, float v1, unsigned int color);
void Render_SetScissor(int x, int y, int w, int h);
void Render_SetClearColor(unsigned int color);
void Render_SetFontLanguage(int language);

#endif
