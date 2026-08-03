/*
 * All drawing (PLAN.md 5.1). SDL_Renderer only - no window, no events, no
 * timing, so the same calls serve the windowed loop and the headless
 * screenshot path (PLAN.md 5.3).
 *
 * Every number in render.c comes from MECHANICS.md 8 (colors, typography,
 * strings) and MECHANICS.md 9 (the 960x544 layout). Change the spec first.
 */
#ifndef RENDER_H
#define RENDER_H

#include "../core/game.h"
#include "text.h"

#include <SDL.h>

/* The Vita panel, and therefore the desktop window: what is verified is what
 * ships (PLAN.md 5.4). */
#define SCREEN_W 960
#define SCREEN_H 544

/*
 * Theme table (PLAN.md 0.6: themes must be data, so adding one later is a data
 * change). Every value comes from a reference stylesheet or a sampled block
 * image, cited per column in PLAN-THEMES.md 2.
 *
 * The reference styles the pause screen separately from the welcome and
 * end-game dialogs, and the three disagree, so they get their own fields rather
 * than sharing overlay_*. Main is the same colour in all three, which is why
 * v1 could borrow one for the others (PLAN-THEMES.md 3).
 */
typedef struct {
    const char *name;
    const char *author;         /* credited in the welcome dialog */
    SDL_Color   background;     /* outside the playfield  */
    SDL_Color   playfield;
    SDL_Color   snake;
    SDL_Color   snake_dead;     /* the head only, on death */
    SDL_Color   food;
    SDL_Color   hud_text;
    SDL_Color   overlay_bg;
    SDL_Color   overlay_text;   /* welcome dialog          */
    SDL_Color   overlay_text_end; /* death and win dialogs */
    SDL_Color   pause_bg;
    SDL_Color   pause_text;
    SDL_Color   button_border;
} Theme;

#define THEME_MAIN     0
#define THEME_MATRIX   1
#define THEME_ORIGINAL 2

int          theme_count(void);
const Theme *theme_get(int index); /* clamped; never returns NULL */

typedef struct {
    TextFont *font;  /* 14px, the original's body size            */
    TextFont *small; /* 10px, for the derivative-work credit line */
    int       theme;
} RenderCtx;

/* Opens the packaged font via plat_asset_path. False on failure, with the
 * reason in SDL_GetError(). */
bool render_init(RenderCtx *rc, int theme);
void render_shutdown(RenderCtx *rc);

/* Draws one complete frame. Does not present. */
void render_frame(SDL_Renderer *r, RenderCtx *rc, const GameState *g);

/*
 * The hidden button-index diagnostic, drawn over the frame.
 * `buttons` is a bit per held joystick button index; the panel names the ones
 * the game binds so the player can verify, on hardware, which index each
 * physical button produces. Nothing else in the game depends on it.
 */
void render_diagnostic(SDL_Renderer *r, RenderCtx *rc, uint32_t buttons);

#endif /* RENDER_H */
