#ifndef CIVPSP_ANALOG_H
#define CIVPSP_ANALOG_H

/* Raw PSP analog axes are nominally 0..255 with 128 at rest, but PSP 1000
 * sticks often rest well off-center and drift as they warm up.  This module
 * converts raw samples into a stable -127..127 camera axis. */
#define ANALOG_RAW_CENTER       128
#define ANALOG_RECENTER_FRAMES  45

typedef struct {
    int center_x;
    int center_y;
    int filtered_x;
    int filtered_y;
    int deadzone;
    int zero_frames;
    int available;
    int out_x;
    int out_y;
} AnalogState;

/* Robust boot-time center estimate: median of the supplied samples.  The
 * caller provides at least one sample; an empty set falls back to 128,128. */
void Analog_Calibrate(int *center_x, int *center_y,
                      const int *samples_x, const int *samples_y, int count);

/* Reset filtering and derive an adaptive deadzone from how far the resting
 * center is from the nominal 128,128.  Worn sticks therefore get a wider
 * deadzone automatically. */
void Analog_Init(AnalogState *state, int center_x, int center_y, int available);

/* Filter one raw sample and update the processed output.  At rest the center
 * slowly follows residual drift, so long sessions do not accumulate bias. */
void Analog_Update(AnalogState *state, int raw_x, int raw_y);

#endif
