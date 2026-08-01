/*
 * SDL events -> core inputs (PLAN.md 5.1).
 *
 * The shell never mutates the game state directly: every direction goes through
 * game_queue_input and every button through game_action (PLAN.md 4.4), so the
 * premove queue behaves identically no matter which device produced the input.
 *
 * Raw button indices never appear here - they come from plat_button_for, whose
 * table is the single thing Phase 4 has to correct for real hardware
 * (PLAN.md 6.5).
 */
#ifndef INPUT_H
#define INPUT_H

#include "../core/game.h"

#include <SDL.h>

/* Indices above this are still counted as pressed, they just do not fit in the
 * bitmask the diagnostic displays. A Vita reports 16. */
#define INPUT_MAX_BUTTONS 32

typedef struct {
    SDL_Joystick *pad;
    Direction     stick_dir; /* last discrete direction the analog stick held */
    int           axis_x;    /* latest raw left-stick deflection */
    int           axis_y;
    uint32_t      buttons;   /* bit per currently-held joystick button index */
} InputState;

/* Opens joystick 0 if one is attached. Harmless when none is. */
void input_init(InputState *in);
void input_shutdown(InputState *in);

/* Translates one event. Returns false when the player asked to quit. */
bool input_handle(InputState *in, const SDL_Event *e, GameState *g);

/*
 * The hidden button-index diagnostic: true while both shoulder buttons are
 * held. The Vita's SDL button indices cannot be confirmed without hardware, so
 * the shell shows which index each physical button actually produces for
 * verification on first run.
 *
 * Whether it is allowed to appear - only on the welcome screen - is the loop's
 * decision, not this module's.
 */
bool input_diag_active(const InputState *in);

#endif /* INPUT_H */
