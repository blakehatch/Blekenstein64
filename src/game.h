#ifndef GAME_H
#define GAME_H

#include <libdragon.h>
#include <stdbool.h>
#include "maps.h"

#define DEPTH_BUF_W       320
#define MAX_LOCAL_PLAYERS 4
#define NUM_DEER          1
#define MAX_BULLETS       16
#define FIRE_COOLDOWN     75   /* frames between shots (~2.5 s at 30 fps) */
#define BULLET_SPEED      0.40f
#define BULLET_HIT_RADIUS 0.55f
#define BIG_BULLET_RADIUS 1.0f  /* hit radius with BIG_BULLET powerup */
#define SPRINT_FACTOR     1.3f
#define JUMP_VELOCITY     0.020f
#define GRAVITY           0.0015f
#define JUMP_DODGE_MIN_Z  0.08f   /* jump_z threshold to dodge a bullet */

#define MAX_POWERUPS         4
#define POWERUP_DURATION     360   /* ~12 s at 30 fps */
#define POWERUP_SPAWN_RATE   240   /* frames between auto-spawns */

typedef enum {
    POWERUP_FAST_FIRE   = 0,  /* reduced fire cooldown */
    POWERUP_BIG_BULLET  = 1,  /* larger bullet hit radius */
    POWERUP_SPEED       = 2,  /* move faster */
    POWERUP_SLOW        = 3,  /* move slower (lightning bolt icon) */
    POWERUP_SPREAD_SHOT = 4,  /* fires 3 bullets in a cone */
    NUM_POWERUP_KINDS   = 5
} powerup_kind_t;

typedef struct {
    float x, y;
    int   kind;   /* powerup_kind_t */
    bool  active;
} powerup_t;

typedef struct {
    float x, y, angle;
    int   health;
    bool  is_firing;
    bool  is_moving;
    int   fire_cooldown;  /* frames remaining in bolt-action delay (0 = ready) */
    int   kills;
    int   deaths;
    bool  just_fired;    /* true for one frame when a shot is fired (SFX trigger) */
    bool  just_died;     /* true for one frame when this player is killed (SFX trigger) */
    float jump_z;        /* current jump height (0 = on ground) */
    float jump_vel;      /* vertical velocity for jump arc */
    bool  is_sprinting;
    int   powerup_kind;  /* -1 = none, or powerup_kind_t */
    int   powerup_timer; /* frames remaining on active powerup */
    bool  is_dead;       /* true while death screen is showing (waiting for A) */
    float respawn_x;     /* pre-picked respawn position, applied when A pressed */
    float respawn_y;
} player_state_t;

typedef struct {
    float x, y;
    float dx, dy;
    int   owner;
    bool  active;
    bool  big;    /* fired with POWERUP_BIG_BULLET — uses BIG_BULLET_RADIUS */
} bullet_t;

typedef struct {
    float x, y;
    bool  active;
    bool  is_dead;      /* true while showing death sprite before despawn */
    int   death_timer;  /* frames remaining before fully removing (is_dead only) */
} deer_t;

extern player_state_t   local_players[MAX_LOCAL_PLAYERS];
extern int              num_local_players;
extern int              playerFov;
extern deer_t         deer_enemies[NUM_DEER];
extern bullet_t       bullets[MAX_BULLETS];
extern powerup_t      powerups[MAX_POWERUPS];

void set_map(int idx);
void respawn_player(int p);
void init_players(int count);
void update_player(int idx, joypad_buttons_t btn, joypad_inputs_t inp);
void update_deer(void);
void update_bullets(void);
void fire_bullet(int player_idx);
void update_powerups(void);

void moveForward(int idx);
void moveBackward(int idx);
void moveLeft(int idx);
void moveRight(int idx);
void rotateLeft(int idx);
void rotateRight(int idx);

#endif /* GAME_H */
