/*
 * Scripted-replay files: parsing and driving (PLAN.md 4.7, 5.3).
 *
 * One parser serves two callers, on purpose:
 *   tests/replay.c   verifies a script's expected final state and hash
 *   src/shell/loop.c --headless runs a script and screenshots it
 *
 * That is what makes the headless renderer's output provably the same run the
 * core tests already pin down: both drive the identical script through the
 * identical core, and both report the same state hash. Two parsers would only
 * be two things to drift apart.
 *
 * This file is pure C99 with no SDL and no platform calls - tests/ links it
 * without linking a renderer. It lives in src/shell/ because driving the core
 * from a script is a shell responsibility, not a core one.
 *
 * File format, one directive per line, # for comments:
 *   seed <u32>
 *   mode <easy|medium|hard>
 *   board <cols> <rows>      optional; defaults to the Vita 48x26
 *   tick_ms <u32>            elapsed_ms handed to game_tick per tick
 *   at <tick> <input>        input is UP/DOWN/LEFT/RIGHT/CONFIRM/PAUSE/BACK/
 *                            CYCLE_MODE
 *   shot <tick> <name>       capture a frame (headless only; ignored by tests)
 *   loop <start> <period> <count>
 *   run <n_ticks>
 *   expect_state <name>
 *   expect_length <n>
 *   expect_hash <0xhex>
 *
 * Ordering within a tick: shots capture the state at the START of the tick,
 * then that tick's inputs are applied, then game_tick runs. A shot whose tick
 * is at or past the end of the run captures the final state instead.
 *
 * `loop` repeats every `at` line whose tick falls in [start, start+period) at
 * tick + period*k for k = 1..count-1. It exists because filling the shipping
 * 46x24 board takes roughly a quarter of a million steps on a Hamiltonian
 * cycle, and writing that out as individual `at` lines would need thousands of
 * them; the cycle's inputs repeat exactly once per lap, so one lap plus a
 * repeat count says the same thing in fifty lines. Only one loop per file, and
 * inputs outside the window still fire exactly once.
 */
#ifndef SCRIPT_H
#define SCRIPT_H

#include "../core/game.h"

#define SCRIPT_MAX_INPUTS 4096
#define SCRIPT_MAX_SHOTS  32
#define SCRIPT_NAME_MAX   32

typedef struct {
    long tick;
    char what[SCRIPT_NAME_MAX];
} ScriptInput;

typedef struct {
    long tick;
    char name[SCRIPT_NAME_MAX];
} ScriptShot;

typedef struct {
    uint32_t    seed;
    int         mode;
    int         cols, rows;
    uint32_t    tick_ms;

    ScriptInput inputs[SCRIPT_MAX_INPUTS];
    int         input_count;
    ScriptShot  shots[SCRIPT_MAX_SHOTS];
    int         shot_count;
    long        total_ticks;

    /* Zero count means no loop; see the `loop` directive above. */
    long        loop_start;
    long        loop_period;
    long        loop_count;

    /* Expectations are optional; a screenshot script need not carry any. */
    int      have_state;
    char     want_state[SCRIPT_NAME_MAX];
    int      have_length;
    long     want_length;
    int      have_hash;
    uint32_t want_hash;
} Script;

/* Parses path into s. Reports the offending line to stderr and returns false. */
bool script_parse(const char *path, Script *s);

/* Called at each `shot` directive. The state is const: a screenshot must not be
 * able to perturb the run it is documenting. */
typedef void (*ScriptShotFn)(void *user, const char *name, const GameState *g);

/* Initialises g from the script and runs it to completion. on_shot may be
 * NULL, which is how the core-only harness ignores `shot` directives. */
bool script_run(const Script *s, GameState *g, ScriptShotFn on_shot,
                void *user);

const char *script_state_name(GameStateId state);

#endif /* SCRIPT_H */
