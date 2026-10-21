#include "input.h"

#include <pspkernel.h>

#include "analog.h"
#include "log.h"

InputState g_input;

static AnalogState analog_state;

static unsigned int prev_buttons = 0;
static unsigned int nav_buttons = 0;
static unsigned int nav_start = 0;
static unsigned int last_repeat = 0;

#define NAV_MASK (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)
#define REPEAT_DELAY_US 250000u
#define REPEAT_INTERVAL_US 80000u
#define ANALOG_CALIBRATION_SAMPLES 15
#define ANALOG_CALIBRATION_DELAY_US 2000

/* Re-sample the resting stick center.  This is done at boot and again when a
 * New Game starts, because a worn PSP 1000 stick can settle differently after
 * the console has been powered on for a while. */
void Input_CalibrateStick(void) {
    int samples_x[ANALOG_CALIBRATION_SAMPLES];
    int samples_y[ANALOG_CALIBRATION_SAMPLES];

    for (int i = 0; i < ANALOG_CALIBRATION_SAMPLES; i++) {
        sceCtrlReadBufferPositive(&g_input.pad, 1);
        samples_x[i] = g_input.pad.Lx;
        samples_y[i] = g_input.pad.Ly;
        sceKernelDelayThread(ANALOG_CALIBRATION_DELAY_US);
    }

    int center_x = 0;
    int center_y = 0;
    Analog_Calibrate(&center_x, &center_y, samples_x, samples_y,
                     ANALOG_CALIBRATION_SAMPLES);
    int available = !(center_x == 0 && center_y == 0);
    Analog_Init(&analog_state, center_x, center_y, available);

    g_input.analog_center_x = analog_state.center_x;
    g_input.analog_center_y = analog_state.center_y;
    g_input.analog_deadzone = analog_state.deadzone;
    g_input.analog_available = analog_state.available;
    g_input.analog_x = 0;
    g_input.analog_y = 0;

    Log_Printf("input: analog center=%d,%d deadzone=%d available=%d",
               g_input.analog_center_x, g_input.analog_center_y,
               g_input.analog_deadzone, g_input.analog_available);
}

void Input_Init(void) {
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    Input_CalibrateStick();

    sceCtrlReadBufferPositive(&g_input.pad, 1);
    prev_buttons = g_input.pad.Buttons;
    g_input.held = prev_buttons;
    g_input.pressed = 0;
    g_input.released = 0;
    g_input.repeated = 0;
}

void Input_Update(void) {
    sceCtrlReadBufferPositive(&g_input.pad, 1);
    unsigned int cur = g_input.pad.Buttons;
    unsigned int now = (unsigned int)sceKernelGetSystemTimeLow();

    Analog_Update(&analog_state, g_input.pad.Lx, g_input.pad.Ly);
    g_input.analog_x = analog_state.out_x;
    g_input.analog_y = analog_state.out_y;
    g_input.analog_center_x = analog_state.center_x;
    g_input.analog_center_y = analog_state.center_y;
    g_input.analog_deadzone = analog_state.deadzone;
    g_input.analog_available = analog_state.available;

    g_input.held = cur;
    g_input.pressed = cur & ~prev_buttons;
    g_input.released = prev_buttons & ~cur;
    g_input.repeated = 0;

    unsigned int dirs = cur & NAV_MASK;
    if (dirs != nav_buttons) {
        nav_buttons = dirs;
        nav_start = now;
        last_repeat = now;
    } else if (dirs && (unsigned int)(now - nav_start) >= REPEAT_DELAY_US) {
        if ((unsigned int)(now - last_repeat) >= REPEAT_INTERVAL_US) {
            g_input.repeated = dirs;
            last_repeat = now;
        }
    }

    prev_buttons = cur;
}

int Input_Pressed(unsigned int mask) {
    return (g_input.pressed & mask) != 0;
}

int Input_Repeated(unsigned int mask) {
    return ((g_input.pressed | g_input.repeated) & mask) != 0;
}

int Input_Held(unsigned int mask) {
    return (g_input.held & mask) != 0;
}
