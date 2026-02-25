#include <math.h>
#include <malloc.h>
#include <libdragon.h>
#include "draw.h"
#include "game.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ------------------------------------------------------------------ */
/* Colorkey fix                                                         */
/* ------------------------------------------------------------------ */

/* Walk every RGBA5551 pixel and clear the alpha bit for any magenta /
 * pink colorkey shade (high R, G notably lower than R).  Then flush
 * the CPU data cache so the RDP DMA sees the updated bytes in RDRAM.  */
void fix_sprite_colorkey(sprite_t *spr) {
    surface_t s = sprite_get_pixels(spr);
    for (int y = 0; y < (int)s.height; y++) {
        uint16_t *row = (uint16_t *)((uint8_t *)s.buffer + (size_t)y * s.stride);
        for (int x = 0; x < (int)s.width; x++) {
            uint16_t px = row[x];
            if (!(px & 1)) continue;   /* already transparent */
            /* RGBA5551: R[15:11] G[10:6] B[5:1] A[0] */
            int r = (px >> 11) & 0x1F;
            int g = (px >> 6)  & 0x1F;
            int b = (px >> 1)  & 0x1F;
            /* Colorkey: very pure magenta/hot-pink only.
             * Require high R, high B, and very low G so that brownish deer
             * fur (high R, moderate G) is never cleared by accident.
             * This matches backgrounds painted with FF00FF-style hot-pink. */
            if (r > 24 && b > 16 && g < 8)
                row[x] = px & ~(uint16_t)1;
        }
    }
    /* Flush to RDRAM so the RDP DMA doesn't read stale cached data. */
    data_cache_hit_writeback(s.buffer, (unsigned long)s.height * s.stride);
}

/* ------------------------------------------------------------------ */
/* billboard_create / billboard_free                                    */
/* ------------------------------------------------------------------ */

/* Transpose a sprite into column-major order so each column lives at
 * its own permanent 8-byte-aligned address.  This eliminates the race
 * condition where a shared column buffer is overwritten by the CPU
 * before the RSP/RDP finishes reading the previous upload command. */
billboard_t *billboard_create(sprite_t *spr) {
    billboard_t *bb = malloc(sizeof(billboard_t));
    surface_t ts = sprite_get_pixels(spr);
    bb->texW = (int)ts.width;
    bb->texH = (int)ts.height;
    if (bb->texH > 256) bb->texH = 256;  /* cap to TMEM-safe height */

    /* Round each column's byte size up to 8 so every column start is
     * 8-byte aligned (RDP DMA requirement). */
    bb->col_stride = ((bb->texH * 2) + 7) & ~7;
    bb->cols = memalign(8, (size_t)bb->texW * (size_t)bb->col_stride);

    /* Copy each sprite column into its dedicated slot. */
    for (int tX = 0; tX < bb->texW; tX++) {
        uint16_t *dst = (uint16_t *)((uint8_t *)bb->cols + (size_t)tX * bb->col_stride);
        const uint8_t *src = (const uint8_t *)ts.buffer + (size_t)tX * 2;
        for (int row = 0; row < bb->texH; row++)
            dst[row] = *(const uint16_t *)(src + (size_t)row * ts.stride);
    }

    /* Flush entire transposed buffer once — columns never change after this. */
    data_cache_hit_writeback(bb->cols, (size_t)bb->texW * (size_t)bb->col_stride);
    return bb;
}

void billboard_free(billboard_t *bb) {
    if (bb) { free(bb->cols); free(bb); }
}

/* ------------------------------------------------------------------ */
/* castRays — flat-colour walls, RDP fill for ceiling/floor            */
/* ------------------------------------------------------------------ */

