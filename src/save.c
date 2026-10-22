#include "save.h"

#include <stdio.h>
#include <string.h>

#ifdef __psp__
#include <pspiofilemgr.h>
#endif

static SaveFile save_file;

/* Version 2 predates city combat.  Its City was 8 bytes smaller, which shifts
 * every field after the city pool.  Keep an explicit legacy layout so old
 * memory-card saves can be migrated into the V0.2 runtime. */
typedef struct {
    uint8_t owner;
    uint8_t name_index;
    uint8_t population;
    int8_t x;
    int8_t y;
    uint16_t buildings;
    uint8_t queue[CITY_MAX_QUEUE];
    uint8_t queue_count;
    int16_t food_stored;
    int16_t production_stored;
    int16_t influence;
    uint32_t owned_mask;
    uint32_t worked_mask;
    uint32_t locked_mask;
    uint8_t is_capital;
} CityV2;

typedef struct {
    CityV2 cities[CITY_MAX];
    int count;
} CityPoolV2;

typedef struct {
    UnitPool units;
    CityPoolV2 cities;
    CivState civs[WORLD_CIV_COUNT];
    int turn;
    int32_t treasury;
    int32_t science;
    int32_t gold_per_turn;
    int32_t science_per_turn;
    uint32_t researched;
    int32_t research_type;
    int32_t research_progress;
    uint8_t player_founded;
    uint8_t game_over;
    uint16_t battles;
    uint16_t cities_captured;
    int notice_turn;
    char notice[96];
    int8_t relation[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t treaty_mask[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t treaty_turns[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t open_borders_turns[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t trade_turns[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t gift_count[WORLD_CIV_COUNT][WORLD_CIV_COUNT];
    uint8_t proposal_civ;
    uint8_t proposal_type;
    uint8_t proposal_cooldown;
    uint8_t alliance_victory_turns;
    uint8_t victory_type;
} WorldV2;

typedef struct {
    SaveHeader header;
    Map map;
    WorldV2 world;
    SaveView view;
} SaveFileV2;

static SaveFileV2 save_file_v2;

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
    if (world->difficulty > WORLD_DIFFICULTY_HARD) return 0;
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
        if (city->hp > CITY_HP_MAX) return 0;
        if (city->defense_max > CITY_WALL_DEFENSE) return 0;
        if (city->defense > city->defense_max) return 0;
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

static void normalize_header(SaveHeader *header) {
    if (header->mode == SAVE_MODE_CLASSIC) {
        if (header->turn_limit == 0) {
            header->turn_limit = WORLD_STANDARD_TURN_LIMIT;
        }
    } else {
        header->turn_limit = 0;
    }
}

static void sanitize_world(World *world) {
    if (world->difficulty > WORLD_DIFFICULTY_HARD) {
        world->difficulty = WORLD_DIFFICULTY_NORMAL;
    }
    for (int i = 0; i < world->cities.count; i++) {
        City_UpdateCombat(&world->cities.cities[i]);
    }
}

static void convert_v2(void) {
    save_file.header = save_file_v2.header;
    save_file.map = save_file_v2.map;
    save_file.view = save_file_v2.view;

    World *dst = &save_file.world;
    const WorldV2 *src = &save_file_v2.world;
    memset(dst, 0, sizeof(*dst));

    dst->units = src->units;
    dst->cities.count = src->cities.count;
    for (int i = 0; i < src->cities.count; i++) {
        const CityV2 *old = &src->cities.cities[i];
        City *city = &dst->cities.cities[i];
        city->owner = old->owner;
        city->name_index = old->name_index;
        city->population = old->population;
        city->x = old->x;
        city->y = old->y;
        city->buildings = old->buildings;
        memcpy(city->queue, old->queue, sizeof(city->queue));
        city->queue_count = old->queue_count;
        city->food_stored = old->food_stored;
        city->production_stored = old->production_stored;
        city->influence = old->influence;
        city->owned_mask = old->owned_mask;
        city->worked_mask = old->worked_mask;
        city->locked_mask = old->locked_mask;
        city->is_capital = old->is_capital;
        City_ResetCombat(city);
    }

    for (int i = 0; i < WORLD_CIV_COUNT; i++) {
        dst->civs[i] = src->civs[i];
    }
    dst->turn = src->turn;
    dst->treasury = src->treasury;
    dst->science = src->science;
    dst->gold_per_turn = src->gold_per_turn;
    dst->science_per_turn = src->science_per_turn;
    dst->researched = src->researched;
    dst->research_type = src->research_type;
    dst->research_progress = src->research_progress;
    dst->player_founded = src->player_founded;
    dst->game_over = src->game_over;
    dst->battles = src->battles;
    dst->cities_captured = src->cities_captured;
    dst->notice_turn = src->notice_turn;
    memcpy(dst->notice, src->notice, sizeof(dst->notice));
    memcpy(dst->relation, src->relation, sizeof(dst->relation));
    memcpy(dst->treaty_mask, src->treaty_mask, sizeof(dst->treaty_mask));
    memcpy(dst->treaty_turns, src->treaty_turns, sizeof(dst->treaty_turns));
    memcpy(dst->open_borders_turns, src->open_borders_turns,
           sizeof(dst->open_borders_turns));
    memcpy(dst->trade_turns, src->trade_turns, sizeof(dst->trade_turns));
    memcpy(dst->gift_count, src->gift_count, sizeof(dst->gift_count));
    dst->proposal_civ = src->proposal_civ;
    dst->proposal_type = src->proposal_type;
    dst->proposal_cooldown = src->proposal_cooldown;
    dst->alliance_victory_turns = src->alliance_victory_turns;
    dst->victory_type = src->victory_type;
    dst->difficulty = WORLD_DIFFICULTY_NORMAL;

    /* Standard saves become classic 200-turn games; timed saves become the
     * new endless mode. */
    save_file.header.turn_limit = WORLD_STANDARD_TURN_LIMIT;
    if (save_file.header.mode == SAVE_MODE_ENDLESS) {
        save_file.header.turn_limit = 0;
    }
    save_file.header.remaining_seconds = 0;
    save_file.header.limit_seconds = 0;
    save_file.header.size = (uint32_t)sizeof(SaveFile);
    save_file.header.version = SAVE_VERSION;
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
                          uint32_t mode, uint32_t turn_limit,
                          uint32_t play_seconds, uint32_t player_score,
                          uint32_t rng_state) {
    if (!path || !valid_map(map) || !valid_world(world)
        || !valid_view(view)) {
        return SAVE_ERR_ARG;
    }
    if (mode > SAVE_MODE_ENDLESS) return SAVE_ERR_ARG;

    memset(&save_file, 0, sizeof(save_file));
    save_file.header.magic = SAVE_MAGIC;
    save_file.header.version = SAVE_VERSION;
    save_file.header.size = (uint32_t)sizeof(SaveFile);
    save_file.header.mode = mode;
    save_file.header.turn = (uint32_t)world->turn;
    save_file.header.play_seconds = play_seconds;
    save_file.header.map_seed = map->seed;
    save_file.header.rng_state = rng_state;
    save_file.header.player_score = player_score;
    if (mode == SAVE_MODE_CLASSIC) {
        save_file.header.turn_limit = turn_limit
            ? turn_limit : WORLD_STANDARD_TURN_LIMIT;
    } else {
        save_file.header.turn_limit = 0;
    }
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

    SaveHeader header;
    size_t header_read = fread(&header, 1, sizeof(header), file);
    if (header_read != sizeof(header)) {
        fclose(file);
        return SAVE_ERR_READ;
    }
    if (header.magic != SAVE_MAGIC) {
        fclose(file);
        return SAVE_ERR_FORMAT;
    }
    if (header.version != SAVE_VERSION && header.version != 2u) {
        fclose(file);
        return SAVE_ERR_VERSION;
    }

    rewind(file);
    if (header.version == 2u) {
        size_t read = fread(&save_file_v2, 1, sizeof(save_file_v2), file);
        int extra = fgetc(file);
        fclose(file);
        if (read != sizeof(save_file_v2) || extra != EOF) {
            return SAVE_ERR_READ;
        }
        if (save_file_v2.header.size != (uint32_t)sizeof(SaveFileV2)) {
            return SAVE_ERR_FORMAT;
        }
        uint32_t stored = save_file_v2.header.checksum;
        save_file_v2.header.checksum = 0;
        uint32_t actual = checksum_bytes(&save_file_v2,
                                         sizeof(save_file_v2));
        save_file_v2.header.checksum = stored;
        if (stored != actual) return SAVE_ERR_CHECKSUM;
        if (save_file_v2.header.mode > SAVE_MODE_ENDLESS) {
            return SAVE_ERR_FORMAT;
        }
        if (!valid_map(&save_file_v2.map)) return SAVE_ERR_FORMAT;
        /* Bound the legacy pools before convert_v2 copies them into the
         * fixed-size runtime arrays. */
        if (save_file_v2.world.units.count < 0
            || save_file_v2.world.units.count > UNIT_MAX
            || save_file_v2.world.cities.count < 0
            || save_file_v2.world.cities.count > CITY_MAX) {
            return SAVE_ERR_FORMAT;
        }
        convert_v2();
        if (!valid_world(&save_file.world) || !valid_view(&save_file.view)) {
            return SAVE_ERR_FORMAT;
        }
        sanitize_world(&save_file.world);
        normalize_header(&save_file.header);
        return SAVE_OK;
    }

    size_t read = fread(&save_file, 1, sizeof(save_file), file);
    int extra = fgetc(file);
    fclose(file);
    if (read != sizeof(save_file) || extra != EOF) return SAVE_ERR_READ;
    if (save_file.header.size != (uint32_t)sizeof(SaveFile)) {
        return SAVE_ERR_FORMAT;
    }

    uint32_t stored = save_file.header.checksum;
    save_file.header.checksum = 0;
    uint32_t actual = checksum_bytes(&save_file, sizeof(save_file));
    save_file.header.checksum = stored;
    if (stored != actual) return SAVE_ERR_CHECKSUM;

    if (save_file.header.mode > SAVE_MODE_ENDLESS) return SAVE_ERR_FORMAT;
    if (!valid_map(&save_file.map) || !valid_world(&save_file.world)
        || !valid_view(&save_file.view)) {
        return SAVE_ERR_FORMAT;
    }
    sanitize_world(&save_file.world);
    normalize_header(&save_file.header);
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
                          uint32_t turn_limit, uint32_t play_seconds,
                          uint32_t player_score, uint32_t rng_state) {
    const char *path = Save_SlotPath(slot);
    if (!path) return SAVE_ERR_ARG;
    return Save_WritePath(path, map, world, view, mode, turn_limit,
                          play_seconds, player_score, rng_state);
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
