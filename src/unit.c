#include "unit.h"

#include <string.h>

const UnitDef g_unit_defs[UNIT_TYPE_COUNT] = {
    [UNIT_SETTLER] = { 6, 2, 0, 1 },
    [UNIT_WARRIOR] = { 6, 2, 6, 0 },
    [UNIT_SCOUT]   = { 8, 3, 3, 0 },
    [UNIT_SWORDSMAN] = { 5, 2, 8, 0 },
    [UNIT_HORSEMAN]  = { 10, 2, 4, 0 },
    [UNIT_WORKER]    = { 4, 2, 0, 0 },
    [UNIT_MEDIC]     = { 4, 2, 0, 0 },
};

/* Dijkstra scratch space.  The game is single-threaded and the search budget
 * is at most every unit's movement points, so static buffers are safe. */
static uint16_t path_dist[MAP_TILE_COUNT];
static uint16_t path_heap[MAP_TILE_COUNT];
static uint8_t path_closed[MAP_TILE_COUNT];

void UnitPool_Init(UnitPool *pool) {
    memset(pool, 0, sizeof(*pool));
}

int Unit_TypeMovePoints(int type) {
    if (type < 0 || type >= UNIT_TYPE_COUNT) return 0;
    return g_unit_defs[type].move_points;
}

int Unit_TypeSight(int type) {
    if (type < 0 || type >= UNIT_TYPE_COUNT) return 0;
    return g_unit_defs[type].sight;
}

int Unit_Spawn(UnitPool *pool, int type, int owner, int x, int y) {
    if (!pool || pool->count >= UNIT_MAX) return -1;
    if (type < 0 || type >= UNIT_TYPE_COUNT) return -1;
    if (owner < 0 || owner > 255) return -1;
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return -1;
    if (Unit_IndexOfAt(pool, x, y) >= 0) return -1;

    int index = pool->count++;
    Unit *unit = &pool->units[index];
    unit->type = (uint8_t)type;
    unit->owner = (uint8_t)owner;
    unit->moves_left = g_unit_defs[type].move_points;
    unit->x = (int8_t)x;
    unit->y = (int8_t)y;
    unit->hp = UNIT_HP_MAX;
    unit->acted = 0;
    return index;
}

int Unit_IndexOfAt(const UnitPool *pool, int x, int y) {
    if (!pool) return -1;
    for (int i = 0; i < pool->count; i++) {
        if (pool->units[i].x == x && pool->units[i].y == y) return i;
    }
    return -1;
}

const Unit *Unit_AtConst(const UnitPool *pool, int x, int y) {
    int index = Unit_IndexOfAt(pool, x, y);
    return index >= 0 ? &pool->units[index] : NULL;
}

Unit *Unit_At(UnitPool *pool, int x, int y) {
    int index = Unit_IndexOfAt(pool, x, y);
    return index >= 0 ? &pool->units[index] : NULL;
}

void Unit_RemoveAt(UnitPool *pool, int index) {
    if (!pool || index < 0 || index >= pool->count) return;
    for (int i = index; i < pool->count - 1; i++) {
        pool->units[i] = pool->units[i + 1];
    }
    pool->count--;
}

int Unit_PassableTile(const Map *map, int x, int y) {
    if (!map || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return 0;
    int terrain = Map_TerrainAt(map, x, y);
    if (terrain == TERRAIN_MOUNTAIN) return 0;
    return Map_IsLand(map, x, y);
}

int Unit_StepCost(const Map *map, int from_x, int from_y, int to_x, int to_y) {
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1) return -1;
    if (dx == 0 && dy == 0) return -1;
    if (!Unit_PassableTile(map, to_x, to_y)) return -1;

    int cost = (dx != 0 && dy != 0) ? 3 : 2;
    int terrain = Map_TerrainAt(map, to_x, to_y);
    if (terrain == TERRAIN_FOREST || terrain == TERRAIN_HILLS) cost += 2;
    /* Roads speed entry into the improved tile.  An empty road is still
     * useful: flat ground drops from 2 to 1 per step. */
    if (map->tiles[to_y][to_x].improvement == IMPROVEMENT_ROAD && cost > 1) {
        cost--;
    }
    return cost;
}

static void heap_push(int node, int *heap_len) {
    /* M2's largest move budget is 8 points, so the reachable region (and this
     * heap, even with duplicate pushes) is far smaller than MAP_TILE_COUNT.
     * Keep a hard guard so a future larger budget cannot corrupt memory. */
    if (*heap_len >= MAP_TILE_COUNT) return;
    int i = (*heap_len)++;
    path_heap[i] = (uint16_t)node;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (path_dist[path_heap[parent]] <= path_dist[path_heap[i]]) break;
        uint16_t tmp = path_heap[parent];
        path_heap[parent] = path_heap[i];
        path_heap[i] = tmp;
        i = parent;
    }
}

