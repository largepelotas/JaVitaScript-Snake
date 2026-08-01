/*
 * Scripted-replay parsing and driving. See script.h for the file format and
 * for why this is shared between the test harness and the headless renderer.
 */
#include "script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCRIPT_LINE_MAX 256

const char *script_state_name(GameStateId state)
{
    switch (state) {
    case STATE_WELCOME: return "WELCOME";
    case STATE_READY:   return "READY";
    case STATE_PLAYING: return "PLAYING";
    case STATE_PAUSED:  return "PAUSED";
    case STATE_DEAD:    return "DEAD";
    case STATE_WON:     return "WON";
    }
    return "?";
}

static int mode_from_name(const char *s)
{
    if (strcmp(s, "easy") == 0)   return MODE_EASY;
    if (strcmp(s, "medium") == 0) return MODE_MEDIUM;
    if (strcmp(s, "hard") == 0)   return MODE_HARD;
    return -1;
}

/* Returns 1 if the token was a direction and writes it to out. */
static int direction_from_name(const char *s, Direction *out)
{
    if (strcmp(s, "UP") == 0)    { *out = DIR_UP;    return 1; }
    if (strcmp(s, "DOWN") == 0)  { *out = DIR_DOWN;  return 1; }
    if (strcmp(s, "LEFT") == 0)  { *out = DIR_LEFT;  return 1; }
    if (strcmp(s, "RIGHT") == 0) { *out = DIR_RIGHT; return 1; }
    return 0;
}

static int action_from_name(const char *s, GameAction *out)
{
    if (strcmp(s, "CONFIRM") == 0) { *out = ACTION_CONFIRM; return 1; }
    if (strcmp(s, "PAUSE") == 0)   { *out = ACTION_PAUSE;   return 1; }
    if (strcmp(s, "BACK") == 0)    { *out = ACTION_BACK;    return 1; }
    /* Square. A script can therefore park the game on a non-default difficulty
     * and screenshot the welcome screen showing it. */
    if (strcmp(s, "CYCLE_MODE") == 0) { *out = ACTION_CYCLE_MODE; return 1; }
    return 0;
}

