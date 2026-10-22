#include <stdio.h>
#include <string.h>

#include "save.h"
#include "tech.h"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void init_fixture(Map *map, World *world, SaveView *view) {
    Map_Generate(map, 0x1234ABCDu);
    memset(world, 0, sizeof(*world));
    world->turn = 7;
    world->treasury = 41;
    world->science = 12;
    world->player_founded = 1;
    world->difficulty = WORLD_DIFFICULTY_HARD;
    world->denounce_used[1] = 1;
    world->researched = (1u << TECH_POTTERY) | (1u << TECH_MINING);
    world->units.count = 1;
    world->units.units[0].type = UNIT_WARRIOR;
    world->units.units[0].owner = WORLD_CIV_PLAYER;
    world->units.units[0].moves_left = 2;
    world->units.units[0].x = (int8_t)map->start_x;
    world->units.units[0].y = (int8_t)map->start_y;
    world->units.units[0].hp = 80;
    world->cities.count = 1;
    world->cities.cities[0].owner = WORLD_CIV_PLAYER;
    world->cities.cities[0].name_index = 0;
    world->cities.cities[0].population = 3;
    world->cities.cities[0].x = (int8_t)map->start_x;
    world->cities.cities[0].y = (int8_t)map->start_y;
    world->cities.cities[0].owned_mask = 1u << CITY_CENTER_INDEX;
    world->cities.cities[0].queue_count = 0;
    world->cities.cities[0].hp = 80;
    world->cities.cities[0].defense = 40;
    world->cities.cities[0].defense_max = CITY_DEFENSE_BASE;
    world->cities.cities[0].last_hit_turn = 3;
    world->civs[1].active = 1;
    world->civs[1].capital_index = WORLD_CIV_NONE;
    world->relation[0][1] = 25;
    world->relation[1][0] = 25;
    world->notice_turn = 7;
    snprintf(world->notice, sizeof(world->notice), "fixture notice");

    view->cursor_x = map->start_x;
    view->cursor_y = map->start_y;
    view->selected = 0;
    view->cam_x = 123.5f;
    view->cam_y = 45.25f;
}

static void test_round_trip(void) {
    const char *path = "/tmp/civpsp_test_save.sav";
    (void)remove(path);
    Save_Init();

    Map map;
    World world;
    SaveView view;
    init_fixture(&map, &world, &view);

    SaveResult result = Save_WritePath(path, &map, &world, &view,
                                       SAVE_MODE_CLASSIC, 200u, 77u,
                                       321u, 0xDEADBEEFu);
    CHECK(result == SAVE_OK);

    SaveHeader peek;
    memset(&peek, 0, sizeof(peek));
    CHECK(Save_PeekPath(path, &peek) == SAVE_OK);
    CHECK(peek.mode == SAVE_MODE_CLASSIC);
    CHECK(peek.turn == 7u);
    CHECK(peek.turn_limit == 200u);
    CHECK(peek.remaining_seconds == 0u);
    CHECK(peek.play_seconds == 77u);
    CHECK(peek.limit_seconds == 0u);
    CHECK(peek.player_score == 321u);
    CHECK(peek.rng_state == 0xDEADBEEFu);

    Map loaded_map;
    World loaded_world;
    SaveView loaded_view;
    SaveHeader header;
    memset(&loaded_map, 0, sizeof(loaded_map));
    memset(&loaded_world, 0, sizeof(loaded_world));
    memset(&loaded_view, 0, sizeof(loaded_view));
    CHECK(Save_ReadPath(path, &loaded_map, &loaded_world, &loaded_view,
                        &header) == SAVE_OK);
    CHECK(memcmp(&map, &loaded_map, sizeof(map)) == 0);
    CHECK(memcmp(&world, &loaded_world, sizeof(world)) == 0);
    CHECK(memcmp(&view, &loaded_view, sizeof(view)) == 0);
    CHECK(header.mode == SAVE_MODE_CLASSIC);
    CHECK(header.turn_limit == 200u);
    CHECK(header.map_seed == map.seed);
    CHECK(loaded_world.cities.cities[0].hp == 80);
    CHECK(loaded_world.cities.cities[0].defense == 40);
    CHECK(loaded_world.difficulty == WORLD_DIFFICULTY_HARD);

    /* The endless mode writes no turn limit. */
    CHECK(Save_WritePath(path, &map, &world, &view, SAVE_MODE_ENDLESS, 200u,
                         77u, 321u, 0xDEADBEEFu) == SAVE_OK);
    CHECK(Save_PeekPath(path, &peek) == SAVE_OK);
    CHECK(peek.mode == SAVE_MODE_ENDLESS);
    CHECK(peek.turn_limit == 0u);
    CHECK(Save_ReadPath(path, &loaded_map, &loaded_world, &loaded_view,
                        &header) == SAVE_OK);
    CHECK(header.mode == SAVE_MODE_ENDLESS);
    CHECK(header.turn_limit == 0u);

    (void)remove(path);
}

