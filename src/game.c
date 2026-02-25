#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include "game.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

const int map[20][20] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

player_state_t local_players[MAX_LOCAL_PLAYERS];
int num_local_players = 1;
int playerFov = 70;

deer_t deer_enemies[NUM_DEER] = {{12.0f, 4.0f, true}};
bullet_t bullets[MAX_BULLETS];

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
    }
    for (int i = 0; i < MAX_BULLETS; i++)
        bullets[i].active = false;

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

    if (mx >= 0 && mx < 20 && my >= 0 && my < 20 && map[my][mx] == 0) {
        local_players[idx].x = nx;
        local_players[idx].y = ny;
        return;
    }
    if (mx >= 0 && mx < 20 && cy >= 0 && cy < 20 && map[cy][mx] == 0)
        local_players[idx].x = nx;
    if (cx >= 0 && cx < 20 && my >= 0 && my < 20 && map[my][cx] == 0)
        local_players[idx].y = ny;
}

void moveForward(int idx)  { move_at_angle(idx, local_players[idx].angle, 0.05f); }
void moveBackward(int idx) { move_at_angle(idx, local_players[idx].angle + M_PI, 0.05f); }
void moveLeft(int idx)     { move_at_angle(idx, local_players[idx].angle - (M_PI / 2.0f), 0.05f); }
void moveRight(int idx)    { move_at_angle(idx, local_players[idx].angle + (M_PI / 2.0f), 0.05f); }

void rotateLeft(int idx) {
    local_players[idx].angle -= 0.05f;
    if (local_players[idx].angle < 0.0f)
        local_players[idx].angle += 2.0f * M_PI;
}

void rotateRight(int idx) {
    local_players[idx].angle += 0.05f;
    if (local_players[idx].angle >= 2.0f * M_PI)
        local_players[idx].angle -= 2.0f * M_PI;
}

void fire_bullet(int player_idx) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) continue;
        bullets[i].x      = local_players[player_idx].x;
        bullets[i].y      = local_players[player_idx].y;
        bullets[i].dx     = cosf(local_players[player_idx].angle);
        bullets[i].dy     = sinf(local_players[player_idx].angle);
        bullets[i].owner  = player_idx;
        bullets[i].active = true;
        return;
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
        if (mx < 0 || mx >= 20 || my < 0 || my >= 20 || map[my][mx] != 0) {
            bullets[i].active = false;
            continue;
        }

        /* Player hit detection — skip the bullet's owner */
        for (int p = 0; p < num_local_players; p++) {
            if (p == bullets[i].owner) continue;
            float dx = local_players[p].x - bullets[i].x;
            float dy = local_players[p].y - bullets[i].y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < BULLET_HIT_RADIUS) {
                int owner = bullets[i].owner;
                local_players[owner].kills++;
                local_players[p].deaths++;

                /* Pick a random respawn spot that isn't where the killer is */
                int spot = respawn_rng % NUM_RESPAWN_SPOTS;
                respawn_rng++;
                for (int attempt = 0; attempt < NUM_RESPAWN_SPOTS; attempt++) {
                    float kx = local_players[owner].x - rsp_x[spot];
                    float ky = local_players[owner].y - rsp_y[spot];
                    if (kx*kx + ky*ky > 4.0f) break; /* > 2 tiles from killer */
                    spot = (spot + 1) % NUM_RESPAWN_SPOTS;
                }

                local_players[p].x             = rsp_x[spot];
                local_players[p].y             = rsp_y[spot];
                local_players[p].health        = 100;
                local_players[p].fire_cooldown = 0;
                local_players[p].just_died     = true;
                bullets[i].active = false;
                break;
            }
        }

        /* AI deer hit detection (1P mode only, handled here for completeness) */
        if (!bullets[i].active) continue;
        if (num_local_players == 1) {
            for (int d = 0; d < NUM_DEER; d++) {
                if (!deer_enemies[d].active) continue;
                float dx = deer_enemies[d].x - bullets[i].x;
                float dy = deer_enemies[d].y - bullets[i].y;
                if (sqrtf(dx*dx + dy*dy) < BULLET_HIT_RADIUS) {
                    deer_enemies[d].active = false;
                    bullets[i].active      = false;
                    local_players[bullets[i].owner].kills++;
                    break;
                }
            }
        }
    }
}

void update_player(int idx, joypad_buttons_t btn, joypad_inputs_t inp) {
    const float THRESHOLD  = 10.0f;
    const float MOVE_SCALE = 0.003f;

    bool moving = false;

    if (inp.stick_y > THRESHOLD) {
        move_at_angle(idx, local_players[idx].angle, inp.stick_y * MOVE_SCALE);
        moving = true;
    } else if (inp.stick_y < -THRESHOLD) {
        move_at_angle(idx, local_players[idx].angle + M_PI, (-inp.stick_y) * MOVE_SCALE);
        moving = true;
    }

    /* Stick X — strafe left/right */
    if (inp.stick_x > THRESHOLD) {
        move_at_angle(idx, local_players[idx].angle + (M_PI / 2.0f),
                      inp.stick_x * MOVE_SCALE);
        moving = true;
    } else if (inp.stick_x < -THRESHOLD) {
        move_at_angle(idx, local_players[idx].angle - (M_PI / 2.0f),
                      (-inp.stick_x) * MOVE_SCALE);
        moving = true;
    }

    /* C-Left / C-Right — turn */
    if (inp.btn.c_left)  rotateLeft(idx);
    if (inp.btn.c_right) rotateRight(idx);

    if (inp.btn.d_up)    { moveForward(idx);  moving = true; }
    if (inp.btn.d_down)  { moveBackward(idx); moving = true; }
    if (inp.btn.d_left)  { moveLeft(idx);   moving = true; }
    if (inp.btn.d_right) { moveRight(idx);  moving = true; }
    if (inp.btn.l)       { moveLeft(idx);  moving = true; }
    if (inp.btn.r)       { moveRight(idx); moving = true; }

    local_players[idx].is_moving = moving;

    /* Decrement bolt-action cooldown */
    if (local_players[idx].fire_cooldown > 0)
        local_players[idx].fire_cooldown--;

    /* Z trigger fires — edge-triggered so one press = one shot */
    local_players[idx].just_fired = false;
    if (btn.z && local_players[idx].fire_cooldown == 0) {
        fire_bullet(idx);
        local_players[idx].fire_cooldown = FIRE_COOLDOWN;
        local_players[idx].just_fired    = true;
    }

    /* Show firing sprite for the first ~15 frames after the shot */
    local_players[idx].is_firing =
        (local_players[idx].fire_cooldown > FIRE_COOLDOWN - 15);
}

void update_deer(void) {
    /* AI deer only active in 1-player mode */
    if (num_local_players > 1) return;

    for (int i = 0; i < NUM_DEER; i++) {
        if (!deer_enemies[i].active) continue;

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
        if (mx >= 0 && mx < 20 && my >= 0 && my < 20 && map[my][mx] == 0) {
            deer_enemies[i].x = newX;
            deer_enemies[i].y = newY;
        }
    }
}
