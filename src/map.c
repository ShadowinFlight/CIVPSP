#include "map.h"

#include <string.h>

/* ------------------------------------------------------------------------- */
/* Deterministic integer noise                                               */
/* ------------------------------------------------------------------------- */

static uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static uint32_t hash2(uint32_t seed, int x, int y) {
    uint32_t h = seed;
    h ^= (uint32_t)x * 0x9E3779B1u;
    h ^= (uint32_t)y * 0x85EBCA77u;
    return hash_u32(h);
}

static int clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static int lerp255(int a, int b, int t) {
    return a + ((b - a) * t) / 255;
}

static int smooth255(int t) {
    return (t * t * (3 * 255 - 2 * t)) / (255 * 255);
}

static int value_noise(uint32_t seed, int x, int y, int cell) {
    int gx = x / cell;
    int gy = y / cell;
    int fx = ((x % cell) * 255) / cell;
    int fy = ((y % cell) * 255) / cell;
    int sx = smooth255(fx);
    int sy = smooth255(fy);

    int v00 = (int)(hash2(seed, gx, gy) & 255u);
    int v10 = (int)(hash2(seed, gx + 1, gy) & 255u);
    int v01 = (int)(hash2(seed, gx, gy + 1) & 255u);
    int v11 = (int)(hash2(seed, gx + 1, gy + 1) & 255u);

    int top = lerp255(v00, v10, sx);
    int bottom = lerp255(v01, v11, sx);
    return lerp255(top, bottom, sy);
}

static int fbm(uint32_t seed, int x, int y) {
    int n = value_noise(seed, x, y, 4) * 4
          + value_noise(seed ^ 0xA53C9E1Du, x, y, 8) * 2
          + value_noise(seed ^ 0x5F356495u, x, y, 16);
    return n / 7;
}

/* ------------------------------------------------------------------------- */
/* Generation helpers                                                        */
/* ------------------------------------------------------------------------- */

static int is_land_terrain(int terrain) {
    return terrain != TERRAIN_OCEAN && terrain != TERRAIN_COAST;
}

static int is_land_tile(const MapTile *tile) {
    return is_land_terrain((int)tile->terrain);
}

static int is_walkable_tile(const MapTile *tile) {
    return is_land_tile(tile) && tile->terrain != TERRAIN_MOUNTAIN;
}

/* Convert every land component except the largest one to ocean.  The game
 * currently has no navy, so this guarantees that every reachable land tile is
 * part of one connected continent. */
static void keep_largest_landmass(Map *map) {
    static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    uint16_t component[MAP_TILE_COUNT];
    int stack[MAP_TILE_COUNT];
    MapTile *tiles = &map->tiles[0][0];
    int next_component = 0;
    int best_component = -1;
    int best_size = 0;

    for (int i = 0; i < MAP_TILE_COUNT; i++) {
        component[i] = 0xFFFFu;
    }

    for (int sy = 0; sy < MAP_HEIGHT; sy++) {
        for (int sx = 0; sx < MAP_WIDTH; sx++) {
            int start = sy * MAP_WIDTH + sx;
            if (component[start] != 0xFFFFu || !is_land_tile(&tiles[start])) {
                continue;
            }

            int top = 0;
            int size = 0;
            int cid = next_component++;
            stack[top++] = start;
            component[start] = (uint16_t)cid;

            while (top > 0) {
                int index = stack[--top];
                int x = index % MAP_WIDTH;
                int y = index / MAP_WIDTH;
                size++;

                for (int n = 0; n < 8; n++) {
                    int nx = x + dx[n];
                    int ny = y + dy[n];
                    if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
                        continue;
                    }
                    int ni = ny * MAP_WIDTH + nx;
                    if (component[ni] != 0xFFFFu || !is_land_tile(&tiles[ni])) {
                        continue;
                    }
                    component[ni] = (uint16_t)cid;
                    stack[top++] = ni;
                }
            }

            if (size > best_size) {
                best_size = size;
                best_component = cid;
            }
        }
    }

    if (best_component < 0) {
        return;
    }

    for (int i = 0; i < MAP_TILE_COUNT; i++) {
        if (component[i] != 0xFFFFu && component[i] != (uint16_t)best_component) {
            tiles[i].terrain = TERRAIN_OCEAN;
        }
    }
}