static void test_bad_files(void) {
    const char *path = "/tmp/civpsp_test_save_bad.sav";
    (void)remove(path);

    Map map;
    World world;
    SaveView view;
    init_fixture(&map, &world, &view);
    CHECK(Save_WritePath(path, &map, &world, &view, SAVE_MODE_CLASSIC,
                         100u, 0u, 10u, 1u) == SAVE_OK);

    FILE *file = fopen(path, "r+b");
    CHECK(file != NULL);
    if (file) {
        CHECK(fseek(file, (long)sizeof(SaveHeader) + 100L, SEEK_SET) == 0);
        int byte = fgetc(file);
        CHECK(byte != EOF);
        if (byte != EOF) {
            CHECK(fseek(file, (long)sizeof(SaveHeader) + 100L, SEEK_SET) == 0);
            CHECK(fputc(byte ^ 0x5A, file) != EOF);
        }
        fclose(file);
    }

    Map loaded_map;
    World loaded_world;
    SaveView loaded_view;
    SaveHeader header;
    CHECK(Save_ReadPath(path, &loaded_map, &loaded_world, &loaded_view,
                        &header) == SAVE_ERR_CHECKSUM);
    CHECK(Save_PeekPath(path, &header) == SAVE_ERR_CHECKSUM);

    file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file) {
        CHECK(fwrite("short", 1, 5, file) == 5);
        fclose(file);
    }
    CHECK(Save_ReadPath(path, &loaded_map, &loaded_world, &loaded_view,
                        &header) == SAVE_ERR_READ);
    CHECK(Save_PeekPath(path, &header) == SAVE_ERR_READ);

    (void)remove(path);
    CHECK(Save_ReadPath(path, &loaded_map, &loaded_world, &loaded_view,
                        &header) == SAVE_ERR_OPEN);
}

static void test_invalid_fixture(void) {
    const char *path = "/tmp/civpsp_test_save_invalid.sav";
    Map map;
    World world;
    SaveView view;
    init_fixture(&map, &world, &view);

    world.units.units[0].owner = WORLD_CIV_COUNT;
    CHECK(Save_WritePath(path, &map, &world, &view, SAVE_MODE_CLASSIC,
                         200u, 0u, 0u, 0u) == SAVE_ERR_ARG);

    init_fixture(&map, &world, &view);
    view.cursor_x = MAP_WIDTH;
    CHECK(Save_WritePath(path, &map, &world, &view, SAVE_MODE_CLASSIC,
                         200u, 0u, 0u, 0u) == SAVE_ERR_ARG);
    (void)remove(path);
}

/* A copy of the V0.1/V0.2-pre-combat on-disk layouts.  The save reader must
 * still load these memory-card files and convert them to the V0.2 model. */
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
} LegacyCity;

typedef struct {
    LegacyCity cities[CITY_MAX];
    int count;
} LegacyCityPool;

typedef struct {
    UnitPool units;
    LegacyCityPool cities;
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
} LegacyWorld;

typedef struct {
    SaveHeader header;
    Map map;
    LegacyWorld world;
    SaveView view;
} LegacyFile;

