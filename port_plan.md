Blekenstein64: Port game/draw from Blekenstein3D and local multiplayer state

Current state





Blekenstein3D (main.c, game.c, draw.c, client.c): SDL-based raycast game with one local player, socket client sending UpdateMessage (x, y, angle, isFiring) and reading back other players from a server.



Blekenstein64 (src/main.c): Libdragon app with title/menu and player count (1–4); game.h and draw.h mirror 3D but use SDL types and there is no game.c/draw.c or client.

1. Local multiplayer primitives (replace client)

Do not port client.h / client.c. Replace with a small local state module.





Data: Same logical fields as UpdateMessage: float x, y, angle; bool is_firing; plus optional int health for future use.



Structure: Define a single struct, e.g. player_state_t, and a fixed-size array plus count:





#define MAX_LOCAL_PLAYERS 4



player_state_t local_players[MAX_LOCAL_PLAYERS];



int num_local_players; (set from menu: 1–4).



Location: Either a new header/source pair (e.g. local_players.h / local_players.c) or add to game.h / game.c. Keeping it in game is simpler: one place for “all players” state.



Update: Each frame, for i = 0 .. num_local_players-1, read joypad_get_buttons(JOYPAD_PORT_1 + i) and apply movement/rotation/fire to local_players[i] (or to a single “camera” player and others as sprites). No sockets, no createAndSendUpdateMessage / readUpdateMessages.

flowchart LR
  subgraph b3d [Blekenstein3D]
    Input[Keyboard]
    Client[client.c]
    Game[game.c]
    Input --> Game
    Game --> Client
    Client -->|socket| Server[Server]
    Server -->|readUpdateMessages| Client
  end
  subgraph b64 [Blekenstein64]
    Joy[Joypad 1..4]
    Local[local_players array]
    Game64[game.c]
    Joy --> Game64
    Game64 --> Local
  end

2. game.h / game.c (Blekenstein64)





game.h





Keep: extern const int map[20][20]; (name consistently map or MAP; 3D uses MAP in code, map in header — pick one).



Depth buffer: 3D uses screenWidth (1920); N64 is 320. Use a constant, e.g. #define DEPTH_BUF_W 320 and extern float depthBuffer[DEPTH_BUF_W]; (or similar).



Player API: Keep getPlayerPosition, moveForward, moveBackward, moveLeft, moveRight, rotateLeft, rotateRight — they operate on “the current player.” Add declarations for local multiplayer: player_state_t *get_local_player(int i); and void set_camera_player_index(int i); (or equivalent) so the view follows one slot.



Define player_state_t and MAX_LOCAL_PLAYERS here (or in a tiny players.h included by game.h).



game.c





Port from Blekenstein3D/game.c: same MAP, same movement/rotation logic.



Add the local_players[] array and num_local_players.



Implement “current player” either as globals (e.g. player_x, player_y, player_angle synced from local_players[camera_index]) or by having movement functions take a player_state_t* (cleaner for 2–4 players). Prefer one “camera” player whose state is used for raycast view and others drawn as sprites from local_players[].

3. draw.h / draw.c (Blekenstein64) — SDL to Libdragon

Replace SDL types and calls with Libdragon equivalents:







Blekenstein3D (SDL)



Blekenstein64 (Libdragon)





SDL_Renderer*



surface_t* (from display_get())





SDL_Color



uint32_t via graphics_make_color(r,g,b,a)





SDL_Texture*



No direct equivalent; use a color buffer (array of uint32_t or color_t) for wall texture sampling, and draw with pixel/line





SDL_RenderDrawPoint



graphics_draw_pixel(surf, x, y, color)





SDL_RenderDrawLine



graphics_draw_line(surf, x0, y0, x1, y1, color)





SDL_RenderClear



graphics_fill_screen(surf, color)





draw.h





Declare the same logical functions: drawPixel(surface_t *surf, int x, int y, uint32_t color);, drawColumn(surface_t *surf, uint32_t *wallColorBuffer, int x, int screenHeight, float depth, uint32_t topBottomColor, int fixedTextureX);, drawMiniMap(..., float depthBuffer[DEPTH_BUF_W]);, drawPlayer(...).



Use surface_t* and uint32_t for colors; wall “texture” becomes a precomputed color buffer (width × height) passed where 3D passes SDL_Color *pixels and texture.



draw.c





Port Blekenstein3D/draw.c: drawPixel → graphics_draw_pixel; drawColumn → segment loop with graphics_draw_line (and optional graphics_draw_pixel for texture column); getPixelColor from a uint32_t* buffer (unpack with graphics_make_color or RGBA32 macro); drawMiniMap → same algorithm, graphics_draw_pixel / graphics_draw_line; drawPlayer same idea with uint32_t color.



Depth buffer size: use DEPTH_BUF_W (320) instead of 1920 so it fits N64 resolution and stack/RAM.

4. main.c (Blekenstein64) — no client, use local state





Remove: Any reference to sockets, client.h, create_and_connect_socket, createAndSendUpdateMessage, readUpdateMessages, close_socket.



After menu: Set num_local_players from the selected player count; initialize local_players[i] (e.g. spawn positions per slot).



Game loop (once you add the raycast view):





joypad_poll(); for each port i in [0, num_local_players), read buttons and call movement/rotate/fire for local_players[i] (or only for the camera player and optionally other slots).



Raycast: fill depthBuffer[] inside drawMiniMap (as in 3D) or in a separate step; then for each column index i, drawColumn(disp, wallColorBuffer, i, screenHeight, depthBuffer[i], ...).



Draw sprites for other players from local_players[] (same math as 3D deer/sprite, using their x,y,angle).



Draw minimap, HUD, gun; then display_show(disp).



Assets: 3D loads BMPs from disk; N64 has no SDL_LoadBMP. Use ROM/romfs or embedded data for wall texture and sprites, or start with a solid/pattern color buffer and graphics_draw_box/pixel art so the pipeline works first.

5. Build and constants





Makefile: Add game.c and draw.c to OBJS so they are compiled and linked.



Depth buffer: In 3D, main.c has const DB_SIZE = screenWidth (and uses it for a local float depthBuffer[DB_SIZE]). In 64, use a named constant (e.g. DEPTH_BUF_W 320) in game.h or draw.h and one shared float depthBuffer[DEPTH_BUF_W] (in game.c or main.c) to avoid magic numbers and match 320-wide display.

6. Summary of files







File



Action





Blekenstein64/src/game.h



Add player_state_t, MAX_LOCAL_PLAYERS, depth buffer constant; keep movement API; add local-player accessors if needed.





Blekenstein64/src/game.c



New: Port 3D game.c; add local_players[] and update logic from joypad (no client).





Blekenstein64/src/draw.h



Switch to surface_t*, uint32_t colors, color-buffer for wall; match draw.c signatures.





Blekenstein64/src/draw.c



New: Port 3D draw.c with Libdragon graphics_* calls.





Blekenstein64/src/main.c



Keep menu; add game loop that uses local_players, game, and draw; remove all client/socket code.





Blekenstein64/Makefile



Add game.o, draw.o to build.





client.h / client.c



Do not add; replaced by in-process local_players state.

7. Optional: drawColumn segment width

3D uses segmentWidth = screenWidth / DB_SIZE and draws one column per i. For 320×240, using 320 columns is 1 pixel per column (slow but correct). For speed, you can use a smaller DEPTH_BUF_W (e.g. 160) and draw each column as a few pixels wide (e.g. 2px) so the API stays the same but with fewer rays.