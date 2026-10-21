#include <stdio.h>

#include "analog.h"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static int abs_int(int value) {
    return value < 0 ? -value : value;
}

static void test_calibration_rejects_spikes(void) {
    int xs[9] = { 120, 121, 119, 200, 120, 122, 120, 121, 120 };
    int ys[9] = { 130, 131, 129, 0, 130, 132, 130, 131, 130 };
    int cx = 0;
    int cy = 0;

    Analog_Calibrate(&cx, &cy, xs, ys, 9);
    CHECK(cx == 120);
    CHECK(cy == 130);

    Analog_Calibrate(&cx, &cy, NULL, NULL, 0);
    CHECK(cx == ANALOG_RAW_CENTER);
    CHECK(cy == ANALOG_RAW_CENTER);
}

static void test_normal_center_deadzone_and_pan(void) {
    AnalogState state;
    Analog_Init(&state, ANALOG_RAW_CENTER, ANALOG_RAW_CENTER, 1);
    CHECK(state.deadzone == 32);

    for (int i = 0; i < 20; i++) {
        Analog_Update(&state, 136, 124);
    }
    CHECK(state.out_x == 0);
    CHECK(state.out_y == 0);

    for (int i = 0; i < 30; i++) {
        Analog_Update(&state, 190, ANALOG_RAW_CENTER);
    }
    CHECK(state.out_x > 0);
    CHECK(state.out_y == 0);
}

static void test_worn_center_gets_wider_deadzone(void) {
    AnalogState state;
    Analog_Init(&state, 165, 133, 1);
    CHECK(state.deadzone >= 48);

    for (int i = 0; i < 60; i++) {
        Analog_Update(&state, 170, 135);
    }
    CHECK(state.out_x == 0);
    CHECK(state.out_y == 0);

    for (int i = 0; i < 120; i++) {
        Analog_Update(&state, 230, 133);
    }
    CHECK(state.out_x > 0);
}

static void test_resting_drift_recenters(void) {
    AnalogState state;
    Analog_Init(&state, ANALOG_RAW_CENTER, ANALOG_RAW_CENTER, 1);

    for (int i = 0; i < ANALOG_RECENTER_FRAMES + 20; i++) {
        Analog_Update(&state, 150, 130);
    }
    CHECK(abs_int(state.center_x - 150) <= 2);
    CHECK(abs_int(state.center_y - 130) <= 2);
    CHECK(state.out_x == 0);
    CHECK(state.out_y == 0);

    for (int i = 0; i < ANALOG_RECENTER_FRAMES + 40; i++) {
        Analog_Update(&state, ANALOG_RAW_CENTER, ANALOG_RAW_CENTER);
    }
    CHECK(abs_int(state.center_x - ANALOG_RAW_CENTER) <= 2);
    CHECK(abs_int(state.center_y - ANALOG_RAW_CENTER) <= 2);
    CHECK(state.out_x == 0);
    CHECK(state.out_y == 0);
}

static void test_held_stick_does_not_recenter(void) {
    AnalogState state;
    Analog_Init(&state, ANALOG_RAW_CENTER, ANALOG_RAW_CENTER, 1);

    for (int i = 0; i < 300; i++) {
        Analog_Update(&state, 200, ANALOG_RAW_CENTER);
    }
    CHECK(state.center_x == ANALOG_RAW_CENTER);
    CHECK(state.out_x > 0);
}

static void test_unavailable_pad_is_ignored(void) {
    AnalogState state;
    Analog_Init(&state, 0, 0, 0);
    Analog_Update(&state, 255, 255);
    CHECK(state.out_x == 0);
    CHECK(state.out_y == 0);
}

int main(void) {
    test_calibration_rejects_spikes();
    test_normal_center_deadzone_and_pan();
    test_worn_center_gets_wider_deadzone();
    test_resting_drift_recenters();
    test_held_stick_does_not_recenter();
    test_unavailable_pad_is_ignored();

    if (failures == 0) {
        printf("PASS: analog calibration, filtering, deadzone, and drift recenter\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