static int heap_pop(int *heap_len) {
    int top = path_heap[0];
    (*heap_len)--;
    if (*heap_len > 0) {
        path_heap[0] = path_heap[*heap_len];
        int i = 0;
        for (;;) {
            int left = i * 2 + 1;
            int right = left + 1;
            int best = i;
            if (left < *heap_len
                && path_dist[path_heap[left]] < path_dist[path_heap[best]]) {
                best = left;
            }
            if (right < *heap_len
                && path_dist[path_heap[right]] < path_dist[path_heap[best]]) {
                best = right;
            }
            if (best == i) break;
            uint16_t tmp = path_heap[best];
            path_heap[best] = path_heap[i];
            path_heap[i] = tmp;
            i = best;
        }
    }
    return top;
}

static int path_search(const UnitPool *pool, const Map *map, int self_index,
                       int sx, int sy, int tx, int ty, int max_points) {
    if (!Unit_PassableTile(map, tx, ty)) return -1;
    int target_unit = Unit_IndexOfAt(pool, tx, ty);
    if (target_unit >= 0 && target_unit != self_index) return -1;
    if (sx == tx && sy == ty) return 0;

    static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    memset(path_dist, 0xFF, sizeof(path_dist));
    memset(path_closed, 0, sizeof(path_closed));

    int start = sy * MAP_WIDTH + sx;
    int target = ty * MAP_WIDTH + tx;
    int heap_len = 0;
    path_dist[start] = 0;
    heap_push(start, &heap_len);

    while (heap_len > 0) {
        int node = heap_pop(&heap_len);
        if (path_closed[node]) continue;
        path_closed[node] = 1;
        int cost_so_far = path_dist[node];
        if (node == target) return cost_so_far;

        int x = node % MAP_WIDTH;
        int y = node / MAP_WIDTH;
        for (int n = 0; n < 8; n++) {
            int nx = x + dx[n];
            int ny = y + dy[n];
            if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) continue;
            int next = ny * MAP_WIDTH + nx;
            if (path_closed[next]) continue;

            int other = Unit_IndexOfAt(pool, nx, ny);
            if (other >= 0 && other != self_index) continue;

            int step = Unit_StepCost(map, x, y, nx, ny);
            if (step < 0) continue;
            int cost = cost_so_far + step;
            if (cost > max_points) continue;
            if (cost < path_dist[next]) {
                path_dist[next] = (uint16_t)cost;
                heap_push(next, &heap_len);
            }
        }
    }
    return -1;
}

int Unit_PathCost(const UnitPool *pool, const Map *map, const Unit *unit,
                  int tx, int ty, int max_points) {
    if (!pool || !map || !unit) return -1;
    if (max_points <= 0) return -1;
    int self_index = Unit_IndexOfAt(pool, unit->x, unit->y);
    return path_search(pool, map, self_index, unit->x, unit->y,
                       tx, ty, max_points);
}

int Unit_MoveTo(UnitPool *pool, const Map *map, int index, int tx, int ty) {
    if (!pool || !map) return -1;
    if (index < 0 || index >= pool->count) return -1;
    Unit *unit = &pool->units[index];
    if (unit->moves_left == 0) return -1;

    int cost = path_search(pool, map, index, unit->x, unit->y,
                           tx, ty, unit->moves_left);
    if (cost < 0) return -1;
    unit->x = (int8_t)tx;
    unit->y = (int8_t)ty;
    unit->moves_left = (uint8_t)(unit->moves_left - cost);
    return cost;
}

/* ------------------------------------------------------------------------- */
/* A* pathing for AI movement                                                */
/* ------------------------------------------------------------------------- */

#define ASTAR_OPEN_MAX (MAP_TILE_COUNT * 2)

static uint16_t astar_dist[MAP_TILE_COUNT];
static uint16_t astar_f[MAP_TILE_COUNT];
static int16_t astar_parent[MAP_TILE_COUNT];
static uint8_t astar_closed[MAP_TILE_COUNT];
static uint16_t astar_heap[ASTAR_OPEN_MAX];

static int chebyshev(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax;
    int dy = ay > by ? ay - by : by - ay;
    return dx > dy ? dx : dy;
}

static void astar_push(int node, int *heap_len) {
    if (*heap_len >= ASTAR_OPEN_MAX) return;
    int i = (*heap_len)++;
    astar_heap[i] = (uint16_t)node;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (astar_f[astar_heap[parent]] <= astar_f[astar_heap[i]]) break;
        uint16_t tmp = astar_heap[parent];
        astar_heap[parent] = astar_heap[i];
        astar_heap[i] = tmp;
        i = parent;
    }
}

static int astar_pop(int *heap_len) {
    int top = astar_heap[0];
    (*heap_len)--;
    if (*heap_len > 0) {
        astar_heap[0] = astar_heap[*heap_len];
        int i = 0;
        for (;;) {
            int left = i * 2 + 1;
            int right = left + 1;
            int best = i;
            if (left < *heap_len
                && astar_f[astar_heap[left]] < astar_f[astar_heap[best]]) {
                best = left;
            }
            if (right < *heap_len
                && astar_f[astar_heap[right]] < astar_f[astar_heap[best]]) {
                best = right;
            }
            if (best == i) break;
            uint16_t tmp = astar_heap[best];
            astar_heap[best] = astar_heap[i];
            astar_heap[i] = tmp;
            i = best;
        }
    }
    return top;
}

