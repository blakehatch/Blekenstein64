/**
 * Blekenstein 64 - Menu, splitscreen game loop
 */
#include <stdio.h>
#include <string.h>
#include <libdragon.h>
#include "game.h"
#include "draw.h"

#define SCREEN_W 320
#define SCREEN_H 240

/* ---- Audio channel allocation ------------------------------------------- */
#define CH_MUSIC   0   /* looping background music (volume 0 — muted) */
#define CH_SHOOT   1   /* gunshot SFX */
#define CH_DEATH   2   /* death SFX */
#define CH_BTN     3   /* menu button-press SFX */
#define NUM_CHANNELS 4

/* ---- Viewport layout ---------------------------------------------------- */
typedef struct { int x, y, w, h; } vp_t;

static vp_t get_vp(int idx, int np) {
    switch (np) {
        default:
        case 1: return (vp_t){0,   0,   320, 240};
        case 2: return (vp_t){idx * 160, 0, 160, 240};
        case 3:
            if (idx == 0) return (vp_t){  0, 0, 160, 120};
            if (idx == 1) return (vp_t){160, 0, 160, 120};
            return                (vp_t){ 80, 120, 160, 120};
        case 4: return (vp_t){(idx & 1) * 160, (idx / 2) * 120, 160, 120};
    }
}

/* ---- Menu --------------------------------------------------------------- */
static const uint8_t SLOT_R[4] = {100, 60, 200, 220};
static const uint8_t SLOT_G[4] = {220, 180,  80, 100};
static const uint8_t SLOT_B[4] = { 60, 220,  60, 220};

static void draw_menu(surface_t *disp, int sel, int connected, int tick) {
    uint32_t bg    = graphics_make_color(  8,  8, 20, 255);
    uint32_t bglo  = graphics_make_color( 12, 18, 40, 255);
    uint32_t tbox  = graphics_make_color( 30, 26, 60, 255);
    uint32_t tbord = graphics_make_color( 90, 60, 200, 255);
    uint32_t gold  = graphics_make_color(255, 200, 40, 255);
    uint32_t white = graphics_make_color(240, 240, 255, 255);
    uint32_t dimgr = graphics_make_color(100, 100, 130, 255);

    graphics_fill_screen(disp, bg);
    graphics_draw_box(disp, 0, 120, SCREEN_W, 120, bglo);
    graphics_draw_box(disp, 20, 8, SCREEN_W - 40, 50, tbox);
    graphics_draw_box(disp, 18, 6, SCREEN_W - 36, 54, tbord);
    graphics_draw_box(disp, 22, 10, 5, 46, gold);
    graphics_draw_box(disp, SCREEN_W - 27, 10, 5, 46, gold);
    graphics_set_color(gold, tbox);
    graphics_draw_text(disp, 52, 17, "*** BLEKENSTEIN 64 ***");
    graphics_set_color(white, tbox);
    graphics_draw_text(disp, 88, 33, " N64 SPLITSCREEN ");
    graphics_set_color(dimgr, 0);
    graphics_draw_text(disp, 84, 72, "SELECT PLAYERS");
    uint32_t sep = graphics_make_color(50, 45, 90, 255);
    graphics_draw_box(disp, 60, 84, 200, 1, sep);

    int row_y0 = 92, row_h = 26, row_x = 54, row_w = 212;
    static const char *labels[4] = {
        " 1 PLAYER  ", " 2 PLAYERS ", " 3 PLAYERS ", " 4 PLAYERS "
    };
    for (int i = 0; i < 4; i++) {
        int ry = row_y0 + i * row_h;
        bool sel_row = (sel == i + 1);
        uint32_t slotC = graphics_make_color(SLOT_R[i], SLOT_G[i], SLOT_B[i], 255);
        if (sel_row) {
            uint32_t hi  = graphics_make_color(40, 35, 80, 255);
            uint32_t brd = (tick & 16)
                ? slotC
                : graphics_make_color(SLOT_R[i]/2, SLOT_G[i]/2, SLOT_B[i]/2, 255);
            graphics_draw_box(disp, row_x-2, ry-2, row_w+4, row_h+2, brd);
            graphics_draw_box(disp, row_x, ry, row_w, row_h-2, hi);
            graphics_draw_box(disp, row_x, ry, 4, row_h-2, slotC);
            graphics_set_color(slotC, hi);
            graphics_draw_text(disp, row_x + 12, ry + 8, labels[i]);
            graphics_set_color(gold, hi);
            graphics_draw_text(disp, row_x + 194, ry + 8, "<");
        } else {
            graphics_draw_box(disp, row_x, ry, 3, row_h-2,
                              graphics_make_color(SLOT_R[i]/2, SLOT_G[i]/2, SLOT_B[i]/2, 255));
            graphics_set_color(dimgr, bg);
            graphics_draw_text(disp, row_x + 12, ry + 8, labels[i]);
        }
        bool avail = (i + 1) <= connected;
        uint32_t dotC = avail ? graphics_make_color(60,220,60,255)
                              : graphics_make_color(40,40,50,255);
        graphics_draw_box(disp, row_x + row_w - 12, ry + 8, 6, 6, dotC);
    }

    graphics_draw_box(disp, 0, SCREEN_H-16, SCREEN_W, 16,
                      graphics_make_color(16, 14, 32, 255));
    graphics_set_color(dimgr, graphics_make_color(16, 14, 32, 255));
    graphics_draw_text(disp, 32, SCREEN_H-12,
                       "UP/DOWN: select    A/START: confirm");
}

