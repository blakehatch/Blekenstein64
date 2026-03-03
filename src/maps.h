#ifndef MAPS_H
#define MAPS_H

#define MAP_SIZE 20
#define NUM_MAPS 5

/* All four playable maps (20×20 tile grids, 0=floor 1=wall).
 * Spawn positions at map[6][7], map[8][7], map[6][9], map[8][9]
 * are guaranteed open in every map. */
extern const int map[MAP_SIZE][MAP_SIZE];           /* Map 0 – Classic   */
extern const int map_corridor[MAP_SIZE][MAP_SIZE];  /* Map 1 – Corridors */
extern const int map_maze[MAP_SIZE][MAP_SIZE];      /* Map 2 – Maze      */
extern const int map_arena[MAP_SIZE][MAP_SIZE];     /* Map 3 – Arena     */
extern const int map_cross[MAP_SIZE][MAP_SIZE];     /* Map 4 – Cross     */

/* Pointer to whichever map is currently active.  Updated by set_map(). */
extern const int (*current_map)[MAP_SIZE];

/* Index of the selected map (0–NUM_MAPS-1).  Written by the level-select
 * screen; read by set_map() / init_players(). */
extern int selected_map;

/* Switch current_map to the map at index idx (clamped to valid range). */
void set_map(int idx);

#endif /* MAPS_H */
