#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "map.h"

static const char *terrain_name(int terrain) {
    static const char *const names[TERRAIN_COUNT] = {
        "grassland", "plains", "forest", "hills", "mountain",
        "desert", "tundra", "coast", "ocean"
    };
    if (terrain < 0 || terrain >= TERRAIN_COUNT) return "?";
    return names[terrain];
}

static const char *resource_name(int resource) {
    static const char *const names[RESOURCE_COUNT] = {
        "none", "horses", "iron", "stone", "gems"
    };
    if (resource < 0 || resource >= RESOURCE_COUNT) return "?";
    return names[resource];
}

static char terrain_char(int terrain) {
    static const char chars[TERRAIN_COUNT] = {
        'G', 'P', 'F', 'H', 'M', 'D', 'T', 'c', '~'
    };
    if (terrain < 0 || terrain >= TERRAIN_COUNT) return '?';
    return chars[terrain];
}

static int walkable_at(const Map *map, int x, int y) {
    return Map_IsLand(map, x, y)
        && Map_TerrainAt(map, x, y) != TERRAIN_MOUNTAIN;
}

static int count_reachable_walkable(const Map *map) {
    static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    uint8_t seen[MAP_TILE_COUNT] = { 0 };
    int stack[MAP_TILE_COUNT];
    int top = 0;
    int count = 0;

    if (!walkable_at(map, map->start_x, map->start_y)) {
        return -1;
    }
    int start = map->start_y * MAP_WIDTH + map->start_x;
    stack[top++] = start;
    seen[start] = 1;
    while (top > 0) {
        int index = stack[--top];
        int x = index % MAP_WIDTH;
        int y = index / MAP_WIDTH;
        count++;
        for (int n = 0; n < 8; n++) {
            int nx = x + dx[n];
            int ny = y + dy[n];
            if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
                continue;
            }
            int ni = ny * MAP_WIDTH + nx;
            if (!seen[ni] && walkable_at(map, nx, ny)) {
                seen[ni] = 1;
                stack[top++] = ni;
            }
        }
    }
    return count;
}

static int count_reachable_land(const Map *map) {
    static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    uint8_t seen[MAP_TILE_COUNT] = { 0 };
    int stack[MAP_TILE_COUNT];
    int top = 0;
    int count = 0;

    int start = map->start_y * MAP_WIDTH + map->start_x;
    if (!Map_IsLand(map, map->start_x, map->start_y)) {
        return -1;
    }
    stack[top++] = start;
    seen[start] = 1;

    while (top > 0) {
        int index = stack[--top];
        int x = index % MAP_WIDTH;
        int y = index / MAP_WIDTH;
        count++;

        for (int n = 0; n < 8; n++) {
            int nx = x + dx[n];
            int ny = y + dy[n];
            if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
                continue;
            }
            int ni = ny * MAP_WIDTH + nx;
            if (!seen[ni] && Map_IsLand(map, nx, ny)) {
                seen[ni] = 1;
                stack[top++] = ni;
            }
        }
    }
    return count;
}

static void dump_map(const Map *map) {
    int terrain_counts[TERRAIN_COUNT] = { 0 };
    int resource_counts[RESOURCE_COUNT] = { 0 };
    int land_count = 0;

    printf("seed=%08X start=(%d,%d)\n", (unsigned)map->seed,
           map->start_x, map->start_y);

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const MapTile *tile = &map->tiles[y][x];
            terrain_counts[tile->terrain]++;
            resource_counts[tile->resource]++;
            if (Map_IsLand(map, x, y)) land_count++;
            char c = terrain_char(tile->terrain);
            if (x == map->start_x && y == map->start_y) {
                c = '@';
            } else if (tile->resource == RESOURCE_HORSES) {
                c = 'h';
            } else if (tile->resource == RESOURCE_IRON) {
                c = 'i';
            } else if (tile->resource == RESOURCE_STONE) {
                c = 's';
            } else if (tile->resource == RESOURCE_GEMS) {
                c = 'j';
            }
            putchar(c);
        }
        putchar('\n');
    }

    printf("land=%d reachable=%d\n", land_count, count_reachable_land(map));
    for (int i = 0; i < TERRAIN_COUNT; i++) {
        printf("terrain %-9s %d\n", terrain_name(i), terrain_counts[i]);
    }
    for (int i = 0; i < RESOURCE_COUNT; i++) {
        printf("resource %-6s %d\n", resource_name(i), resource_counts[i]);
    }
}