static int connected_count(void) {
    int n = 0;
    for (joypad_port_t p = JOYPAD_PORT_1; p < JOYPAD_PORT_COUNT; p++)
        if (joypad_is_connected(p)) n++;
    return n;
}

/* ---- Pause screen ------------------------------------------------------- */
static void draw_pause(surface_t *disp, int sel, int tick) {
    /* Dark vignette over the 3D scene */
    uint32_t overlay = graphics_make_color(0, 0, 0, 255);
    uint32_t panel   = graphics_make_color(20, 16, 45, 255);
    uint32_t border  = graphics_make_color(90, 60, 200, 255);
    uint32_t gold    = graphics_make_color(255, 200, 40, 255);
    uint32_t white   = graphics_make_color(240, 240, 255, 255);
    uint32_t dimgr   = graphics_make_color(100, 100, 130, 255);

    /* Side strips to darken viewport edges */
    graphics_draw_box(disp, 0, 0, 80, SCREEN_H, overlay);
    graphics_draw_box(disp, 240, 0, 80, SCREEN_H, overlay);
    graphics_draw_box(disp, 0, 0, SCREEN_W, 70, overlay);
    graphics_draw_box(disp, 0, 170, SCREEN_W, 70, overlay);

    /* Central panel */
    graphics_draw_box(disp, 88, 74, 144, 96, border);
    graphics_draw_box(disp, 90, 76, 140, 92, panel);

    /* PAUSED title */
    graphics_set_color(gold, panel);
    graphics_draw_text(disp, 122, 84, "- PAUSED -");

    /* Separator */
    graphics_draw_box(disp, 98, 96, 124, 1, border);

    /* Options */
    static const char *opts[2] = {"RESUME", "EXIT TO MENU"};
    int oy[2] = {105, 120};
    for (int i = 0; i < 2; i++) {
        bool active = (sel == i);
        uint32_t col = active
            ? (((tick >> 3) & 1) ? gold : white)
            : dimgr;
        graphics_set_color(col, panel);
        if (active) graphics_draw_text(disp, 102, oy[i], ">");
        graphics_draw_text(disp, 112, oy[i], opts[i]);
    }

    graphics_set_color(dimgr, panel);
    graphics_draw_text(disp, 98, 140, "A/START:confirm  B:resume");
}