bool script_parse(const char *path, Script *s)
{
    FILE *f;
    char  line[SCRIPT_LINE_MAX];
    int   lineno = 0;

    memset(s, 0, sizeof *s);
    s->seed    = 1u;
    s->mode    = MODE_MEDIUM;
    s->cols    = BOARD_MAX_COLS;
    s->rows    = BOARD_MAX_ROWS;
    s->tick_ms = 16u;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "script: cannot open %s\n", path);
        return false;
    }

    while (fgets(line, sizeof line, f)) {
        char key[32], a[32], b[32], c[32];
        int  n;

        lineno++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        n = sscanf(line, "%31s %31s %31s %31s", key, a, b, c);
        if (n < 1) {
            continue;
        }

        if (strcmp(key, "seed") == 0 && n >= 2) {
            s->seed = (uint32_t)strtoul(a, NULL, 0);
        } else if (strcmp(key, "mode") == 0 && n >= 2) {
            s->mode = mode_from_name(a);
            if (s->mode < 0) {
                fprintf(stderr, "%s:%d: unknown mode '%s'\n", path, lineno, a);
                fclose(f);
                return false;
            }
        } else if (strcmp(key, "board") == 0 && n >= 3) {
            s->cols = atoi(a);
            s->rows = atoi(b);
        } else if (strcmp(key, "tick_ms") == 0 && n >= 2) {
            s->tick_ms = (uint32_t)strtoul(a, NULL, 0);
        } else if (strcmp(key, "at") == 0 && n >= 3) {
            if (s->input_count >= SCRIPT_MAX_INPUTS) {
                fprintf(stderr, "%s:%d: too many inputs\n", path, lineno);
                fclose(f);
                return false;
            }
            s->inputs[s->input_count].tick = strtol(a, NULL, 10);
            snprintf(s->inputs[s->input_count].what,
                     sizeof s->inputs[0].what, "%s", b);
            s->input_count++;
        } else if (strcmp(key, "shot") == 0 && n >= 3) {
            if (s->shot_count >= SCRIPT_MAX_SHOTS) {
                fprintf(stderr, "%s:%d: too many shots\n", path, lineno);
                fclose(f);
                return false;
            }
            s->shots[s->shot_count].tick = strtol(a, NULL, 10);
            snprintf(s->shots[s->shot_count].name, sizeof s->shots[0].name,
                     "%s", b);
            s->shot_count++;
        } else if (strcmp(key, "loop") == 0 && n >= 4) {
            if (s->loop_count != 0) {
                fprintf(stderr, "%s:%d: only one loop per file\n", path,
                        lineno);
                fclose(f);
                return false;
            }
            s->loop_start  = strtol(a, NULL, 10);
            s->loop_period = strtol(b, NULL, 10);
            s->loop_count  = strtol(c, NULL, 10);
            if (s->loop_period <= 0 || s->loop_count <= 0 ||
                s->loop_start < 0) {
                fprintf(stderr, "%s:%d: loop needs start >= 0, period > 0, "
                                "count > 0\n", path, lineno);
                fclose(f);
                return false;
            }
        } else if (strcmp(key, "run") == 0 && n >= 2) {
            s->total_ticks += strtol(a, NULL, 10);
        } else if (strcmp(key, "expect_state") == 0 && n >= 2) {
            s->have_state = 1;
            snprintf(s->want_state, sizeof s->want_state, "%s", a);
        } else if (strcmp(key, "expect_length") == 0 && n >= 2) {
            s->have_length = 1;
            s->want_length = strtol(a, NULL, 10);
        } else if (strcmp(key, "expect_hash") == 0 && n >= 2) {
            s->have_hash = 1;
            s->want_hash = (uint32_t)strtoul(a, NULL, 0);
        } else {
            fprintf(stderr, "%s:%d: unrecognised directive '%s'\n", path,
                    lineno, key);
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

/*
 * Whether a scripted input fires on this tick, directly or as a loop repeat.
 * Iteration 0 is the input's own tick, so a loop of count N replays the window
 * N times in total.
 */
static bool input_fires(const Script *s, const ScriptInput *in, long tick)
{
    long delta, iteration;

    if (in->tick == tick) {
        return true;
    }
    if (s->loop_count <= 1 || tick <= in->tick) {
        return false;
    }
    if (in->tick < s->loop_start ||
        in->tick >= s->loop_start + s->loop_period) {
        return false;
    }

    delta = tick - in->tick;
    if (delta % s->loop_period != 0) {
        return false;
    }
    iteration = delta / s->loop_period;
    return iteration < s->loop_count;
}

static bool apply_inputs(const Script *s, GameState *g, long tick)
{
    int i;

    for (i = 0; i < s->input_count; i++) {
        Direction  dir;
        GameAction act;

        if (!input_fires(s, &s->inputs[i], tick)) {
            continue;
        }
        if (direction_from_name(s->inputs[i].what, &dir)) {
            (void)game_queue_input(g, dir);
        } else if (action_from_name(s->inputs[i].what, &act)) {
            game_action(g, act);
        } else {
            fprintf(stderr, "script: unknown input '%s'\n", s->inputs[i].what);
            return false;
        }
    }
    return true;
}

static void fire_shots(const Script *s, const GameState *g, long tick,
                       bool final, ScriptShotFn on_shot, void *user)
{
    int i;

    if (!on_shot) {
        return;
    }
    for (i = 0; i < s->shot_count; i++) {
        long at = s->shots[i].tick;

        if (final ? at >= s->total_ticks : at == tick) {
            on_shot(user, s->shots[i].name, g);
        }
    }
}

bool script_run(const Script *s, GameState *g, ScriptShotFn on_shot, void *user)
{
    long tick;

    if (!game_init(g, s->mode, s->seed, s->cols, s->rows)) {
        fprintf(stderr, "script: board %dx%d rejected\n", s->cols, s->rows);
        return false;
    }

    for (tick = 0; tick < s->total_ticks; tick++) {
        fire_shots(s, g, tick, false, on_shot, user);
        if (!apply_inputs(s, g, tick)) {
            return false;
        }
        (void)game_tick(g, s->tick_ms);
    }

    fire_shots(s, g, tick, true, on_shot, user);
    return true;
}
