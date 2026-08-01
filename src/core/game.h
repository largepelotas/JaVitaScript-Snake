/*
 * Game state machine, input queue, tick and scoring.
 *
 * PLAN.md 4.2: game_tick is a pure function of (state, elapsed_ms). It must not
 * read a clock, allocate, or touch I/O. All randomness comes from g->rng, which
 * the caller seeds. Rendering reads the resulting state; the core never draws.
 */
#ifndef GAME_H
#define GAME_H

#include "board.h"
#include "modes.h"
#include "rng.h"
#include "snake.h"
#include "snake_types.h"

/*
 * Zeroes the whole struct, seeds the RNG, builds a cols x rows board
 * (dimensions INCLUDE the wall ring) and places the snake and first food.
 * State is STATE_WELCOME: the original draws a live board behind the welcome
 * overlay, so the board is set up before the player confirms.
 *
 * Returns false if the board dimensions are rejected.
 */
bool game_init(GameState *g, int mode, uint32_t seed, int cols, int rows);

/* The Vita board: 48x26 including walls, 46x24 playable (MECHANICS.md 9). */
bool game_init_vita(GameState *g, int mode, uint32_t seed);

/*
 * Advances time. Returns the OR of every GameEvent that occurred, which may
 * cover several steps if elapsed_ms spans more than one interval.
 *
 * A large elapsed_ms produces the correct number of steps and stops at the
 * first death or win rather than running past it. Clamping belongs to the
 * shell (PLAN.md 5.2), not here.
 */
GameEvent game_tick(GameState *g, uint32_t elapsed_ms);

/*
 * Queues a direction. The shell calls only this; it never mutates direction
 * directly (PLAN.md 4.4). Implements the original's premove queue of depth 1
 * and its reversal rule (MECHANICS.md 4.3, 4.4).
 *
 * Ignored unless the state accepts input.
 *
 * From STATE_READY the first direction starts the game AND takes the first step
 * immediately, mirroring the original's synchronous go() on the starting
 * keypress (snake.js:1338). That step's events are the return value, so a
 * caller that only watches game_tick would miss them; in every other case the
 * return is EVENT_NONE.
 *
 * dir must be a real direction; DIR_NONE is rejected. The original routes every
 * keypress through this path and lets DIR_NONE corrupt the direction state -
 * that bug is deliberately not ported (MECHANICS.md 4.6, deviation 4).
 */
GameEvent game_queue_input(GameState *g, Direction dir);

/*
 * Confirm (Cross), pause (Start), back (Circle), cycle difficulty (Square).
 * Out-of-context presses are ignored rather than being treated as errors.
 *
 * ACTION_CYCLE_MODE advances g->mode by one, wrapping, and is accepted ONLY in
 * STATE_WELCOME. The original picks its mode from a dropdown that is disabled
 * during play (`snake.js:174-191`); the Vita has no dropdown, so the welcome
 * screen is the equivalent quiet moment. Allowing it mid-game would change the
 * step interval underneath a running snake.
 *
 * The core does not persist the choice - it does no I/O (PLAN.md 4.2). The
 * shell watches g->mode and writes the save record.
 */
void game_action(GameState *g, GameAction action);

/* Displayed Length, i.e. including growth not yet materialised. */
int game_length(const GameState *g);

/* Seeds the persisted highscore at startup. The core never does I/O; the
 * platform layer reads the file and hands the value in. */
void game_set_highscore(GameState *g, int value);

/*
 * Order-independent digest of everything that defines behavior, for the replay
 * harness (PLAN.md 4.7). Hashes named fields rather than struct bytes so that
 * padding and future field reordering cannot silently change the result.
 */
uint32_t game_state_hash(const GameState *g);

#endif /* GAME_H */
