#include "analog.h"

#define ANALOG_MAX_SAMPLES     16
#define ANALOG_BASE_DEADZONE   32
#define ANALOG_MAX_DEADZONE    56
#define ANALOG_FILTER_DIV      4
#define ANALOG_OUTPUT_MAX      127
#define ANALOG_MIN_OUTPUT      4

static int abs_int(int value) {
    return value < 0 ? -value : value;
}

static int max_int(int a, int b) {
    return a > b ? a : b;
}

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

/* Integer low-pass step.  Plain division stalls three units short of the raw
 * value, which would leave a permanent residual bias after recentering. */
static int filter_toward(int value, int target) {
    int diff = target - value;
    if (diff == 0) return value;
    int step = diff / ANALOG_FILTER_DIV;
    if (step == 0) step = diff > 0 ? 1 : -1;
    return value + step;
}

static void output_zero(AnalogState *state) {
    state->out_x = 0;
    state->out_y = 0;
    state->zero_frames++;
    /* If the output has been zero for a while, the stick is at rest even if
     * the mechanical center has drifted or the user is resting just outside
     * the deadzone.  Follow that position so long sessions do not slowly
     * walk the camera. */
    if (state->zero_frames >= ANALOG_RECENTER_FRAMES) {
        state->center_x = state->filtered_x;
        state->center_y = state->filtered_y;
        state->zero_frames = 0;
    }
}

/* Median rejects a single noisy/spike sample without needing floating point or
 * libm.  The sample count is tiny, so insertion sort is ideal. */
static int median_of(int *values, int count) {
    for (int i = 1; i < count; i++) {
        int value = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > value) {
            values[j + 1] = values[j];
            j--;
        }
        values[j + 1] = value;
    }
    return values[count / 2];
}

void Analog_Calibrate(int *center_x, int *center_y,
                      const int *samples_x, const int *samples_y, int count) {
    int xs[ANALOG_MAX_SAMPLES];
    int ys[ANALOG_MAX_SAMPLES];

    if (!center_x || !center_y) return;
    if (!samples_x || !samples_y || count <= 0) {
        *center_x = ANALOG_RAW_CENTER;
        *center_y = ANALOG_RAW_CENTER;
        return;
    }
    if (count > ANALOG_MAX_SAMPLES) count = ANALOG_MAX_SAMPLES;
    for (int i = 0; i < count; i++) {
        xs[i] = clamp_int(samples_x[i], 0, 255);
        ys[i] = clamp_int(samples_y[i], 0, 255);
    }
    *center_x = median_of(xs, count);
    *center_y = median_of(ys, count);
}

void Analog_Init(AnalogState *state, int center_x, int center_y,
                 int available) {
    if (!state) return;
    state->center_x = clamp_int(center_x, 0, 255);
    state->center_y = clamp_int(center_y, 0, 255);
    state->filtered_x = state->center_x;
    state->filtered_y = state->center_y;

    /* The further the worn stick rests from nominal center, the more noise it
     * usually has.  Scale the deadzone from 32 up to 56 raw units. */
    int bias = max_int(abs_int(state->center_x - ANALOG_RAW_CENTER),
                       abs_int(state->center_y - ANALOG_RAW_CENTER));
    int deadzone = ANALOG_BASE_DEADZONE + bias / 2;
    state->deadzone = clamp_int(deadzone, ANALOG_BASE_DEADZONE,
                                ANALOG_MAX_DEADZONE);

    state->zero_frames = 0;
    state->available = available != 0;
    state->out_x = 0;
    state->out_y = 0;
}

void Analog_Update(AnalogState *state, int raw_x, int raw_y) {
    if (!state) return;
    if (!state->available) {
        state->out_x = 0;
        state->out_y = 0;
        return;
    }

    raw_x = clamp_int(raw_x, 0, 255);
    raw_y = clamp_int(raw_y, 0, 255);

    /* A short low-pass filter removes worn-stick jitter while keeping the
     * camera responsive (roughly a 4-frame time constant at 60 Hz). */
    state->filtered_x = filter_toward(state->filtered_x, raw_x);
    state->filtered_y = filter_toward(state->filtered_y, raw_y);

    int dx = state->filtered_x - state->center_x;
    int dy = state->filtered_y - state->center_y;
    int magnitude = max_int(abs_int(dx), abs_int(dy));

    if (magnitude <= state->deadzone) {
        output_zero(state);
        return;
    }

    int span = ANALOG_OUTPUT_MAX - state->deadzone;
    if (span < 1) span = 1;
    int scaled = (magnitude - state->deadzone) * ANALOG_OUTPUT_MAX / span;
    scaled = clamp_int(scaled, 0, ANALOG_OUTPUT_MAX);

    /* A one-unit output still moves the camera roughly 60 pixels per second.
     * Treat tiny post-deadzone values as rest and let the recenter logic learn
     * that bias instead of drifting forever. */
    if (scaled < ANALOG_MIN_OUTPUT) {
        output_zero(state);
        return;
    }

    state->zero_frames = 0;
    state->out_x = dx * scaled / magnitude;
    state->out_y = dy * scaled / magnitude;
}