/* keep_largest_landmass() only guarantees land connectivity, but mountains are
 * land while being impassable to units.  A ring of mountains can therefore
 * enclose a passable valley.  This pass finds every walkable component, keeps
 * the largest one, and carves the shortest mountain pass to each remaining
 * component.  If a component cannot be reached through mountains at all, its
 * walkable tiles become mountains so no unit can be trapped there. */
static void connect_walkable_areas(Map *map) {
    static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    static uint8_t connected[MAP_TILE_COUNT];
    static int16_t component[MAP_TILE_COUNT];
    static int16_t parent[MAP_TILE_COUNT];
    static int stack[MAP_TILE_COUNT];
    static int queue[MAP_TILE_COUNT];
    MapTile *tiles = &map->tiles[0][0];

    memset(connected, 0, sizeof(connected));
    for (int i = 0; i < MAP_TILE_COUNT; i++) {
        component[i] = -1;
    }

    int best_component = -1;
    int best_size = 0;
    int next_component = 0;
    for (int sy = 0; sy < MAP_HEIGHT; sy++) {
        for (int sx = 0; sx < MAP_WIDTH; sx++) {
            int start = sy * MAP_WIDTH + sx;
            if (component[start] != -1 || !is_walkable_tile(&tiles[start])) {
                continue;
            }

            int top = 0;
            int size = 0;
            stack[top++] = start;
            component[start] = (int16_t)next_component;
            while (top > 0) {
                int index = stack[--top];
                int x = index % MAP_WIDTH;
                int y = index / MAP_WIDTH;
                size++;
                for (int n = 0; n < 8; n++) {
                    int nx = x + dx[n];
                    int ny = y + dy[n];
                    if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
                        continue;
                    }
                    int ni = ny * MAP_WIDTH + nx;
                    if (component[ni] != -1 || !is_walkable_tile(&tiles[ni])) {
                        continue;
                    }
                    component[ni] = (int16_t)next_component;
                    stack[top++] = ni;
                }
            }
            if (size > best_size) {
                best_size = size;
                best_component = next_component;
            }
            next_component++;
        }
    }

    if (best_component < 0) {
        return;
    }

    for (int i = 0; i < MAP_TILE_COUNT; i++) {
        if (component[i] == (int16_t)best_component) {
            connected[i] = 1;
        }
    }

    for (;;) {
        int target = -1;
        int via = -1;
        int head = 0;
        int tail = 0;

        for (int i = 0; i < MAP_TILE_COUNT; i++) {
            parent[i] = -1;
        }

        /* Start the search from every tile already in the main component and
         * expand only through mountain tiles. */
        for (int i = 0; i < MAP_TILE_COUNT && target < 0; i++) {
            if (!connected[i]) {
                continue;
            }
            int x = i % MAP_WIDTH;
            int y = i / MAP_WIDTH;
            for (int n = 0; n < 8; n++) {
                int nx = x + dx[n];
                int ny = y + dy[n];
                if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
                    continue;
                }
                int ni = ny * MAP_WIDTH + nx;
                if (is_walkable_tile(&tiles[ni])) {
                    if (!connected[ni]) {
                        target = ni;
                        via = i;
                        break;
                    }
                } else if (tiles[ni].terrain == TERRAIN_MOUNTAIN
                           && parent[ni] < 0) {
                    parent[ni] = (int16_t)i;
                    queue[tail++] = ni;
                }
            }
        }

        while (head < tail && target < 0) {
            int index = queue[head++];
            int x = index % MAP_WIDTH;
            int y = index / MAP_WIDTH;
            for (int n = 0; n < 8; n++) {
                int nx = x + dx[n];
                int ny = y + dy[n];
                if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
                    continue;
                }
                int ni = ny * MAP_WIDTH + nx;
                if (is_walkable_tile(&tiles[ni])) {
                    if (!connected[ni]) {
                        target = ni;
                        via = index;
                        break;
                    }
                } else if (tiles[ni].terrain == TERRAIN_MOUNTAIN
                           && parent[ni] < 0) {
                    parent[ni] = (int16_t)index;
                    queue[tail++] = ni;
                }
            }
        }

        if (target < 0) {
            break;
        }

        /* Turn the mountain tiles on the shortest path into hills, creating a
         * pass that is traversable by the normal 8-way movement rules. */
        for (int index = via; index >= 0 && !connected[index];
             index = parent[index]) {
            if (tiles[index].terrain == TERRAIN_MOUNTAIN) {
                tiles[index].terrain = TERRAIN_HILLS;
            }
        }

        /* Mark the newly reached component (and anything the pass touched) as
         * connected before looking for the next pocket. */
        int top = 0;
        stack[top++] = target;
        connected[target] = 1;
        while (top > 0) {
            int index = stack[--top];
            int x = index % MAP_WIDTH;
            int y = index / MAP_WIDTH;
            for (int n = 0; n < 8; n++) {
                int nx = x + dx[n];
                int ny = y + dy[n];
                if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
                    continue;
                }
                int ni = ny * MAP_WIDTH + nx;
                if (is_walkable_tile(&tiles[ni]) && !connected[ni]) {
                    connected[ni] = 1;
                    stack[top++] = ni;
                }
            }
        }
    }

    /* Safety net for a pocket separated by something other than mountains. */
    for (int i = 0; i < MAP_TILE_COUNT; i++) {
        if (is_walkable_tile(&tiles[i]) && !connected[i]) {
            tiles[i].terrain = TERRAIN_MOUNTAIN;
        }
    }
}