int Unit_PathNextStep(const UnitPool *pool, const Map *map, int unit_index,
                      int tx, int ty, int *next_x, int *next_y) {
    if (!pool || !map || !next_x || !next_y) return -1;
    if (unit_index < 0 || unit_index >= pool->count) return -1;
    if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT) return -1;
    if (!Unit_PassableTile(map, tx, ty)) return -1;

    const Unit *self = &pool->units[unit_index];
    int sx = self->x;
    int sy = self->y;
    int start = sy * MAP_WIDTH + sx;
    int target = ty * MAP_WIDTH + tx;
    int target_occupied = Unit_IndexOfAt(pool, tx, ty) >= 0
                       && !(tx == sx && ty == sy);

    if (start == target) return 0;
    if (target_occupied && chebyshev(sx, sy, tx, ty) <= 1) return 0;

    static const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    memset(astar_dist, 0xFF, sizeof(astar_dist));
    memset(astar_parent, 0xFF, sizeof(astar_parent));
    memset(astar_closed, 0, sizeof(astar_closed));

    astar_dist[start] = 0;
    astar_f[start] = (uint16_t)((target_occupied ? 0 : 2)
                                * chebyshev(sx, sy, tx, ty));
    int heap_len = 0;
    astar_push(start, &heap_len);

    int goal = -1;
    while (heap_len > 0) {
        int node = astar_pop(&heap_len);
        if (astar_closed[node]) continue;
        astar_closed[node] = 1;

        int x = node % MAP_WIDTH;
        int y = node / MAP_WIDTH;
        if (node == target
            || (target_occupied && chebyshev(x, y, tx, ty) == 1)) {
            goal = node;
            break;
        }

        for (int n = 0; n < 8; n++) {
            int nx = x + dx[n];
            int ny = y + dy[n];
            if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) continue;
            int next = ny * MAP_WIDTH + nx;
            if (astar_closed[next]) continue;
            if (Unit_IndexOfAt(pool, nx, ny) >= 0) continue;

            int step = Unit_StepCost(map, x, y, nx, ny);
            if (step < 0) continue;
            int cost = (int)astar_dist[node] + step;
            if (cost > 0xFFFF) continue;
            if (cost >= astar_dist[next]) continue;

            astar_dist[next] = (uint16_t)cost;
            astar_parent[next] = (int16_t)node;
            int h = 2 * chebyshev(nx, ny, tx, ty);
            if (target_occupied) h = 2 * (chebyshev(nx, ny, tx, ty) - 1);
            if (h < 0) h = 0;
            astar_f[next] = (uint16_t)(cost + h);
            astar_push(next, &heap_len);
        }
    }

    if (goal < 0) return -1;

    int node = goal;
    while (astar_parent[node] >= 0 && astar_parent[node] != start) {
        node = astar_parent[node];
    }
    if (astar_parent[node] != start) return -1;

    *next_x = node % MAP_WIDTH;
    *next_y = node / MAP_WIDTH;
    return 1;
}

int Unit_AutoExploreEnabled(const Unit *unit) {
    return unit && (unit->acted & UNIT_AUTO_EXPLORE_FLAG) != 0;
}

void Unit_SetAutoExplore(Unit *unit, int enabled) {
    if (!unit) return;
    if (enabled) {
        unit->acted |= UNIT_AUTO_EXPLORE_FLAG;
    } else {
        unit->acted &= (uint8_t)~UNIT_AUTO_EXPLORE_FLAG;
    }
}

void Unit_ResetTurn(UnitPool *pool) {
    if (!pool) return;
    for (int i = 0; i < pool->count; i++) {
        int type = pool->units[i].type;
        uint8_t auto_explore = pool->units[i].acted & UNIT_AUTO_EXPLORE_FLAG;
        pool->units[i].moves_left = (uint8_t)Unit_TypeMovePoints(type);
        pool->units[i].acted = auto_explore;
    }
}

void Unit_GrantVision(const UnitPool *pool, Map *map) {
    if (!pool || !map) return;
    for (int i = 0; i < pool->count; i++) {
        const Unit *unit = &pool->units[i];
        Map_MarkVisibleArea(map, unit->x, unit->y, Unit_TypeSight(unit->type));
    }
}

void Unit_GrantVisionOwner(const UnitPool *pool, Map *map, int owner) {
    if (!pool || !map) return;
    for (int i = 0; i < pool->count; i++) {
        const Unit *unit = &pool->units[i];
        if (unit->owner != owner) continue;
        Map_MarkVisibleArea(map, unit->x, unit->y, Unit_TypeSight(unit->type));
    }
}
