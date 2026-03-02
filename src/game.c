#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include "game.h"
#include "maps.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

player_state_t local_players[MAX_LOCAL_PLAYERS];
int num_local_players = 1;
int playerFov = 70;

deer_t    deer_enemies[NUM_DEER] = {{12.0f, 4.0f, true, false, 0}};
bullet_t  bullets[MAX_BULLETS];
powerup_t powerups[MAX_POWERUPS];

static int powerup_spawn_timer = 0;
static void spawn_one_powerup(void);  /* forward declaration */

/* Open floor positions where powerups may spawn — verified against the map. */
#define NUM_PU_SPOTS 8
static const float pu_x[NUM_PU_SPOTS] = { 3.5f, 10.5f, 15.5f,  3.5f, 15.5f,  3.5f, 10.5f, 15.5f };
static const float pu_y[NUM_PU_SPOTS] = { 3.5f,  3.5f,  3.5f, 10.5f, 10.5f, 16.5f, 16.5f, 16.5f };

static const float spawn_x[MAX_LOCAL_PLAYERS] = {7.5f, 7.5f, 9.5f, 9.5f};
static const float spawn_y[MAX_LOCAL_PLAYERS] = {6.5f, 8.5f, 6.5f, 8.5f};

/* Pre-verified open floor tiles spread around the map — used for random
 * respawns so killed players don't always return to the same spot. */
#define NUM_RESPAWN_SPOTS 12
static const float rsp_x[NUM_RESPAWN_SPOTS] = {
    2.5f,  5.5f, 10.5f, 17.5f,   /* top band  */
    2.5f,  5.5f, 14.5f, 17.5f,   /* mid band  */
    2.5f,  5.5f, 14.5f, 17.5f,   /* bot band  */
};
static const float rsp_y[NUM_RESPAWN_SPOTS] = {
    2.5f,  2.5f,  2.5f,  2.5f,
    8.5f,  8.5f,  8.5f,  8.5f,
   17.5f, 17.5f, 17.5f, 17.5f,
};

static int respawn_rng = 0;   /* rolling counter for pseudo-random index */

void init_players(int count) {
    set_map(selected_map);
    if (count < 1) count = 1;
    if (count > MAX_LOCAL_PLAYERS) count = MAX_LOCAL_PLAYERS;
    num_local_players = count;
    for (int i = 0; i < count; i++) {
        local_players[i].x             = spawn_x[i];
        local_players[i].y             = spawn_y[i];
        local_players[i].angle         = 0.0f;
        local_players[i].health        = 100;
        local_players[i].is_firing     = false;
        local_players[i].is_moving     = false;
        local_players[i].fire_cooldown = 0;
        local_players[i].kills         = 0;
        local_players[i].deaths        = 0;
        local_players[i].just_fired    = false;
        local_players[i].just_died     = false;
        local_players[i].jump_z        = 0.0f;
        local_players[i].jump_vel      = 0.0f;
        local_players[i].is_sprinting  = false;
        local_players[i].powerup_kind  = -1;
        local_players[i].powerup_timer = 0;
        local_players[i].is_dead       = false;
        local_players[i].respawn_x     = spawn_x[i];
        local_players[i].respawn_y     = spawn_y[i];
    }
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = false;
        bullets[i].big    = false;
    }
    for (int i = 0; i < MAX_POWERUPS; i++)
        powerups[i].active = false;
    powerup_spawn_timer = 0;
    /* Pre-seed two powerups so there is always something to pick up. */
    spawn_one_powerup();
    spawn_one_powerup();

    /* Seed the C random number generator from the hardware cycle counter so
     * respawn positions differ every play session. */
    srand((unsigned int)TICKS_READ());
    respawn_rng = 0;
}

static void move_at_angle(int idx, float angle, float dist) {
    float nx = local_players[idx].x + dist * cosf(angle);
    float ny = local_players[idx].y + dist * sinf(angle);
    int mx = (int)nx,  my = (int)ny;
    int cx = (int)local_players[idx].x;
    int cy = (int)local_players[idx].y;

    if (mx >= 0 && mx < 20 && my >= 0 && my < 20 && current_map[my][mx] == 0) {
        local_players[idx].x = nx;
        local_players[idx].y = ny;
        return;
    }
    if (mx >= 0 && mx < 20 && cy >= 0 && cy < 20 && current_map[cy][mx] == 0)
        local_players[idx].x = nx;
    if (cx >= 0 && cx < 20 && my >= 0 && my < 20 && current_map[my][cx] == 0)
        local_players[idx].y = ny;
}