static int has_land_neighbor(const MapTile *tiles, int x, int y) {
    static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    for (int n = 0; n < 8; n++) {
        int nx = x + dx[n];
        int ny = y + dy[n];
        if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) {
            continue;
        }
        if (is_land_tile(&tiles[ny * MAP_WIDTH + nx])) {
            return 1;
        }
    }
    return 0;
}

static void classify_coast(Map *map) {
    MapTile *tiles = &map->tiles[0][0];
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            MapTile *tile = &tiles[y * MAP_WIDTH + x];
            if (tile->terrain == TERRAIN_OCEAN && has_land_neighbor(tiles, x, y)) {
                tile->terrain = TERRAIN_COAST;
            }
        }
    }
}

static void ensure_central_land(Map *map) {
    int any_land = 0;
    for (int y = 0; y < MAP_HEIGHT && !any_land; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (is_land_tile(&map->tiles[y][x])) {
                any_land = 1;
                break;
            }
        }
    }
    if (any_land) {
        return;
    }

    for (int y = MAP_HEIGHT / 2 - 2; y <= MAP_HEIGHT / 2 + 2; y++) {
        for (int x = MAP_WIDTH / 2 - 2; x <= MAP_WIDTH / 2 + 2; x++) {
            map->tiles[y][x].terrain = (y & 1) ? TERRAIN_PLAINS : TERRAIN_GRASSLAND;
        }
    }
}

static int resource_allowed(int terrain, int resource) {
    switch (resource) {
        case RESOURCE_HORSES:
            return terrain == TERRAIN_GRASSLAND || terrain == TERRAIN_PLAINS;
        case RESOURCE_IRON:
            return terrain == TERRAIN_HILLS || terrain == TERRAIN_MOUNTAIN
                || terrain == TERRAIN_TUNDRA;
        case RESOURCE_STONE:
            return terrain == TERRAIN_HILLS || terrain == TERRAIN_MOUNTAIN
                || terrain == TERRAIN_DESERT;
        case RESOURCE_GEMS:
            return terrain == TERRAIN_HILLS || terrain == TERRAIN_FOREST
                || terrain == TERRAIN_DESERT || terrain == TERRAIN_TUNDRA;
        default:
            return 0;
    }
}

