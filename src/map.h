#ifndef CIVPSP_MAP_H
#define CIVPSP_MAP_H

#include <stdint.h>

#define MAP_WIDTH       64
#define MAP_HEIGHT      48
#define MAP_TILE_PX     24
#define MAP_PIXEL_WIDTH  (MAP_WIDTH * MAP_TILE_PX)
#define MAP_PIXEL_HEIGHT (MAP_HEIGHT * MAP_TILE_PX)
#define MAP_TILE_COUNT   (MAP_WIDTH * MAP_HEIGHT)

typedef enum {
    TERRAIN_GRASSLAND = 0,
    TERRAIN_PLAINS,
    TERRAIN_FOREST,
    TERRAIN_HILLS,
    TERRAIN_MOUNTAIN,
    TERRAIN_DESERT,
    TERRAIN_TUNDRA,
    TERRAIN_COAST,
    TERRAIN_OCEAN,
    TERRAIN_COUNT
} TerrainType;

typedef enum {
    RESOURCE_NONE = 0,
    RESOURCE_HORSES,
    RESOURCE_IRON,
    RESOURCE_STONE,
    RESOURCE_GEMS,
    RESOURCE_COUNT
} ResourceType;

typedef enum {
    IMPROVEMENT_NONE = 0,
    IMPROVEMENT_FARM,
    IMPROVEMENT_MINE,
    IMPROVEMENT_PASTURE,
    IMPROVEMENT_QUARRY,
    IMPROVEMENT_ROAD,
    IMPROVEMENT_COUNT
} ImprovementType;

typedef struct {
    uint8_t terrain;
    uint8_t resource;
    uint8_t improvement;
    uint8_t explored;
    uint8_t visible;
} MapTile;

typedef struct {
    uint32_t seed;
    int start_x;
    int start_y;
    MapTile tiles[MAP_HEIGHT][MAP_WIDTH];
} Map;

/* Pure data generation; no GU, PSP, or global RNG dependency.  The same seed
 * always produces the same terrain, resources, and start position. */
void Map_Generate(Map *map, uint32_t seed);

void Map_ResetFog(Map *map);
void Map_ClearVisibility(Map *map);
void Map_MarkVisibleArea(Map *map, int cx, int cy, int radius);
void Map_RevealArea(Map *map, int cx, int cy, int radius);
void Map_UpdateVisibility(Map *map, int cx, int cy, int radius);

MapTile *Map_TileAt(Map *map, int x, int y);
const MapTile *Map_TileAtConst(const Map *map, int x, int y);
int Map_IsLand(const Map *map, int x, int y);
int Map_IsWater(const Map *map, int x, int y);
int Map_TerrainAt(const Map *map, int x, int y);
int Map_ResourceAt(const Map *map, int x, int y);

#endif