void moveForward(int idx)  { move_at_angle(idx, local_players[idx].angle, 0.05f); }
void moveBackward(int idx) { move_at_angle(idx, local_players[idx].angle + M_PI, 0.05f); }
void moveLeft(int idx)     { move_at_angle(idx, local_players[idx].angle - (M_PI / 2.0f), 0.05f); }
void moveRight(int idx)    { move_at_angle(idx, local_players[idx].angle + (M_PI / 2.0f), 0.05f); }

void rotateLeft(int idx) {
    local_players[idx].angle -= 0.025f;
    if (local_players[idx].angle < 0.0f)
        local_players[idx].angle += 2.0f * M_PI;
}

void rotateRight(int idx) {
    local_players[idx].angle += 0.025f;
    if (local_players[idx].angle >= 2.0f * M_PI)
        local_players[idx].angle -= 2.0f * M_PI;
}

/* Spawn one bullet travelling at the given absolute angle. */
static void fire_one_bullet(int player_idx, float angle, bool big) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) continue;
        bullets[i].x      = local_players[player_idx].x;
        bullets[i].y      = local_players[player_idx].y;
        bullets[i].dx     = cosf(angle);
        bullets[i].dy     = sinf(angle);
        bullets[i].owner  = player_idx;
        bullets[i].active = true;
        bullets[i].big    = big;
        return;
    }
}

void fire_bullet(int player_idx) {
    float base  = local_players[player_idx].angle;
    int   kind  = local_players[player_idx].powerup_kind;
    bool  big   = (kind == POWERUP_BIG_BULLET);

    if (kind == POWERUP_SPREAD_SHOT) {
        /* Three bullets: centre ±0.26 rad (~15°) */
        fire_one_bullet(player_idx, base - 0.26f, false);
        fire_one_bullet(player_idx, base,          false);
        fire_one_bullet(player_idx, base + 0.26f, false);
    } else {
        fire_one_bullet(player_idx, base, big);
    }
}

void update_bullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;

        bullets[i].x += bullets[i].dx * BULLET_SPEED;
        bullets[i].y += bullets[i].dy * BULLET_SPEED;

        /* Wall collision */
        int mx = (int)bullets[i].x;
        int my = (int)bullets[i].y;
        if (mx < 0 || mx >= 20 || my < 0 || my >= 20 || current_map[my][mx] != 0) {
            bullets[i].active = false;
            continue;
        }

        /* Player hit detection — skip the bullet's owner and dead players */
        float hit_r = bullets[i].big ? BIG_BULLET_RADIUS : BULLET_HIT_RADIUS;
        for (int p = 0; p < num_local_players; p++) {
            if (p == bullets[i].owner) continue;
            if (local_players[p].is_dead) continue;
            float dx = local_players[p].x - bullets[i].x;
            float dy = local_players[p].y - bullets[i].y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < hit_r) {
                /* Bullet travels at floor level — a well-timed jump dodges it */
                if (local_players[p].jump_z >= JUMP_DODGE_MIN_Z) continue;
                int owner = bullets[i].owner;
                local_players[owner].kills++;
                local_players[p].deaths++;

                /* Pre-pick a respawn spot: far from killer, not inside a wall */
                int spot = respawn_rng % NUM_RESPAWN_SPOTS;
                respawn_rng++;
                for (int attempt = 0; attempt < NUM_RESPAWN_SPOTS; attempt++) {
                    float kx = local_players[owner].x - rsp_x[spot];
                    float ky = local_players[owner].y - rsp_y[spot];
                    bool in_wall = (current_map[(int)rsp_y[spot]][(int)rsp_x[spot]] != 0);
                    if (!in_wall && kx*kx + ky*ky > 4.0f) break;
                    spot = (spot + 1) % NUM_RESPAWN_SPOTS;
                }
                /* Fallback: any open spot */
                if (current_map[(int)rsp_y[spot]][(int)rsp_x[spot]] != 0) {
                    for (int attempt = 0; attempt < NUM_RESPAWN_SPOTS; attempt++) {
                        if (current_map[(int)rsp_y[spot]][(int)rsp_x[spot]] == 0) break;
                        spot = (spot + 1) % NUM_RESPAWN_SPOTS;
                    }
                }

                local_players[p].respawn_x     = rsp_x[spot];
                local_players[p].respawn_y     = rsp_y[spot];
                local_players[p].is_dead       = true;
                local_players[p].just_died     = true;
                local_players[p].powerup_kind  = -1;
                local_players[p].powerup_timer = 0;
                bullets[i].active = false;
                break;
            }
        }

        /* AI deer hit detection (1P mode only, handled here for completeness) */
        if (!bullets[i].active) continue;
        if (num_local_players == 1) {
            for (int d = 0; d < NUM_DEER; d++) {
                if (!deer_enemies[d].active || deer_enemies[d].is_dead) continue;
                float dx = deer_enemies[d].x - bullets[i].x;
                float dy = deer_enemies[d].y - bullets[i].y;
                if (sqrtf(dx*dx + dy*dy) < hit_r) {
                    deer_enemies[d].is_dead    = true;
                    deer_enemies[d].death_timer = 120; /* 2 s at 60 fps */
                    bullets[i].active           = false;
                    local_players[bullets[i].owner].kills++;
                    break;
                }
            }
        }
    }
}