static int choose_resource(int terrain, uint32_t roll) {
    switch (terrain) {
        case TERRAIN_GRASSLAND:
        case TERRAIN_PLAINS:
            return RESOURCE_HORSES;
        case TERRAIN_FOREST:
            return RESOURCE_GEMS;
        case TERRAIN_HILLS:
            if (roll < 40u) return RESOURCE_IRON;
            if (roll < 75u) return RESOURCE_STONE;
            return RESOURCE_GEMS;
        case TERRAIN_MOUNTAIN:
            if (roll < 50u) return RESOURCE_IRON;
            if (roll < 85u) return RESOURCE_STONE;
            return RESOURCE_GEMS;
        case TERRAIN_DESERT:
            if (roll < 80u) return RESOURCE_STONE;
            return RESOURCE_GEMS;
        case TERRAIN_TUNDRA:
            if (roll < 60u) return RESOURCE_IRON;
            return RESOURCE_GEMS;
        default:
            return RESOURCE_NONE;
    }
}

static void place_resources(Map *map) {
    MapTile *tiles = &map->tiles[0][0];
    int counts[RESOURCE_COUNT] = { 0 };

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            MapTile *tile = &tiles[y * MAP_WIDTH + x];
            if (!is_land_tile(tile)) {
                continue;
            }
            uint32_t h = hash2(map->seed ^ 0x51ED270Bu, x, y);
            if ((h % 100u) >= 7u) {
                continue;
            }
            int resource = choose_resource((int)tile->terrain, (h >> 9) % 100u);
            if (resource != RESOURCE_NONE && resource_allowed((int)tile->terrain, resource)) {
                tile->resource = (uint8_t)resource;
                counts[resource]++;
            }
        }
    }

    /* Keep the M1 demo from ever producing a map with a missing resource
     * type.  Scan in row-major order so the fallback is deterministic too. */
    for (int resource = RESOURCE_HORSES; resource < RESOURCE_COUNT; resource++) {
        if (counts[resource] != 0) {
            continue;
        }
        for (int i = 0; i < MAP_TILE_COUNT; i++) {
            MapTile *tile = &tiles[i];
            if (is_land_tile(tile) && tile->resource == RESOURCE_NONE
                && resource_allowed((int)tile->terrain, resource)) {
                tile->resource = (uint8_t)resource;
                counts[resource]++;
                break;
            }
        }
    }
}

static void choose_start(Map *map) {
    int best_x = -1;
    int best_y = -1;
    int best_score = 0x7FFFFFFF;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            const MapTile *tile = &map->tiles[y][x];
            if (!is_land_tile(tile) || tile->terrain == TERRAIN_MOUNTAIN) {
                continue;
            }
            int dx = x - MAP_WIDTH / 2;
            int dy = y - MAP_HEIGHT / 2;
            int score = dx * dx + dy * dy;
            if (tile->terrain == TERRAIN_HILLS) score += 12;
            if (tile->terrain == TERRAIN_DESERT) score += 8;
            if (tile->terrain == TERRAIN_TUNDRA) score += 20;
            if (score < best_score) {
                best_score = score;
                best_x = x;
                best_y = y;
            }
        }
    }

    if (best_x < 0) {
        best_x = MAP_WIDTH / 2;
        best_y = MAP_HEIGHT / 2;
    }
    map->start_x = best_x;
    map->start_y = best_y;
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