/* ---- Main --------------------------------------------------------------- */
int main(void) {
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    joypad_init();
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();

    /* ---- Audio init ---- */
    audio_init(22050, 4);
    mixer_init(NUM_CHANNELS);

    wav64_t snd_music, snd_shoot, snd_death, snd_btn;
    wav64_open(&snd_music, "rom:/sounds/music.wav64");
    wav64_open(&snd_shoot, "rom:/sounds/shoot.wav64");
    wav64_open(&snd_death, "rom:/sounds/death.wav64");
    wav64_open(&snd_btn,   "rom:/sounds/button_press.wav64");

    wav64_set_loop(&snd_music, true);

    /* Start music looping at full volume */
    wav64_play(&snd_music, CH_MUSIC);
    mixer_ch_set_vol(CH_MUSIC, 1.0f, 1.0f);

    /* ---- Load sprites ---- */
    sprite_t *wall_tex = sprite_load("rom:/sprites/Tree_Wall_Texture.sprite");
    sprite_t *deer_tex = sprite_load("rom:/sprites/Deer_Enemy_Sprite.sprite");
    sprite_t *gun_idle = sprite_load("rom:/sprites/Rifle_GUI_Sprite.sprite");
    sprite_t *gun_fire = sprite_load("rom:/sprites/Rifle_GUI_Sprite_Firing.sprite");

    fix_sprite_colorkey(deer_tex);
    fix_sprite_colorkey(gun_idle);
    fix_sprite_colorkey(gun_fire);
    billboard_t *deer_bb = billboard_create(deer_tex);

    static const color_t DEER_TINT = {255, 255, 255, 255};
    static const color_t PLAYER_TINT[4] = {
        {255, 150, 150, 255},
        {150, 150, 255, 255},
        {150, 255, 150, 255},
        {255, 255, 150, 255},
    };

    /* HUD text colours */
    uint32_t hud_fg = graphics_make_color(255, 255,   0, 255);
    uint32_t hud_bg = graphics_make_color(  0,   0,   0, 255);

    /* ======== Outer restart loop ======================================== */
restart_game:;

    /* ---- Menu loop ---- */
    int  num_players = 1;
    bool confirmed   = false;
    int  debounce    = 0;
    int  menu_tick   = 0;

    while (!confirmed) {
        joypad_poll();
        int connected = connected_count();
        if (connected < 1) connected = 1;

        if (debounce > 0) {
            debounce--;
        } else {
            for (joypad_port_t p = JOYPAD_PORT_1; p < JOYPAD_PORT_COUNT; p++) {
                if (!joypad_is_connected(p)) continue;
                joypad_buttons_t btn = joypad_get_buttons_pressed(p);
                if (btn.d_up || btn.c_up) {
                    if (num_players > 1) { num_players--; debounce = 8;
                        wav64_play(&snd_btn, CH_BTN);
                        mixer_ch_set_vol(CH_BTN, 0.7f, 0.7f); }
                    break;
                }
                if (btn.d_down || btn.c_down) {
                    if (num_players < 4) { num_players++; debounce = 8;
                        wav64_play(&snd_btn, CH_BTN);
                        mixer_ch_set_vol(CH_BTN, 0.7f, 0.7f); }
                    break;
                }
                if (btn.a || btn.start) { confirmed = true; break; }
            }
        }

        /* Keep music mixer alive during menu */
        if (audio_can_write()) {
            short *buf = audio_write_begin();
            mixer_poll(buf, audio_get_buffer_length());
            audio_write_end();
        }

        surface_t *disp = display_get();
        draw_menu(disp, num_players, connected, menu_tick++);
        display_show(disp);
    }

    /* ---- Game setup ---- */
    init_players(num_players);
    float    depthBuf[DEPTH_BUF_W];
    uint32_t game_tick = 0;

    /* Pause state */
    bool paused   = false;
    int  pause_sel = 0;    /* 0=Resume, 1=Exit to menu */
    int  pause_debounce = 0;

    /* ---- Game loop ---- */
    while (1) {
        joypad_poll();
        game_tick++;

        /* ---- Global: check START for pause on any controller ---- */
        for (int i = 0; i < num_local_players; i++) {
            joypad_buttons_t pbtn =
                joypad_get_buttons_pressed((joypad_port_t)(JOYPAD_PORT_1 + i));
            if (pbtn.start) {
                if (!paused) {
                    paused     = true;
                    pause_sel  = 0;
                    pause_debounce = 12;
                    wav64_play(&snd_btn, CH_BTN);
                    mixer_ch_set_vol(CH_BTN, 0.7f, 0.7f);
                }
                break;
            }
        }

        if (paused) {
            /* ---- Pause menu input ---- */
            if (pause_debounce > 0) pause_debounce--;

            if (pause_debounce == 0) {
                joypad_buttons_t pb =
                    joypad_get_buttons_pressed(JOYPAD_PORT_1);

                if (pb.d_up && pause_sel > 0) {
                    pause_sel--;
                    wav64_play(&snd_btn, CH_BTN);
                    mixer_ch_set_vol(CH_BTN, 0.7f, 0.7f);
                    pause_debounce = 10;
                }
                if (pb.d_down && pause_sel < 1) {
                    pause_sel++;
                    wav64_play(&snd_btn, CH_BTN);
                    mixer_ch_set_vol(CH_BTN, 0.7f, 0.7f);
                    pause_debounce = 10;
                }
                if (pb.b) {
                    /* B always resumes */
                    paused = false;
                    pause_debounce = 12;
                }
                if (pb.a || pb.start) {
                    if (pause_sel == 0) {
                        paused = false;
                        pause_debounce = 12;
                    } else {
                        /* Exit to menu — restart entire game */
                        goto restart_game;
                    }
                }
            }
        } else {
            /* ---- Normal game update ---- */
            for (int i = 0; i < num_local_players; i++) {
                joypad_buttons_t btn =
                    joypad_get_buttons_pressed((joypad_port_t)(JOYPAD_PORT_1 + i));
                joypad_inputs_t inp =
                    joypad_get_inputs((joypad_port_t)(JOYPAD_PORT_1 + i));
                update_player(i, btn, inp);
            }

            if (num_local_players == 1)
                update_deer();

            update_bullets();

            /* ---- Trigger SFX from game events ---- */
            for (int i = 0; i < num_local_players; i++) {
                if (local_players[i].just_fired) {
                    wav64_play(&snd_shoot, CH_SHOOT);
                    mixer_ch_set_vol(CH_SHOOT, 1.0f, 1.0f);
                    local_players[i].just_fired = false;
                }
                if (local_players[i].just_died) {
                    wav64_play(&snd_death, CH_DEATH);
                    mixer_ch_set_vol(CH_DEATH, 1.0f, 1.0f);
                    local_players[i].just_died = false;
                }
            }
        }

        /* ---- Render ---- */
        surface_t *disp = display_get();
        rdpq_attach(disp, NULL);

        for (int i = 0; i < num_local_players; i++) {
            vp_t vp = get_vp(i, num_local_players);
            castRays(disp, depthBuf, wall_tex, vp.x, vp.y, vp.w, vp.h, i);

            if (num_local_players == 1) {
                for (int d = 0; d < NUM_DEER; d++) {
                    if (!deer_enemies[d].active) continue;
                    drawDeerBillboard(disp, depthBuf, deer_bb, DEER_TINT,
                                      deer_enemies[d].x, deer_enemies[d].y, 0.0f,
                                      vp.x, vp.y, vp.w, vp.h, i);
                }
            }

            for (int j = 0; j < num_local_players; j++) {
                if (j == i) continue;
                drawDeerBillboard(disp, depthBuf, deer_bb, PLAYER_TINT[j],
                                  local_players[j].x, local_players[j].y,
                                  local_players[j].jump_z,
                                  vp.x, vp.y, vp.w, vp.h, i);
            }

            drawBullets(disp, depthBuf, vp.x, vp.y, vp.w, vp.h, i);

            drawGunHUD(disp, gun_idle, gun_fire,
                       local_players[i].is_firing,
                       local_players[i].is_moving,
                       game_tick,
                       vp.x, vp.y, vp.w, vp.h);

            if (num_local_players == 1)
                drawMiniMap(disp, map,
                            local_players[0].x, local_players[0].y,
                            local_players[0].angle,
                            6, 20, 20, SCREEN_W, SCREEN_H);
        }

        drawViewportBorders(disp, num_local_players);

        /* Finish RDP then do CPU text/overlay */
        rdpq_detach_wait();

        /* Kills/Deaths in top-right of each viewport */
        for (int i = 0; i < num_local_players; i++) {
            vp_t vp = get_vp(i, num_local_players);
            char buf[16];
            snprintf(buf, sizeof(buf), "K:%d D:%d",
                     local_players[i].kills, local_players[i].deaths);
            graphics_draw_box(disp, vp.x + vp.w - 56, vp.y, 56, 11, hud_bg);
            graphics_set_color(hud_fg, hud_bg);
            graphics_draw_text(disp, vp.x + vp.w - 54, vp.y + 2, buf);
        }

        /* Pause overlay (CPU-drawn on top) */
        if (paused)
            draw_pause(disp, pause_sel, (int)game_tick);

        display_show(disp);

        /* ---- Audio: feed mixer once per frame ---- */
        if (audio_can_write()) {
            short *buf = audio_write_begin();
            mixer_poll(buf, audio_get_buffer_length());
            audio_write_end();
        }
    }
}
