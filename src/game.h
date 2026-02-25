#ifndef GAME_H
#define GAME_H

#include <libdragon.h>
#include <stdbool.h>

#define DEPTH_BUF_W       320
#define MAX_LOCAL_PLAYERS 4
#define NUM_DEER          1
#define MAX_BULLETS       16
#define FIRE_COOLDOWN     75   /* frames between shots (~2.5 s at 30 fps) */
#define BULLET_SPEED      0.40f
#define BULLET_HIT_RADIUS 0.55f
#define SPRINT_FACTOR     1.8f
#define JUMP_VELOCITY     0.020f
#define GRAVITY           0.0015f

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
} player_state_t;

typedef struct {
    float x, y;
    float dx, dy;
    int   owner;
    bool  active;
} bullet_t;

typedef struct {
    float x, y;
    bool  active;
} deer_t;

extern player_state_t local_players[MAX_LOCAL_PLAYERS];
extern int            num_local_players;
extern const int      map[20][20];
extern int            playerFov;
extern deer_t         deer_enemies[NUM_DEER];
extern bullet_t       bullets[MAX_BULLETS];

void init_players(int count);
void update_player(int idx, joypad_buttons_t btn, joypad_inputs_t inp);
void update_deer(void);
void update_bullets(void);
void fire_bullet(int player_idx);

void moveForward(int idx);
void moveBackward(int idx);
void moveLeft(int idx);
void moveRight(int idx);
void rotateLeft(int idx);
void rotateRight(int idx);

#endif /* GAME_H */