static int check_map(const Map *map) {
    int errors = 0;
    int land_count = 0;
    int walkable_count = 0;
    int terrain_counts[TERRAIN_COUNT] = { 0 };
    int resource_counts[RESOURCE_COUNT] = { 0 };

    if (!Map_IsLand(map, map->start_x, map->start_y)) {
        printf("error: start tile is not land\n");
        errors++;
    }

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const MapTile *tile = &map->tiles[y][x];
            if (tile->terrain >= TERRAIN_COUNT) {
                printf("error: invalid terrain at %d,%d\n", x, y);
                errors++;
            }
            if (tile->resource >= RESOURCE_COUNT) {
                printf("error: invalid resource at %d,%d\n", x, y);
                errors++;
            }
            if (tile->resource != RESOURCE_NONE && !Map_IsLand(map, x, y)) {
                printf("error: water resource at %d,%d\n", x, y);
                errors++;
            }
            if (Map_IsLand(map, x, y)) land_count++;
            if (walkable_at(map, x, y)) walkable_count++;
            terrain_counts[tile->terrain]++;
            resource_counts[tile->resource]++;
        }
    }

    int reachable = count_reachable_land(map);
    if (reachable != land_count) {
        printf("error: land is not fully connected (%d of %d reachable)\n",
               reachable, land_count);
        errors++;
    }
    int reachable_walkable = count_reachable_walkable(map);
    if (reachable_walkable != walkable_count) {
        printf("error: walkable terrain is not fully connected "
               "(%d of %d reachable)\n", reachable_walkable, walkable_count);
        errors++;
    }
    /* Keep worlds land-rich but with a real ocean rim, and require the full
     * terrain mix so every map can show off coasts, desert, tundra, and
     * mountains. */
    if (land_count * 100 < MAP_TILE_COUNT * 60) {
        printf("error: land fraction too low (%d of %d tiles)\n",
               land_count, MAP_TILE_COUNT);
        errors++;
    }
    if (land_count * 100 > MAP_TILE_COUNT * 85) {
        printf("error: land fraction too high (%d of %d tiles)\n",
               land_count, MAP_TILE_COUNT);
        errors++;
    }
    for (int terrain = 0; terrain < TERRAIN_COUNT; terrain++) {
        if (terrain_counts[terrain] == 0) {
            printf("error: terrain %s is missing\n", terrain_name(terrain));
            errors++;
        }
    }
    for (int resource = RESOURCE_HORSES; resource < RESOURCE_COUNT; resource++) {
        if (resource_counts[resource] == 0) {
            printf("error: resource %s is missing\n", resource_name(resource));
            errors++;
        }
    }

    Map_ResetFog((Map *)map);
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->tiles[y][x].explored || map->tiles[y][x].visible) {
                printf("error: fog reset failed at %d,%d\n", x, y);
                errors++;
            }
        }
    }
    Map_UpdateVisibility((Map *)map, map->start_x, map->start_y, 4);
    if (!map->tiles[map->start_y][map->start_x].visible
        || !map->tiles[map->start_y][map->start_x].explored) {
        printf("error: start tile is not visible\n");
        errors++;
    }
    Map_UpdateVisibility((Map *)map, map->start_x + 5, map->start_y, 4);
    if (!map->tiles[map->start_y][map->start_x].explored) {
        printf("error: explored tile lost its explored flag\n");
        errors++;
    }
    if (map->tiles[map->start_y][map->start_x].visible) {
        printf("error: distant tile is still visible\n");
        errors++;
    }

    return errors;
}

int main(int argc, char **argv) {
    int dump = 0;
    uint32_t first_seed = 0x12345678u;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump") == 0) {
            dump = 1;
        } else {
            first_seed = (uint32_t)strtoul(argv[i], NULL, 0);
        }
    }

    Map a;
    Map b;
    Map c;
    int errors = 0;

    Map_Generate(&a, first_seed);
    Map_Generate(&b, first_seed);
    if (memcmp(&a, &b, sizeof(a)) != 0) {
        printf("error: same seed produced different maps\n");
        errors++;
    }
    Map_Generate(&c, first_seed ^ 0xFFFFFFFFu);
    if (memcmp(&a, &c, sizeof(a)) == 0) {
        printf("error: different seeds produced identical maps\n");
        errors++;
    }

    if (dump) {
        dump_map(&a);
    }

    errors += check_map(&a);

    for (uint32_t seed = 1; seed <= 100; seed++) {
        Map m;
        Map_Generate(&m, seed * 0x9E3779B9u);
        errors += check_map(&m);
    }

    /* Seeds that previously produced a walkable pocket enclosed by
     * mountains (1 and 35 tiles respectively) before the connectivity pass. */
    {
        Map pocket;
        Map_Generate(&pocket, 0xF1BBCDC9u);
        errors += check_map(&pocket);
        Map_Generate(&pocket, 0xCC147C65u);
        errors += check_map(&pocket);
    }

    if (errors != 0) {
        printf("FAILED: %d error(s)\n", errors);
        return 1;
    }
    printf("PASS: deterministic map generation and fog invariants\n");
    return 0;
}
