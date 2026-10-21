#ifndef CIVPSP_GAME_H
#define CIVPSP_GAME_H

#include <stdint.h>

#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  272
#define BUFFER_WIDTH   512

#define VERSION_STRING "V0.1"

/* GU colours are AABBGGRR.  The UI source colours are written as AARRGGBB
 * (the convention used by most web/art tools), then converted here. */
#define FIXC(c) (((c) & 0xFF00FF00u) | (((c) & 0xFFu) << 16) | (((c) >> 16) & 0xFFu))

#define ARRAY_SIZE(a) ((int)(sizeof(a) / sizeof((a)[0])))

#endif