void update_player(int idx, joypad_buttons_t btn, joypad_inputs_t inp) {
    const float DEADZONE       = 0.08f;
    const float MAX_MOVE_SPEED = 0.08f;   /* units/frame at full stick deflection */
    const float MAX_TURN_SPEED = 0.045f;  /* rad/frame at full stick deflection   */
    const float STEP_SIZE      = 0.05f;   /* digital button movement step         */

    /* B held = sprint */
    bool sprinting = inp.btn.b;
    float spd = sprinting ? SPRINT_FACTOR : 1.0f;
    local_players[idx].is_sprinting = sprinting;

    /* Powerup speed modifiers */
    if (local_players[idx].powerup_kind == POWERUP_SPEED)
        spd *= 2.0f;
    else if (local_players[idx].powerup_kind == POWERUP_SLOW)
        spd *= 0.4f;

    /* Effective fire cooldown (FAST_FIRE cuts it to 1/4) */
    int cooldown = (local_players[idx].powerup_kind == POWERUP_FAST_FIRE)
        ? (FIRE_COOLDOWN / 4)
        : FIRE_COOLDOWN;

    bool moving = false;

    /* Normalize stick axes to [-1, 1] */
    float sy = inp.stick_y / 127.0f;
    float sx = inp.stick_x / 127.0f;

    /* Stick Y — forward / backward */
    if (sy > DEADZONE) {
        move_at_angle(idx, local_players[idx].angle, sy * MAX_MOVE_SPEED * spd);
        moving = true;
    } else if (sy < -DEADZONE) {
        move_at_angle(idx, local_players[idx].angle + M_PI, -sy * MAX_MOVE_SPEED * spd);
        moving = true;
    }
    /* D-pad up/down — digital forward / backward */
    if (inp.btn.d_up)   { move_at_angle(idx, local_players[idx].angle,        STEP_SIZE * spd); moving = true; }
    if (inp.btn.d_down) { move_at_angle(idx, local_players[idx].angle + M_PI, STEP_SIZE * spd); moving = true; }

    /* Stick X — turn left / right */
    if (fabsf(sx) > DEADZONE) {
        local_players[idx].angle += sx * MAX_TURN_SPEED;
        if (local_players[idx].angle < 0.0f)         local_players[idx].angle += 2.0f * M_PI;
        if (local_players[idx].angle >= 2.0f * M_PI) local_players[idx].angle -= 2.0f * M_PI;
    }

    /* C-Left / C-Right — strafe */
    if (inp.btn.c_left)  { move_at_angle(idx, local_players[idx].angle - (M_PI / 2.0f), STEP_SIZE * spd); moving = true; }
    if (inp.btn.c_right) { move_at_angle(idx, local_players[idx].angle + (M_PI / 2.0f), STEP_SIZE * spd); moving = true; }

    /* D-pad left/right — turn */
    if (inp.btn.d_left)  rotateLeft(idx);
    if (inp.btn.d_right) rotateRight(idx);

    /* L / R shoulders — strafe */
    if (inp.btn.l) { move_at_angle(idx, local_players[idx].angle - (M_PI / 2.0f), STEP_SIZE * spd); moving = true; }
    if (inp.btn.r) { move_at_angle(idx, local_players[idx].angle + (M_PI / 2.0f), STEP_SIZE * spd); moving = true; }

    local_players[idx].is_moving = moving;

    /* A pressed = jump (only when on ground) */
    if (btn.a && local_players[idx].jump_z == 0.0f) {
        local_players[idx].jump_vel = JUMP_VELOCITY;
    }
    local_players[idx].jump_z   += local_players[idx].jump_vel;
    local_players[idx].jump_vel -= GRAVITY;
    if (local_players[idx].jump_z <= 0.0f) {
        local_players[idx].jump_z   = 0.0f;
        local_players[idx].jump_vel = 0.0f;
    }

    /* Decrement bolt-action cooldown */
    if (local_players[idx].fire_cooldown > 0)
        local_players[idx].fire_cooldown--;

    /* Z trigger fires — edge-triggered so one press = one shot */
    local_players[idx].just_fired = false;
    if (btn.z && local_players[idx].fire_cooldown == 0) {
        fire_bullet(idx);
        local_players[idx].fire_cooldown = cooldown;
        local_players[idx].just_fired    = true;
    }

    /* Show firing sprite for the first ~15 frames after the shot */
    local_players[idx].is_firing =
        (local_players[idx].fire_cooldown > cooldown - 15);
}

