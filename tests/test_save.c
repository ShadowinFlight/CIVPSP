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
                                       SAVE_MODE_TIMED, 1234u, 77u, 5400u,
                                       321u, 0xDEADBEEFu);
    CHECK(result == SAVE_OK);

    SaveHeader peek;
    memset(&peek, 0, sizeof(peek));
    CHECK(Save_PeekPath(path, &peek) == SAVE_OK);
    CHECK(peek.mode == SAVE_MODE_TIMED);
    CHECK(peek.turn == 7u);
    CHECK(peek.remaining_seconds == 1234u);
    CHECK(peek.play_seconds == 77u);
    CHECK(peek.limit_seconds == 5400u);
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
    CHECK(header.mode == SAVE_MODE_TIMED);
    CHECK(header.map_seed == map.seed);

    (void)remove(path);
}

static void test_bad_files(void) {
    const char *path = "/tmp/civpsp_test_save_bad.sav";
    (void)remove(path);

    Map map;
    World world;
    SaveView view;
    init_fixture(&map, &world, &view);
    CHECK(Save_WritePath(path, &map, &world, &view, SAVE_MODE_STANDARD,
                         0u, 0u, 0u, 10u, 1u) == SAVE_OK);

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
    CHECK(Save_WritePath(path, &map, &world, &view, SAVE_MODE_STANDARD,
                         0u, 0u, 0u, 0u, 0u) == SAVE_ERR_ARG);

    init_fixture(&map, &world, &view);
    view.cursor_x = MAP_WIDTH;
    CHECK(Save_WritePath(path, &map, &world, &view, SAVE_MODE_STANDARD,
                         0u, 0u, 0u, 0u, 0u) == SAVE_ERR_ARG);
    (void)remove(path);
}

int main(void) {
    test_round_trip();
    test_bad_files();
    test_invalid_fixture();

    if (failures) {
        fprintf(stderr, "%d save check(s) failed\n", failures);
        return 1;
    }
    printf("PASS: save round-trip, metadata, corruption, and validation\n");
    return 0;
}