static uint32_t checksum_bytes(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static void write_legacy_file(const char *path, uint32_t old_mode) {
    static LegacyFile file;
    memset(&file, 0, sizeof(file));
    Map_Generate(&file.map, 0x0BADF00Du);

    file.header.magic = SAVE_MAGIC;
    file.header.version = 2u;
    file.header.size = (uint32_t)sizeof(file);
    file.header.mode = old_mode;
    file.header.turn = 42u;
    file.header.remaining_seconds = 1234u;
    file.header.play_seconds = 55u;
    file.header.limit_seconds = 5400u;
    file.header.map_seed = file.map.seed;
    file.header.rng_state = 0xCAFEBABEu;
    file.header.player_score = 99u;
    file.map = file.map;

    file.world.turn = 42;
    file.world.treasury = 55;
    file.world.science = 9;
    file.world.player_founded = 1;
    file.world.cities.count = 1;
    file.world.cities.cities[0].owner = 1;
    file.world.cities.cities[0].name_index = 1;
    file.world.cities.cities[0].population = 2;
    file.world.cities.cities[0].x = (int8_t)file.map.start_x;
    file.world.cities.cities[0].y = (int8_t)file.map.start_y;
    file.world.cities.cities[0].owned_mask = 1u << CITY_CENTER_INDEX;
    file.world.cities.cities[0].is_capital = 1;
    file.world.civs[1].active = 1;
    file.world.civs[1].capital_index = 0;
    file.world.relation[0][1] = 15;
    file.world.relation[1][0] = 15;
    file.view.cursor_x = file.map.start_x;
    file.view.cursor_y = file.map.start_y;
    file.view.selected = -1;
    file.view.cam_x = 10.0f;
    file.view.cam_y = 20.0f;

    file.header.checksum = 0;
    file.header.checksum = checksum_bytes(&file, sizeof(file));

    FILE *out = fopen(path, "wb");
    CHECK(out != NULL);
    if (!out) return;
    CHECK(fwrite(&file, 1, sizeof(file), out) == sizeof(file));
    fclose(out);
}

static void test_v2_migration(void) {
    const char *path = "/tmp/civpsp_test_save_v2.sav";
    SaveHeader header;
    Map map;
    World world;
    SaveView view;

    (void)remove(path);
    write_legacy_file(path, 0u);
    CHECK(Save_ReadPath(path, &map, &world, &view, &header) == SAVE_OK);
    CHECK(header.version == SAVE_VERSION);
    CHECK(header.mode == SAVE_MODE_CLASSIC);
    CHECK(header.turn_limit == 200u);
    CHECK(header.remaining_seconds == 0u);
    CHECK(header.limit_seconds == 0u);
    CHECK(header.play_seconds == 55u);
    CHECK(header.player_score == 99u);
    CHECK(world.turn == 42);
    CHECK(world.cities.count == 1);
    CHECK(world.cities.cities[0].hp == CITY_HP_MAX);
    CHECK(world.cities.cities[0].defense == CITY_DEFENSE_BASE);
    CHECK(world.cities.cities[0].defense_max == CITY_DEFENSE_BASE);
    CHECK(world.difficulty == WORLD_DIFFICULTY_NORMAL);
    CHECK(world.denounce_used[1] == 0);

    (void)remove(path);
    write_legacy_file(path, 1u);
    CHECK(Save_ReadPath(path, &map, &world, &view, &header) == SAVE_OK);
    CHECK(header.version == SAVE_VERSION);
    CHECK(header.mode == SAVE_MODE_ENDLESS);
    CHECK(header.turn_limit == 0u);
    CHECK(world.cities.cities[0].hp == CITY_HP_MAX);
    (void)remove(path);
}

int main(void) {
    test_round_trip();
    test_bad_files();
    test_invalid_fixture();
    test_v2_migration();

    if (failures) {
        fprintf(stderr, "%d save check(s) failed\n", failures);
        return 1;
    }
    printf("PASS: save v3 round-trip, v2 migration, corruption, and validation\n");
    return 0;
}
