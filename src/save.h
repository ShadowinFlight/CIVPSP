#ifndef CIVPSP_SAVE_H
#define CIVPSP_SAVE_H

#include <stdint.h>

#include "map.h"
#include "world.h"

#define SAVE_MAGIC   0x50564943u  /* "CIVP" little-endian */
#define SAVE_VERSION 3u

#define SAVE_DIR   "ms0:/PSP/SAVEDATA/CIVPSP"
#define SAVE_AUTO  SAVE_DIR "/auto.sav"
#define SAVE_SLOT1 SAVE_DIR "/slot1.sav"
#define SAVE_SLOT2 SAVE_DIR "/slot2.sav"
#define SAVE_SLOT3 SAVE_DIR "/slot3.sav"

#define SAVE_SLOT_AUTO 0
#define SAVE_SLOT_FIRST 1
#define SAVE_SLOT_LAST 3

typedef enum {
    SAVE_MODE_CLASSIC = 0,
    SAVE_MODE_ENDLESS = 1
} SaveMode;

typedef enum {
    SAVE_OK = 0,
    SAVE_ERR_ARG,
    SAVE_ERR_OPEN,
    SAVE_ERR_READ,
    SAVE_ERR_WRITE,
    SAVE_ERR_FORMAT,
    SAVE_ERR_VERSION,
    SAVE_ERR_CHECKSUM
} SaveResult;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t checksum;
    uint32_t mode;
    uint32_t turn;
    uint32_t remaining_seconds;
    uint32_t play_seconds;
    uint32_t limit_seconds;
    uint32_t map_seed;
    uint32_t rng_state;
    uint32_t player_score;
    uint32_t turn_limit;        /* V0.2 classic limit; 0 in endless */
    uint32_t reserved[3];
} SaveHeader;

/* Camera/cursor state is stored separately from the map so loading a save
 * returns the player to the same view. */
typedef struct {
    int32_t cursor_x;
    int32_t cursor_y;
    int32_t selected;
    float cam_x;
    float cam_y;
} SaveView;

typedef struct {
    SaveHeader header;
    Map map;
    World world;
    SaveView view;
} SaveFile;

void Save_Init(void);
const char *Save_SlotPath(int slot);

SaveResult Save_WritePath(const char *path, const Map *map,
                          const World *world, const SaveView *view,
                          uint32_t mode, uint32_t turn_limit,
                          uint32_t play_seconds, uint32_t player_score,
                          uint32_t rng_state);
SaveResult Save_ReadPath(const char *path, Map *map, World *world,
                         SaveView *view, SaveHeader *header);
SaveResult Save_PeekPath(const char *path, SaveHeader *header);

SaveResult Save_WriteSlot(int slot, const Map *map, const World *world,
                          const SaveView *view, uint32_t mode,
                          uint32_t turn_limit, uint32_t play_seconds,
                          uint32_t player_score, uint32_t rng_state);
SaveResult Save_ReadSlot(int slot, Map *map, World *world, SaveView *view,
                         SaveHeader *header);
SaveResult Save_PeekSlot(int slot, SaveHeader *header);

#endif
