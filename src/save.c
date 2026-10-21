#include "save.h"

#include <stdio.h>
#include <string.h>

#ifdef __psp__
#include <pspiofilemgr.h>
#endif

static SaveFile save_file;

static uint32_t checksum_bytes(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static int valid_map(const Map *map) {
    if (!map) return 0;
    if (map->start_x < 0 || map->start_x >= MAP_WIDTH) return 0;
    if (map->start_y < 0 || map->start_y >= MAP_HEIGHT) return 0;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const MapTile *tile = &map->tiles[y][x];
            if (tile->terrain >= TERRAIN_COUNT) return 0;
            if (tile->resource >= RESOURCE_COUNT) return 0;
            if (tile->improvement >= IMPROVEMENT_COUNT) return 0;
        }
    }
    return 1;
}

static int valid_world(const World *world) {
    if (!world) return 0;
    if (world->turn < 1 || world->turn > 100000) return 0;
    if (world->units.count < 0 || world->units.count > UNIT_MAX) return 0;
    if (world->cities.count < 0 || world->cities.count > CITY_MAX) return 0;
    if (world->game_over > WORLD_GAME_DEFEAT) return 0;
    for (int i = 0; i < world->units.count; i++) {
        const Unit *unit = &world->units.units[i];
        if (unit->type >= UNIT_TYPE_COUNT) return 0;
        if (unit->owner >= WORLD_CIV_COUNT) return 0;
        if (unit->x < 0 || unit->x >= MAP_WIDTH) return 0;
        if (unit->y < 0 || unit->y >= MAP_HEIGHT) return 0;
    }
    for (int i = 0; i < world->cities.count; i++) {
        const City *city = &world->cities.cities[i];
        if (city->owner >= WORLD_CIV_COUNT) return 0;
        if (city->x < 0 || city->x >= MAP_WIDTH) return 0;
        if (city->y < 0 || city->y >= MAP_HEIGHT) return 0;
        if (city->name_index >= CITY_NAME_COUNT) return 0;
        if (city->population < 1 || city->population > CITY_MAX_POP) return 0;
    }
    return 1;
}

static int valid_view(const SaveView *view) {
    if (!view) return 0;
    if (view->cursor_x < 0 || view->cursor_x >= MAP_WIDTH) return 0;
    if (view->cursor_y < 0 || view->cursor_y >= MAP_HEIGHT) return 0;
    if (view->selected < -1 || view->selected >= UNIT_MAX) return 0;
    if (view->cam_x != view->cam_x || view->cam_y != view->cam_y) return 0;
    if (view->cam_x < -100000.0f || view->cam_x > 100000.0f) return 0;
    if (view->cam_y < -100000.0f || view->cam_y > 100000.0f) return 0;
    return 1;
}

void Save_Init(void) {
#ifdef __psp__
    /* The SAVEDATA directory is not guaranteed to exist on a fresh card. */
    (void)sceIoMkdir("ms0:/PSP", 0777);
    (void)sceIoMkdir("ms0:/PSP/SAVEDATA", 0777);
    (void)sceIoMkdir(SAVE_DIR, 0777);
#endif
}

const char *Save_SlotPath(int slot) {
    switch (slot) {
        case SAVE_SLOT_AUTO: return SAVE_AUTO;
        case 1: return SAVE_SLOT1;
        case 2: return SAVE_SLOT2;
        case 3: return SAVE_SLOT3;
        default: return NULL;
    }
}

