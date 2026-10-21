#include "audio.h"

#include <pspaudio.h>
#include <pspkernel.h>
#include <psputils.h>

#include <stdint.h>
#include <string.h>

#include "gen/audio_gen.h"
#include "log.h"

/* The assets are generated at 22050Hz, but the PSP's normal hardware output
 * channel runs at 44100Hz.  Duplicating each source frame is cheap and avoids
 * relying on the SRC channel path, which is less consistent on real PSP 1000
 * hardware. */
#define AUDIO_OUTPUT_SAMPLES 1024
#define AUDIO_OUTPUT_RATE 44100
#define AUDIO_MUSIC_GAIN 100
#define AUDIO_MUSIC_DUCK_GAIN 20
#define AUDIO_SFX_GAIN 100

typedef struct {
    const short *data;
    int length;
} AudioClip;

static const AudioClip sfx_clips[AUDIO_SFX_COUNT] = {
    [AUDIO_SFX_MOVE] = { audio_sfx_move, AUDIO_SFX_MOVE_SAMPLES },
    [AUDIO_SFX_SELECT] = { audio_sfx_select, AUDIO_SFX_SELECT_SAMPLES },
    [AUDIO_SFX_BACK] = { audio_sfx_back, AUDIO_SFX_BACK_SAMPLES },
    [AUDIO_SFX_TURN] = { audio_sfx_turn, AUDIO_SFX_TURN_SAMPLES },
    [AUDIO_SFX_BUILD] = { audio_sfx_build, AUDIO_SFX_BUILD_SAMPLES },
    [AUDIO_SFX_FOUND] = { audio_sfx_found, AUDIO_SFX_FOUND_SAMPLES },
    [AUDIO_SFX_COMBAT] = { audio_sfx_combat, AUDIO_SFX_COMBAT_SAMPLES },
    [AUDIO_SFX_RESEARCH] = { audio_sfx_research, AUDIO_SFX_RESEARCH_SAMPLES },
};

static short mix_buffer[AUDIO_OUTPUT_SAMPLES * 2]
    __attribute__((aligned(64)));

static SceUID audio_thread_id = -1;
static int audio_channel = -1;
static volatile int audio_available;
static volatile int audio_stop;
static volatile int music_enabled = 1;
static volatile int pending_sfx = -1;

static int audio_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    int music_pos = 0;
    int active_sfx = -1;
    int sfx_pos = 0;
    int output_error_logged = 0;

    while (!audio_stop) {
        int requested = pending_sfx;
        if (requested >= 0) {
            pending_sfx = -1;
            if (requested < AUDIO_SFX_COUNT) {
                active_sfx = requested;
                sfx_pos = 0;
            }
        }

        /* Two output frames per generated source frame (22050 -> 44100). */
        for (int i = 0; i < AUDIO_OUTPUT_SAMPLES; i += 2) {
            int32_t sample = 0;
            if (music_enabled) {
                int music_gain = active_sfx >= 0
                                     ? AUDIO_MUSIC_DUCK_GAIN
                                     : AUDIO_MUSIC_GAIN;
                sample += (int32_t)audio_ambient[music_pos]
                          * music_gain / 100;
            }
            if (active_sfx >= 0) {
                const AudioClip *clip = &sfx_clips[active_sfx];
                if (sfx_pos < clip->length) {
                    sample += (int32_t)clip->data[sfx_pos]
                              * AUDIO_SFX_GAIN / 100;
                    sfx_pos++;
                }
                if (sfx_pos >= clip->length) {
                    active_sfx = -1;
                }
            }
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            mix_buffer[i * 2] = (short)sample;
            mix_buffer[i * 2 + 1] = (short)sample;
            mix_buffer[(i + 1) * 2] = (short)sample;
            mix_buffer[(i + 1) * 2 + 1] = (short)sample;
            music_pos++;
            if (music_pos >= AUDIO_AMBIENT_SAMPLES) {
                music_pos = 0;
            }
        }

        /* Audio DMA reads main memory directly; the mix buffer is cached. */
        sceKernelDcacheWritebackRange(mix_buffer, sizeof(mix_buffer));
        int rc = sceAudioOutputPannedBlocking(audio_channel,
                                              PSP_AUDIO_VOLUME_MAX,
                                              PSP_AUDIO_VOLUME_MAX,
                                              mix_buffer);
        if (rc < 0 && !output_error_logged) {
            Log_Printf("audio: output failed rc=%d channel=%d",
                       rc, audio_channel);
            output_error_logged = 1;
        }
    }
    return 0;
}

void Audio_Init(void) {
    audio_available = 0;
    audio_stop = 0;
    pending_sfx = -1;

    audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
                                      AUDIO_OUTPUT_SAMPLES,
                                      PSP_AUDIO_FORMAT_STEREO);
    if (audio_channel < 0) {
        Log_Printf("audio: channel reserve failed rc=%d", audio_channel);
        audio_channel = -1;
        return;
    }

    audio_thread_id = sceKernelCreateThread("civ_audio", audio_thread,
                                            0x12, 0x2000, 0, NULL);
    if (audio_thread_id < 0) {
        Log_Printf("audio: thread create failed rc=%d", audio_thread_id);
        sceAudioChRelease(audio_channel);
        audio_channel = -1;
        return;
    }
    if (sceKernelStartThread(audio_thread_id, 0, NULL) < 0) {
        Log_Printf("audio: thread start failed");
        sceKernelDeleteThread(audio_thread_id);
        audio_thread_id = -1;
        sceAudioChRelease(audio_channel);
        audio_channel = -1;
        return;
    }

    audio_available = 1;
    Log_Printf("audio: channel=%d %dHz stereo, ambient %d source samples",
               audio_channel, AUDIO_OUTPUT_RATE, AUDIO_AMBIENT_SAMPLES);
}

void Audio_Shutdown(void) {
    if (!audio_available) return;
    audio_stop = 1;
    if (audio_thread_id >= 0) {
        sceKernelWaitThreadEnd(audio_thread_id, NULL);
        sceKernelDeleteThread(audio_thread_id);
        audio_thread_id = -1;
    }
    int release_rc = -1;
    for (int tries = 0; tries < 6; tries++) {
        release_rc = sceAudioChRelease(audio_channel);
        if (release_rc >= 0) break;
        sceKernelDelayThread(20000);
    }
    audio_channel = -1;
    audio_available = 0;
    Log_Printf("audio: shutdown rc=%d", release_rc);
}

void Audio_PlaySfx(AudioSfx sfx) {
    if (!audio_available || sfx < 0 || sfx >= AUDIO_SFX_COUNT) return;
    pending_sfx = (int)sfx;
}

void Audio_SetMusicEnabled(int enabled) {
    music_enabled = enabled ? 1 : 0;
}

int Audio_MusicEnabled(void) {
    return music_enabled;
}

int Audio_Available(void) {
    return audio_available;
}
