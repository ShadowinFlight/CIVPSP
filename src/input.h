#ifndef CIVPSP_INPUT_H
#define CIVPSP_INPUT_H

#include <pspctrl.h>

typedef struct {
    SceCtrlData pad;
    unsigned int held;
    unsigned int pressed;
    unsigned int released;
    unsigned int repeated;
    /* Analog stick, already filtered/deadzoned and relative to the adaptive
     * center.  analog_center/deadzone are exposed for boot diagnostics. */
    int analog_x;
    int analog_y;
    int analog_center_x;
    int analog_center_y;
    int analog_deadzone;
    int analog_available;
} InputState;

extern InputState g_input;

void Input_Init(void);
void Input_CalibrateStick(void);
void Input_Update(void);

/* These return a boolean (1/0), not the masked button bits.  Use one call per
 * button when a caller needs to distinguish directions. */
int Input_Pressed(unsigned int mask);
int Input_Repeated(unsigned int mask);
int Input_Held(unsigned int mask);

#endif