void Map_Generate(Map *map, uint32_t seed) {
    memset(map, 0, sizeof(*map));
    map->seed = seed;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int elevation_noise = fbm(seed ^ 0xE1A7C3D5u, x, y);
            int moisture = fbm(seed ^ 0x7B4D29F1u, x, y);

            /* Normalized elliptical continent falloff: a large central
             * landmass with an ocean rim at the map borders.  The weights are
             * scaled to the map aspect so bigger maps keep the same shape. */
            int half_w = MAP_WIDTH / 2;
            int half_h = MAP_HEIGHT / 2;
            int dx = x - half_w;
            int dy = y - half_h;
            int qx = (dx * dx * 256) / (half_w * half_w);
            int qy = (dy * dy * 256) / (half_h * half_h);
            int falloff = (qx * 3 + qy * 4) / 7;
            int continent = 205 - falloff * 11 / 16;
            int elevation = clamp255(continent + (elevation_noise - 128) * 3 / 4);

            int terrain;
            if (elevation < 120) {
                terrain = TERRAIN_OCEAN; /* coast is assigned after connectivity */
            } else if (elevation >= 215) {
                terrain = TERRAIN_MOUNTAIN;
            } else if (elevation >= 190) {
                terrain = TERRAIN_HILLS;
            } else {
                int cold_band = MAP_HEIGHT / 7;
                int cold = (y < cold_band) || (y >= MAP_HEIGHT - cold_band);
                int warm = (y >= MAP_HEIGHT * 3 / 10) && (y < MAP_HEIGHT * 7 / 10);
                if (cold) {
                    terrain = TERRAIN_TUNDRA;
                } else if (warm && moisture < 75) {
                    terrain = TERRAIN_DESERT;
                } else if (moisture > 155) {
                    terrain = TERRAIN_FOREST;
                } else if (moisture > 120) {
                    terrain = TERRAIN_GRASSLAND;
                } else {
                    terrain = TERRAIN_PLAINS;
                }
            }

            map->tiles[y][x].terrain = (uint8_t)terrain;
        }
    }

    keep_largest_landmass(map);
    ensure_central_land(map);
    connect_walkable_areas(map);
    classify_coast(map);
    place_resources(map);
    choose_start(map);
}

void Map_ResetFog(Map *map) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->tiles[y][x].explored = 0;
            map->tiles[y][x].visible = 0;
        }
    }
}

void Map_ClearVisibility(Map *map) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->tiles[y][x].visible = 0;
        }
    }
}

void Map_MarkVisibleArea(Map *map, int cx, int cy, int radius) {
    if (!map || radius < 0) return;
    int r2 = radius * radius;
    int x0 = cx - radius;
    int y0 = cy - radius;
    int x1 = cx + radius;
    int y1 = cy + radius;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= MAP_WIDTH) x1 = MAP_WIDTH - 1;
    if (y1 >= MAP_HEIGHT) y1 = MAP_HEIGHT - 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= r2) {
                map->tiles[y][x].explored = 1;
                map->tiles[y][x].visible = 1;
            }
        }
    }
}

void Map_RevealArea(Map *map, int cx, int cy, int radius) {
    if (radius < 0) return;
    int r2 = radius * radius;
    int x0 = cx - radius;
    int y0 = cy - radius;
    int x1 = cx + radius;
    int y1 = cy + radius;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= MAP_WIDTH) x1 = MAP_WIDTH - 1;
    if (y1 >= MAP_HEIGHT) y1 = MAP_HEIGHT - 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= r2) {
                map->tiles[y][x].explored = 1;
            }
        }
    }
}

void Map_UpdateVisibility(Map *map, int cx, int cy, int radius) {
    Map_ClearVisibility(map);
    Map_MarkVisibleArea(map, cx, cy, radius);
}

MapTile *Map_TileAt(Map *map, int x, int y) {
    if (!map || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
        return NULL;
    }
    return &map->tiles[y][x];
}

const MapTile *Map_TileAtConst(const Map *map, int x, int y) {
    if (!map || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
        return NULL;
    }
    return &map->tiles[y][x];
}

int Map_IsLand(const Map *map, int x, int y) {
    const MapTile *tile = Map_TileAtConst(map, x, y);
    return tile && is_land_tile(tile);
}

int Map_IsWater(const Map *map, int x, int y) {
    const MapTile *tile = Map_TileAtConst(map, x, y);
    return !tile || !is_land_tile(tile);
}

int Map_TerrainAt(const Map *map, int x, int y) {
    const MapTile *tile = Map_TileAtConst(map, x, y);
    return tile ? (int)tile->terrain : TERRAIN_OCEAN;
}

int Map_ResourceAt(const Map *map, int x, int y) {
    const MapTile *tile = Map_TileAtConst(map, x, y);
    return tile ? (int)tile->resource : RESOURCE_NONE;
}
