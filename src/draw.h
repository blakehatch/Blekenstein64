#ifndef DRAW_H
#define DRAW_H

#include <libdragon.h>
#include "game.h"

/* Pre-transposed sprite columns for race-free per-column RDP uploads.
 * Each column is stored at a separate 8-byte-aligned address so the RDP
 * DMA always reads the correct data regardless of CPU/RSP async timing. */
typedef struct {
    uint16_t *cols;       /* column-major pixel buffer (8-byte-aligned per column) */
    int       texW;
    int       texH;
    int       col_stride; /* bytes per column, always a multiple of 8 */
} billboard_t;

/* Create a billboard from a sprite (call once at load time, after
 * fix_sprite_colorkey so the transposed copy has correct alpha bits). */
billboard_t *billboard_create(sprite_t *spr);

/* Free a billboard created with billboard_create. */
void billboard_free(billboard_t *bb);

/* Render the 3D view for one player into a viewport sub-region.
 * depthBuf must have room for at least vpW floats. */
void castRays(surface_t *surf, float *depthBuf, sprite_t *wallTex,
              int vpX, int vpY, int vpW, int vpH, int playerIdx);

/* Billboard-render a deer/player sprite, depth-tested against depthBuf.
 * tint is multiplied with texture colour; use RGBA32(255,255,255,255) for
 * no tint (AI deer).  Player tints differentiate split-screen players.
 * v_offset: extra downward shift as a fraction of billboard height
 *   (0 = centred at horizon, 0.5 = top edge at horizon / sits on ground).
 * sprite_zbuf: optional per-column depth array updated with this sprite's
 *   depth; used by drawPowerups for sprite-vs-powerup occlusion. */
void drawDeerBillboard(surface_t *surf, const float *depthBuf,
                       const billboard_t *bb, color_t tint,
                       float deerX, float deerY, float targetJumpZ,
                       float v_offset, float *sprite_zbuf,
                       int vpX, int vpY, int vpW, int vpH, int playerIdx);

/* Draw the gun HUD sprite at the bottom of a viewport. */
void drawGunHUD(surface_t *surf, sprite_t *gunIdle, sprite_t *gunFire,
                bool firing, bool moving, uint32_t tick,
                int vpX, int vpY, int vpW, int vpH);

/* Simplified top-right minimap (1-player mode only). */
void drawMiniMap(surface_t *surf, const int mapData[20][20], float xf, float yf,
                 float angle, int tileSize, int mapW, int mapH,
                 int screenW, int screenH);

/* Draw active projectiles as depth-tested fireballs for one viewport. */
void drawBullets(surface_t *surf, const float *depthBuf,
                 int vpX, int vpY, int vpW, int vpH, int playerIdx);

/* Draw separator lines between split-screen viewports. */
void drawViewportBorders(surface_t *surf, int numPlayers);

/* Draw active world-space powerup pickups, depth-tested against depthBuf and
 * sprite_zbuf (nearest billboard depth per column; may be NULL).
 * tick drives the bobbing animation. */
void drawPowerups(surface_t *surf, const float *depthBuf,
                  const float *sprite_zbuf,
                  int vpX, int vpY, int vpW, int vpH, int playerIdx,
                  uint32_t tick);

/* Fix magenta/pink colorkey: clears alpha bit for hot-pink RGBA5551 pixels.
 * Call once after sprite_load() for any sprite with a painted background. */
void fix_sprite_colorkey(sprite_t *spr);

/* Variant for sprites with a white background: clears near-white pixels
 * (all channels >= 28) in addition to the standard magenta key. */
void fix_sprite_colorkey_white(sprite_t *spr);

#endif /* DRAW_H */
