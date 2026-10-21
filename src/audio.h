#ifndef CIVPSP_AUDIO_H
#define CIVPSP_AUDIO_H

typedef enum {
    AUDIO_SFX_MOVE = 0,
    AUDIO_SFX_SELECT,
    AUDIO_SFX_BACK,
    AUDIO_SFX_TURN,
    AUDIO_SFX_BUILD,
    AUDIO_SFX_FOUND,
    AUDIO_SFX_COMBAT,
    AUDIO_SFX_RESEARCH,
    AUDIO_SFX_COUNT
} AudioSfx;

void Audio_Init(void);
void Audio_Shutdown(void);
void Audio_PlaySfx(AudioSfx sfx);
void Audio_SetMusicEnabled(int enabled);
int Audio_MusicEnabled(void);
int Audio_Available(void);

#endif
