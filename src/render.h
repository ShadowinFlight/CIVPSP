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
/* Alpha-gradient terrain transitions use the separate RGBA transition atlas
 * and are flushed after the base terrain sprites but before detail rects. */
void Render_DrawTransitionSprite(float x, float y, float w, float h,
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
/* Flush before changing this flag so a whole text batch uses one filter. */
void Render_SetTextSmooth(int smooth);

#endif