SaveResult Save_WritePath(const char *path, const Map *map,
                          const World *world, const SaveView *view,
                          uint32_t mode, uint32_t remaining_seconds,
                          uint32_t play_seconds, uint32_t limit_seconds,
                          uint32_t player_score, uint32_t rng_state) {
    if (!path || !valid_map(map) || !valid_world(world)
        || !valid_view(view)) {
        return SAVE_ERR_ARG;
    }
    if (mode > SAVE_MODE_TIMED) return SAVE_ERR_ARG;

    memset(&save_file, 0, sizeof(save_file));
    save_file.header.magic = SAVE_MAGIC;
    save_file.header.version = SAVE_VERSION;
    save_file.header.size = (uint32_t)sizeof(SaveFile);
    save_file.header.mode = mode;
    save_file.header.turn = (uint32_t)world->turn;
    save_file.header.remaining_seconds = remaining_seconds;
    save_file.header.play_seconds = play_seconds;
    save_file.header.limit_seconds = limit_seconds;
    save_file.header.map_seed = map->seed;
    save_file.header.rng_state = rng_state;
    save_file.header.player_score = player_score;
    save_file.map = *map;
    save_file.world = *world;
    save_file.view = *view;

    save_file.header.checksum = 0;
    save_file.header.checksum = checksum_bytes(&save_file, sizeof(save_file));

    FILE *file = fopen(path, "wb");
    if (!file) return SAVE_ERR_OPEN;
    size_t written = fwrite(&save_file, 1, sizeof(save_file), file);
    int close_rc = fclose(file);
    if (written != sizeof(save_file) || close_rc != 0) return SAVE_ERR_WRITE;
    return SAVE_OK;
}

static SaveResult read_save_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return SAVE_ERR_OPEN;
    size_t read = fread(&save_file, 1, sizeof(save_file), file);
    int extra = fgetc(file);
    fclose(file);
    if (read != sizeof(save_file) || extra != EOF) return SAVE_ERR_READ;

    if (save_file.header.magic != SAVE_MAGIC) return SAVE_ERR_FORMAT;
    if (save_file.header.version != SAVE_VERSION) return SAVE_ERR_VERSION;
    if (save_file.header.size != (uint32_t)sizeof(SaveFile)) {
        return SAVE_ERR_FORMAT;
    }

    uint32_t stored_checksum = save_file.header.checksum;
    save_file.header.checksum = 0;
    uint32_t actual_checksum = checksum_bytes(&save_file, sizeof(save_file));
    save_file.header.checksum = stored_checksum;
    if (stored_checksum != actual_checksum) return SAVE_ERR_CHECKSUM;

    if (save_file.header.mode > SAVE_MODE_TIMED) return SAVE_ERR_FORMAT;
    if (!valid_map(&save_file.map) || !valid_world(&save_file.world)
        || !valid_view(&save_file.view)) {
        return SAVE_ERR_FORMAT;
    }
    return SAVE_OK;
}

SaveResult Save_ReadPath(const char *path, Map *map, World *world,
                         SaveView *view, SaveHeader *header) {
    if (!path || !map || !world || !view || !header) return SAVE_ERR_ARG;
    SaveResult result = read_save_file(path);
    if (result != SAVE_OK) return result;

    *map = save_file.map;
    *world = save_file.world;
    *view = save_file.view;
    *header = save_file.header;
    return SAVE_OK;
}

SaveResult Save_PeekPath(const char *path, SaveHeader *header) {
    if (!path || !header) return SAVE_ERR_ARG;
    SaveResult result = read_save_file(path);
    if (result != SAVE_OK) return result;
    *header = save_file.header;
    return SAVE_OK;
}

SaveResult Save_WriteSlot(int slot, const Map *map, const World *world,
                          const SaveView *view, uint32_t mode,
                          uint32_t remaining_seconds, uint32_t play_seconds,
                          uint32_t limit_seconds, uint32_t player_score,
                          uint32_t rng_state) {
    const char *path = Save_SlotPath(slot);
    if (!path) return SAVE_ERR_ARG;
    return Save_WritePath(path, map, world, view, mode, remaining_seconds,
                          play_seconds, limit_seconds, player_score,
                          rng_state);
}

SaveResult Save_ReadSlot(int slot, Map *map, World *world, SaveView *view,
                         SaveHeader *header) {
    const char *path = Save_SlotPath(slot);
    if (!path) return SAVE_ERR_ARG;
    return Save_ReadPath(path, map, world, view, header);
}

SaveResult Save_PeekSlot(int slot, SaveHeader *header) {
    const char *path = Save_SlotPath(slot);
    if (!path) return SAVE_ERR_ARG;
    return Save_PeekPath(path, header);
}