void castRays(surface_t *surf, float *depthBuf, sprite_t *wallTex,
              int vpX, int vpY, int vpW, int vpH, int playerIdx) {
    (void)surf; (void)wallTex;

    float posX  = local_players[playerIdx].x;
    float posY  = local_players[playerIdx].y;
    float angle = local_players[playerIdx].angle;
    float fovR  = (float)playerFov * (M_PI / 180.0f);

    int vshift = (int)(local_players[playerIdx].jump_z * (float)vpH);
    int horizon = vpH / 2 + vshift;

    rdpq_set_scissor(vpX, vpY, vpX + vpW, vpY + vpH);

    /* ---- Ceiling & floor: two fast fill rectangles ---- */
    rdpq_set_mode_fill(RGBA32(30, 30, 180, 255));
    rdpq_fill_rectangle(vpX, vpY, vpX + vpW, vpY + horizon);
    rdpq_set_mode_fill(RGBA32(100, 55, 15, 255));
    rdpq_fill_rectangle(vpX, vpY + horizon, vpX + vpW, vpY + vpH);

    /* ---- Wall columns: flat colour with distance fog ---- */
    /* Using texture_rectangle in standard+FLAT mode avoids texture uploads
     * entirely and handles 1-pixel-wide columns with no minimum-size limit. */
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_FLAT);

    for (int col = 0; col < vpW; col++) {
        float rayAngle = angle - fovR * 0.5f + fovR * (float)col / (float)vpW;
        float rDX = cosf(rayAngle);
        float rDY = sinf(rayAngle);

        int mX = (int)posX, mY = (int)posY;

        float ddX = (rDX == 0.0f) ? 1e30f : fabsf(1.0f / rDX);
        float ddY = (rDY == 0.0f) ? 1e30f : fabsf(1.0f / rDY);

        int stepX, stepY, side;
        float sdX, sdY;

        if (rDX < 0.0f) { stepX = -1; sdX = (posX - mX) * ddX; }
        else             { stepX =  1; sdX = (mX + 1.0f - posX) * ddX; }
        if (rDY < 0.0f) { stepY = -1; sdY = (posY - mY) * ddY; }
        else             { stepY =  1; sdY = (mY + 1.0f - posY) * ddY; }

        int hit = 0, lim = 40;
        side = 0;
        while (!hit && lim-- > 0) {
            if (sdX < sdY) { sdX += ddX; mX += stepX; side = 0; }
            else           { sdY += ddY; mY += stepY; side = 1; }
            if (mX < 0 || mX >= 20 || mY < 0 || mY >= 20) break;
            if (map[mY][mX] > 0) hit = 1;
        }

        float dist;
        if (side == 0) dist = (mX - posX + (1.0f - stepX) * 0.5f) / rDX;
        else           dist = (mY - posY + (1.0f - stepY) * 0.5f) / rDY;
        if (dist < 0.001f) dist = 0.001f;

        depthBuf[col] = dist;

        int lineH = (int)((float)vpH / dist);
        int drawStart = vpY + horizon - lineH / 2;
        if (drawStart < vpY) drawStart = vpY;
        int drawEnd = vpY + horizon + lineH / 2;
        if (drawEnd >= vpY + vpH) drawEnd = vpY + vpH - 1;
        int wallH = drawEnd - drawStart + 1;
        if (wallH <= 0) continue;

        /* Two shades: side 0 (E/W faces) darker, side 1 (N/S) lighter.
         * Lerp both toward grey fog colour with distance.             */
        float fog = dist / 8.0f;
        if (fog > 1.0f) fog = 1.0f;
        float inv = 1.0f - fog;
        uint8_t br = side == 0 ? 100 : 130;
        uint8_t bg = side == 0 ? 140 : 180;
        uint8_t bb = side == 0 ?  60 :  80;
        uint8_t r = (uint8_t)(br * inv + 80.0f * fog);
        uint8_t g = (uint8_t)(bg * inv + 80.0f * fog);
        uint8_t b = (uint8_t)(bb * inv + 80.0f * fog);

        rdpq_set_prim_color(RGBA32(r, g, b, 255));
        rdpq_texture_rectangle(TILE0,
            vpX + col, drawStart, vpX + col + 1, drawEnd + 1, 0, 0);
    }
}

/* ------------------------------------------------------------------ */
/* drawDeerBillboard — column-by-column depth-tested RDP sprite        */
/* ------------------------------------------------------------------ */

