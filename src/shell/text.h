/*
 * Text drawing behind a tiny interface (PLAN.md 5.1).
 *
 * The interface is deliberately narrower than SDL2_ttf's: draw a string at a
 * point, centre a string, wrap a paragraph, measure. That is everything the
 * game needs, and it is the seam that PLAN.md 6.4's fallback (a baked bitmap
 * atlas) would slot into if SDL2_ttf ever turned out to be unusable on the
 * Vita. Nothing outside this file includes SDL_ttf.h.
 *
 * The original's typography is Verdana/Arial/Helvetica 14px (MECHANICS.md 8.4).
 * assets/font.ttf is DejaVu Sans, whose proportions descend from Bitstream Vera
 * and sit close to Verdana's; see assets/font-LICENSE.txt.
 */
#ifndef TEXT_H
#define TEXT_H

#include <SDL.h>
#include <stdbool.h>

typedef struct TextFont TextFont;

/* Starts and stops the font backend. text_init is idempotent per process. */
bool text_init(void);
void text_shutdown(void);

/* px is the em size in pixels, matching a CSS font-size. NULL on failure;
 * SDL_GetError() carries the reason. */
TextFont *text_open(const char *path, int px);
void      text_close(TextFont *f);

/* Distance between successive baselines, i.e. what to add per rendered line. */
int text_line_height(const TextFont *f);
int text_width(TextFont *f, const char *s);

/* (x, y) is the top-left of the rendered string, matching CSS box placement
 * rather than a typographic baseline. */
void text_draw(SDL_Renderer *r, TextFont *f, const char *s, int x, int y,
               SDL_Color color);

/* Horizontally centres the string on cx; y is still the top edge. */
void text_draw_center(SDL_Renderer *r, TextFont *f, const char *s, int cx,
                      int y, SDL_Color color);

/*
 * Greedy word wrap at max_w pixels, each resulting line centred on cx. Returns
 * the total height drawn, so a caller can stack paragraphs the way the
 * original's dialog divs stack.
 *
 * Pass r = NULL to measure without drawing: the original's welcome dialog has
 * no fixed height, so its box must be sized from its wrapped content first.
 */
int text_draw_wrapped(SDL_Renderer *r, TextFont *f, const char *s, int cx,
                      int y, int max_w, SDL_Color color);

#endif /* TEXT_H */
