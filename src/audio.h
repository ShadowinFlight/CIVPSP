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

/* One loop per era and peace/war mood.  The order must match the generated
 * table in src/gen/audio_gen.h and tools/gen_audio.py. */
typedef enum {
    AUDIO_MUSIC_ANCIENT_PEACE = 0,
    AUDIO_MUSIC_ANCIENT_WAR,
    AUDIO_MUSIC_CLASSICAL_PEACE,
    AUDIO_MUSIC_CLASSICAL_WAR,
    AUDIO_MUSIC_MEDIEVAL_PEACE,
    AUDIO_MUSIC_MEDIEVAL_WAR,
    AUDIO_MUSIC_RENAISSANCE_PEACE,
    AUDIO_MUSIC_RENAISSANCE_WAR,
    AUDIO_MUSIC_COUNT
} AudioMusic;

void Audio_Init(void);
void Audio_Shutdown(void);
void Audio_PlaySfx(AudioSfx sfx);
/* Switch tracks with a short crossfade.  Repeated calls with the current
 * track are ignored, so this is safe to call every frame. */
void Audio_SetMusicTrack(AudioMusic track);
void Audio_SetMusicEnabled(int enabled);
int Audio_MusicEnabled(void);
int Audio_Available(void);

#endif