void drawDeerBillboard(surface_t *surf, const float *depthBuf,
                       const billboard_t *bb, color_t tint,
                       float deerX, float deerY, float targetJumpZ,
                       int vpX, int vpY, int vpW, int vpH, int playerIdx) {
    (void)surf;

    float posX  = local_players[playerIdx].x;
    float posY  = local_players[playerIdx].y;
    float angle = local_players[playerIdx].angle;
    float fovR  = (float)playerFov * (M_PI / 180.0f);

    float dx = deerX - posX;
    float dy = deerY - posY;
    float depth = sqrtf(dx*dx + dy*dy);
    if (depth < 0.1f) return;

    float sprAngle = atan2f(dy, dx);
    float rel = sprAngle - angle;
    while (rel >  M_PI) rel -= 2.0f * M_PI;
    while (rel < -M_PI) rel += 2.0f * M_PI;

    if (fabsf(rel) > fovR * 0.5f + 0.5f) return;

    int vshift       = (int)(local_players[playerIdx].jump_z * (float)vpH);
    int targetVShift = (int)(targetJumpZ * (float)vpH);

    int cX = vpX + (int)((0.5f + rel / fovR) * (float)vpW);
    int sz = (int)((float)vpH / depth);
    if (sz <= 0) return;

    int hOff = cX - sz / 2;
    int vOff = vpY + vpH / 2 + vshift - targetVShift - sz / 2;

    rdpq_set_scissor(vpX, vpY, vpX + vpW, vpY + vpH);
    rdpq_set_mode_standard();
    /* TEX0 * PRIM: texture colour multiplied by tint.
     * White tint (255,255,255,255) = no change to texture colours. */
    rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
    rdpq_set_prim_color(tint);
    rdpq_mode_alphacompare(1);

    for (int c = 0; c < sz; c++) {
        int sx = hOff + c;
        if (sx < vpX || sx >= vpX + vpW) continue;

        int di = sx - vpX;
        if (depthBuf[di] < depth) continue;   /* wall in front */

        int tX = c * bb->texW / sz;
        if (tX >= bb->texW) tX = bb->texW - 1;

        /* Each column has its own permanent 8-byte-aligned address.
         * No copy needed; no race condition with the CPU. */
        uint16_t *col_ptr = (uint16_t *)((uint8_t *)bb->cols
                                         + (size_t)tX * bb->col_stride);
        surface_t col_surf = surface_make(col_ptr, FMT_RGBA16, 1, bb->texH, 2);
        rdpq_tex_upload(TILE0, &col_surf, NULL);
        rdpq_texture_rectangle_scaled(TILE0,
            sx, vOff, sx + 1, vOff + sz,
            0, 0, 1, bb->texH);
    }
}

/* ------------------------------------------------------------------ */
/* drawGunHUD — scaled sprite blit via RDP                             */
/* ------------------------------------------------------------------ */

void drawGunHUD(surface_t *surf, sprite_t *gunIdle, sprite_t *gunFire,
                bool firing, bool moving, uint32_t tick,
                int vpX, int vpY, int vpW, int vpH) {
    (void)surf;

    sprite_t *tex = firing ? gunFire : gunIdle;

    int dstW = vpW / 2;
    int dstH = vpH / 2;
    int dstX = vpX + vpW - dstW;
    int dstY = vpY + vpH - dstH;

    if (moving)
        dstY += (int)(3.0f * sinf((float)tick * 0.15f));

    float scaleX = (float)dstW / (float)tex->width;
    float scaleY = (float)dstH / (float)tex->height;

    rdpq_set_scissor(vpX, vpY, vpX + vpW, vpY + vpH);
    rdpq_set_mode_standard();
    rdpq_mode_combiner(RDPQ_COMBINER_TEX);
    rdpq_mode_alphacompare(1);

    rdpq_sprite_blit(tex, (float)dstX, (float)dstY, &(rdpq_blitparms_t){
        .scale_x = scaleX,
        .scale_y = scaleY,
    });
}

/* ------------------------------------------------------------------ */
/* drawMiniMap — RDP fill rectangles                                    */
/* ------------------------------------------------------------------ */