void respawn_player(int p) {
    local_players[p].x             = local_players[p].respawn_x;
    local_players[p].y             = local_players[p].respawn_y;
    local_players[p].health        = 100;
    local_players[p].fire_cooldown = 0;
    local_players[p].jump_z        = 0.0f;
    local_players[p].jump_vel      = 0.0f;
    local_players[p].is_moving     = false;
    local_players[p].is_dead       = false;
}

static void spawn_one_powerup(void) {
    int slot = -1;
    for (int i = 0; i < MAX_POWERUPS; i++)
        if (!powerups[i].active) { slot = i; break; }
    if (slot == -1) return;   /* all slots occupied */

    int kind = rand() % NUM_POWERUP_KINDS;
    int pos  = rand() % NUM_PU_SPOTS;

    /* Try not to stack two powerups on the same tile, and skip wall positions. */
    for (int attempt = 0; attempt < NUM_PU_SPOTS; attempt++) {
        bool in_wall = (current_map[(int)pu_y[pos]][(int)pu_x[pos]] != 0);
        if (!in_wall) {
            bool clear = true;
            for (int i = 0; i < MAX_POWERUPS; i++) {
                if (!powerups[i].active) continue;
                float ddx = powerups[i].x - pu_x[pos];
                float ddy = powerups[i].y - pu_y[pos];
                if (ddx*ddx + ddy*ddy < 1.0f) { clear = false; break; }
            }
            if (clear) break;
        }
        pos = (pos + 1) % NUM_PU_SPOTS;
    }
    if (current_map[(int)pu_y[pos]][(int)pu_x[pos]] != 0) return; /* no open spot */

    powerups[slot].x      = pu_x[pos];
    powerups[slot].y      = pu_y[pos];
    powerups[slot].kind   = kind;
    powerups[slot].active = true;
}

void update_powerups(void) {
    /* Timed auto-spawn */
    if (++powerup_spawn_timer >= POWERUP_SPAWN_RATE) {
        powerup_spawn_timer = 0;
        spawn_one_powerup();
    }

    /* Check player pickup (contact radius = 0.5 tiles) */
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!powerups[i].active) continue;
        for (int p = 0; p < num_local_players; p++) {
            float dx = local_players[p].x - powerups[i].x;
            float dy = local_players[p].y - powerups[i].y;
            if (dx*dx + dy*dy < 0.85f * 0.85f) {
                local_players[p].powerup_kind  = powerups[i].kind;
                local_players[p].powerup_timer = POWERUP_DURATION;
                powerups[i].active = false;
                break;
            }
        }
    }

    /* Count-down active powerup timers */
    for (int p = 0; p < num_local_players; p++) {
        if (local_players[p].powerup_timer > 0) {
            if (--local_players[p].powerup_timer == 0)
                local_players[p].powerup_kind = -1;
        }
    }
}

void update_deer(void) {
    /* Count down death timers regardless of player count */
    for (int i = 0; i < NUM_DEER; i++) {
        if (!deer_enemies[i].active || !deer_enemies[i].is_dead) continue;
        if (--deer_enemies[i].death_timer <= 0)
            deer_enemies[i].active = false;
    }

    /* AI movement only active in 1-player mode */
    if (num_local_players > 1) return;

    for (int i = 0; i < NUM_DEER; i++) {
        if (!deer_enemies[i].active || deer_enemies[i].is_dead) continue;

        /* Chase the nearest player */
        float minDist = 1e10f;
        int   nearest = 0;
        for (int p = 0; p < num_local_players; p++) {
            float dx = local_players[p].x - deer_enemies[i].x;
            float dy = local_players[p].y - deer_enemies[i].y;
            float d  = sqrtf(dx*dx + dy*dy);
            if (d < minDist) { minDist = d; nearest = p; }
        }

        float dx = local_players[nearest].x - deer_enemies[i].x;
        float dy = local_players[nearest].y - deer_enemies[i].y;
        float len = sqrtf(dx*dx + dy*dy);
        if (len < 0.1f) continue;
        dx /= len; dy /= len;

        float newX, newY;
        if (minDist > 2.0f) {
            newX = deer_enemies[i].x + dx * 0.01f;
            newY = deer_enemies[i].y + dy * 0.01f;
        } else {
            newX = deer_enemies[i].x + dy * 0.005f;
            newY = deer_enemies[i].y - dx * 0.005f;
        }

        int mx = (int)newX, my = (int)newY;
        if (mx >= 0 && mx < 20 && my >= 0 && my < 20 && current_map[my][mx] == 0) {
            deer_enemies[i].x = newX;
            deer_enemies[i].y = newY;
        }
    }
}