void drawMiniMap(surface_t *surf, const int mapData[20][20], float xf, float yf,
                 float angle, int tileSize, int mapW, int mapH,
                 int screenW, int screenH) {
    (void)surf; (void)angle; (void)screenH;

    int offX = screenW - mapW * tileSize - 5;
    int offY = 5;

    rdpq_set_scissor(0, 0, screenW, 240);
    rdpq_set_mode_fill(RGBA32(170, 165, 140, 255));

    for (int ty = 0; ty < mapH; ty++) {
        for (int tx = 0; tx < mapW; tx++) {
            rdpq_set_fill_color(mapData[ty][tx] == 1
                ? RGBA32( 50,  50,  50, 255)
                : RGBA32(170, 165, 140, 255));
            rdpq_fill_rectangle(
                offX +  tx      * tileSize, offY +  ty      * tileSize,
                offX + (tx + 1) * tileSize, offY + (ty + 1) * tileSize);
        }
    }

    int dotX = offX + (int)(xf * tileSize);
    int dotY = offY + (int)(yf * tileSize);
    rdpq_set_fill_color(RGBA32(255, 0, 0, 255));
    rdpq_fill_rectangle(dotX - 3, dotY - 3, dotX + 3, dotY + 3);
}

/* ------------------------------------------------------------------ */
/* drawBullets — depth-tested fireball sprites for each active bullet  */
/* ------------------------------------------------------------------ */

void drawBullets(surface_t *surf, const float *depthBuf,
                 int vpX, int vpY, int vpW, int vpH, int playerIdx) {
    (void)surf;

    float posX  = local_players[playerIdx].x;
    float posY  = local_players[playerIdx].y;
    float angle = local_players[playerIdx].angle;
    float fovR  = (float)playerFov * (M_PI / 180.0f);

    int vshift = (int)(local_players[playerIdx].jump_z * (float)vpH);

    bool any = false;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;

        float dx    = bullets[i].x - posX;
        float dy    = bullets[i].y - posY;
        float depth = sqrtf(dx*dx + dy*dy);
        if (depth < 0.2f) continue;   /* skip bullets right in our face */

        float sprAngle = atan2f(dy, dx);
        float rel = sprAngle - angle;
        while (rel >  M_PI) rel -= 2.0f * M_PI;
        while (rel < -M_PI) rel += 2.0f * M_PI;
        if (fabsf(rel) > fovR * 0.5f + 0.3f) continue;

        int cX = vpX + (int)((0.5f + rel / fovR) * (float)vpW);
        int di = cX - vpX;
        if (di < 0 || di >= vpW) continue;
        if (depthBuf[di] < depth) continue;   /* wall is closer */

        /* Fireball: outer orange box + bright yellow core */
        int sz = (int)((float)vpH / depth * 0.12f);
        if (sz < 3)  sz = 3;
        if (sz > 20) sz = 20;

        int sx = cX - sz / 2;
        int sy = vpY + vpH / 2 + vshift - sz / 2;

        if (!any) {
            rdpq_set_scissor(vpX, vpY, vpX + vpW, vpY + vpH);
            rdpq_set_mode_fill(RGBA32(255, 80, 0, 255));
            any = true;
        }

        /* Outer glow: deep orange */
        rdpq_set_fill_color(RGBA32(255, 80, 0, 255));
        rdpq_fill_rectangle(sx, sy, sx + sz, sy + sz);

        /* Mid ring: bright orange */
        int m = sz / 4;
        if (m >= 1) {
            rdpq_set_fill_color(RGBA32(255, 160, 20, 255));
            rdpq_fill_rectangle(sx + m, sy + m, sx + sz - m, sy + sz - m);
        }

        /* Core: hot yellow-white */
        int c = sz / 3;
        if (c >= 1) {
            rdpq_set_fill_color(RGBA32(255, 240, 120, 255));
            rdpq_fill_rectangle(sx + c, sy + c, sx + sz - c, sy + sz - c);
        }
    }
}

/* ------------------------------------------------------------------ */
/* drawViewportBorders — 4-px RDP fill dividers                        */
/* ------------------------------------------------------------------ */

void drawViewportBorders(surface_t *surf, int numPlayers) {
    (void)surf;
    if (numPlayers < 2) return;

    rdpq_set_scissor(0, 0, 320, 240);
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));

    if (numPlayers == 2) {
        rdpq_fill_rectangle(158, 0, 162, 240);
    } else if (numPlayers == 3) {
        rdpq_fill_rectangle(158,   0, 162, 120);
        rdpq_fill_rectangle(  0, 118, 320, 122);
    } else if (numPlayers == 4) {
        rdpq_fill_rectangle(158,   0, 162, 240);
        rdpq_fill_rectangle(  0, 118, 320, 122);
    }
}
