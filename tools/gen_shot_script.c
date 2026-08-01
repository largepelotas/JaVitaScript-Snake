/*
 * Generates scripted-replay files that would be impractical to write by hand
 * (PLAN.md 5.3 wants a long-snake screenshot and a win screenshot).
 *
 * Two policies:
 *
 *   greedy  head toward the food, avoiding cells that are occupied. Food
 *           positions come from the seeded RNG, so a script that eats twenty
 *           times has to know where the food will be - which is exactly why
 *           this cannot be authored by hand. Dies eventually; that is fine,
 *           it is a screenshot of a mid-game board.
 *
 *   cycle   follow a Hamiltonian cycle over the whole interior. The head's
 *           path is independent of the food, so it visits every cell every
 *           lap, eats everything, and fills the board - the only way to reach
 *           WON on the shipping 46x24 grid. The inputs repeat exactly once per
 *           lap, so the output uses one `loop` directive instead of thousands
 *           of `at` lines.
 *
 * Neither policy is a game feature and neither links into the shipped binary
 * (PLAN.md 0.6 puts AI-driver hooks out of scope). They only write test assets,
 * and the assets are checked in so the screenshots are reproducible without
 * this tool.
 *
 * Driving order here mirrors src/shell/script.c exactly - inputs for tick N,
 * then one game_tick - which is what makes the emitted script replay to the
 * identical state.
 *
 * Build:  make build-host/gen_shot_script
 * Usage:  gen_shot_script greedy <seed> <easy|medium|hard> <max_ticks> > out.txt
 *         gen_shot_script cycle  <seed> <easy|medium|hard> <max_ticks> > out.txt
 *
 * Checked-in output, both blessed afterwards by build-host/replay so they carry
 * an expected hash like any other replay:
 *   tests/shots/long_snake.txt        greedy 4 medium 6000
 *   tests/replays/06_full_board_win.txt  cycle 1 hard 400000
 */
#include "../src/core/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int row_shift[4] = { -1, 0, 1, 0 };
static const int col_shift[4] = { 0, 1, 0, -1 };

static const char *dir_name(Direction d)
{
    switch (d) {
    case DIR_UP:    return "UP";
    case DIR_RIGHT: return "RIGHT";
    case DIR_DOWN:  return "DOWN";
    case DIR_LEFT:  return "LEFT";
    case DIR_NONE:  break;
    }
    return "?";
}

/* A step into this cell is fatal unless the tail is about to vacate it, and the
 * autopilot does not try to be clever about that. */
static bool blocked(const GameState *g, int row, int col)
{
    return board_get(&g->board, row, col) > 0;
}

/*
 * Greedy: close the larger of the two gaps to the food first, fall back to the
 * other axis, then to any survivable direction. Reversals are excluded because
 * the core would reject them anyway (MECHANICS.md 4.3).
 */
static void push(Direction *want, int *n, Direction d)
{
    int i;

    for (i = 0; i < *n; i++) {
        if (want[i] == d) {
            return;
        }
    }
    if (*n < 4) {
        want[(*n)++] = d;
    }
}

static Direction choose(const GameState *g)
{
    Cell      head = snake_head(&g->snake);
    int       drow = g->food.row - head.row;
    int       dcol = g->food.col - head.col;
    int       arow = drow < 0 ? -drow : drow;
    int       acol = dcol < 0 ? -dcol : dcol;
    Direction want[4];
    int       n = 0, i, d;

    if (arow >= acol) {
        if (drow != 0) push(want, &n, drow > 0 ? DIR_DOWN : DIR_UP);
        if (dcol != 0) push(want, &n, dcol > 0 ? DIR_RIGHT : DIR_LEFT);
    } else {
        if (dcol != 0) push(want, &n, dcol > 0 ? DIR_RIGHT : DIR_LEFT);
        if (drow != 0) push(want, &n, drow > 0 ? DIR_DOWN : DIR_UP);
    }
    for (d = 0; d < 4; d++) {
        push(want, &n, (Direction)d); /* any survivable direction will do */
    }

    for (i = 0; i < n; i++) {
        Direction c = want[i];

        if (g->last_move != DIR_NONE) {
            int diff = (int)c - (int)g->last_move;

            if (diff == 2 || diff == -2) {
                continue; /* the core would reject it anyway */
            }
        }
        if (!blocked(g, head.row + row_shift[c], head.col + col_shift[c])) {
            return c;
        }
    }
    return DIR_NONE;
}

/*
 * The Hamiltonian cycle, as a rule rather than a stored path.
 *
 * Interior rows 1..R and cols 1..C, with R even (the Vita board is 46x24):
 *
 *   row 1        traversed left to right, then down at the right edge
 *   col 1        the return spine, traversed upward
 *   even rows    right to left, then down at col 2 (or left into the spine
 *                on the last row)
 *   odd rows     left to right, then down at the right edge
 *
 * The result is one closed loop through all R*C cells, so a snake following it
 * passes over every free cell each lap and cannot meet its own body until the
 * board is full. Note the path never depends on where the food is, which is
 * what makes the input pattern exactly periodic.
 */
static Direction cycle_dir(const GameState *g)
{
    Cell head = snake_head(&g->snake);
    int  rows = board_interior_rows(&g->board);
    int  cols = board_interior_cols(&g->board);
    int  r    = head.row;
    int  c    = head.col;

    if (r == 1) {
        return c < cols ? DIR_RIGHT : DIR_DOWN;
    }
    if (c == 1) {
        return DIR_UP;
    }
    if (r % 2 == 0) {
        if (c > 2) {
            return DIR_LEFT;
        }
        return r == rows ? DIR_LEFT : DIR_DOWN;
    }
    return c < cols ? DIR_RIGHT : DIR_DOWN;
}

static const char *state_name(const GameState *g)
{
    switch (g->state) {
    case STATE_DEAD: return "DEAD";
    case STATE_WON:  return "WON";
    default:         return "UNFINISHED";
    }
}

static int gen_greedy(GameState *g, uint32_t tick_ms, long max_ticks)
{
    Direction last_queued = DIR_NONE;
    long      tick;
    int       best_len  = 0;
    long      best_tick = 0;

    printf("# Greedy autopilot toward the food, so the snake grows long enough\n"
           "# for the playing_long_snake screenshot (PLAN.md 5.3). Regenerate\n"
           "# with the same arguments to reproduce it exactly.\n");
    printf("at 0 CONFIRM\n");

    for (tick = 0; tick < max_ticks; tick++) {
        if (tick == 0) {
            game_action(g, ACTION_CONFIRM);
        } else if (g->state == STATE_READY || g->state == STATE_PLAYING) {
            Direction want = choose(g);

            if (want != DIR_NONE && want != last_queued) {
                printf("at %ld %s\n", tick, dir_name(want));
                (void)game_queue_input(g, want);
                last_queued = want;
            }
        }

        game_tick(g, tick_ms);

        if (game_length(g) > best_len) {
            best_len  = game_length(g);
            best_tick = tick;
        }
        if (g->state == STATE_DEAD || g->state == STATE_WON) {
            tick++;
            break;
        }
    }

    printf("run %ld\n", tick);
    fprintf(stderr, "final state=%s length=%d ticks=%ld longest=%d at tick %ld\n",
            state_name(g), game_length(g), tick, best_len, best_tick);
    return 0;
}

/*
 * Cycle mode. Two things have to settle before the input pattern repeats.
 *
 * First, the starting press takes a step immediately and the same tick's
 * game_tick takes another (MECHANICS.md 6.6), so tick 1 is a two-step
 * transient that cuts a corner off the cycle.
 *
 * Second - and this is the one that is easy to miss - the head's POSITION is
 * periodic one lap earlier than the INPUTS are. The transient enters the first
 * lap's turn already heading down, so no input is queued there, while every
 * later lap arrives heading left and has to turn. The first lap is therefore
 * written out in full and the loop starts at the second.
 *
 * None of this is taken on faith: every lap after the recorded one is compared
 * against it, and a mismatch aborts rather than emitting a script that would
 * replay into a different game.
 */
static int gen_cycle(GameState *g, uint32_t tick_ms, long max_ticks)
{
    static Direction window[SNAKE_MAX_CELLS]; /* DIR_NONE where nothing changed */
    Direction last_queued  = DIR_NONE;
    long      period       = (long)board_interior_rows(&g->board) *
                             board_interior_cols(&g->board);
    long      window_start = 2 + period; /* transient, then one full lap */
    long      window_end   = window_start + period;
    long      tick;
    long      i;

    if (board_interior_rows(&g->board) % 2 != 0) {
        fprintf(stderr, "cycle: needs an even number of interior rows\n");
        return 1;
    }
    if (period > (long)SNAKE_MAX_CELLS) {
        fprintf(stderr, "cycle: period %ld exceeds the recording window\n",
                period);
        return 1;
    }
    for (i = 0; i < period; i++) {
        window[i] = DIR_NONE;
    }

    printf("# Hamiltonian cycle over the whole interior: the snake visits every\n"
           "# cell on every lap, so it eats every food and eventually fills the\n"
           "# board. This is the only way to reach WON on the shipping 46x24\n"
           "# grid, and it is what the won.png screenshot is taken from.\n"
           "#\n"
           "# The inputs repeat exactly once per lap, so all but the first lap\n"
           "# are expressed by the `loop` directive (see src/shell/script.h).\n");
    printf("at 0 CONFIRM\n");

    for (tick = 0; tick < max_ticks; tick++) {
        Direction want = DIR_NONE;

        if (tick == 0) {
            game_action(g, ACTION_CONFIRM);
        } else if (g->state == STATE_READY || g->state == STATE_PLAYING) {
            want = cycle_dir(g);
            if (want == last_queued) {
                want = DIR_NONE; /* already heading that way */
            } else {
                (void)game_queue_input(g, want);
                last_queued = want;
            }
        }

        if (tick < window_start) {
            if (want != DIR_NONE) {
                printf("at %ld %s\n", tick, dir_name(want));
            }
        } else if (tick < window_end) {
            window[(tick - window_start) % period] = want;
            if (want != DIR_NONE) {
                printf("at %ld %s\n", tick, dir_name(want));
            }
        } else if (window[(tick - window_start) % period] != want) {
            fprintf(stderr,
                    "cycle: lap %ld diverges at tick %ld: %s, recorded lap had "
                    "%s\n", (tick - window_start) / period, tick,
                    dir_name(want),
                    dir_name(window[(tick - window_start) % period]));
            return 1;
        }

        game_tick(g, tick_ms);

        if (g->state == STATE_DEAD || g->state == STATE_WON) {
            tick++;
            break;
        }
    }

    if (g->state != STATE_WON) {
        fprintf(stderr, "cycle: ended %s after %ld ticks, not WON\n",
                state_name(g), tick);
        return 1;
    }

    /* Ceiling division: the final, partial lap still has to be replayed. */
    printf("loop %ld %ld %ld\n", window_start, period,
           (tick - window_start + period - 1) / period);
    printf("run %ld\n", tick);
    fprintf(stderr, "final state=%s length=%d ticks=%ld laps=%ld\n",
            state_name(g), game_length(g), tick,
            (tick - window_start + period - 1) / period);
    return 0;
}

int main(int argc, char **argv)
{
    GameState   g;
    uint32_t    seed;
    int         mode = MODE_MEDIUM;
    long        max_ticks;
    uint32_t    tick_ms;
    const char *policy;

    if (argc != 5) {
        fprintf(stderr,
                "usage: %s <greedy|cycle> <seed> <easy|medium|hard> "
                "<max_ticks>\n", argv[0]);
        return 2;
    }
    policy    = argv[1];
    seed      = (uint32_t)strtoul(argv[2], NULL, 0);
    max_ticks = strtol(argv[4], NULL, 10);

    if (strcmp(argv[3], "easy") == 0)        mode = MODE_EASY;
    else if (strcmp(argv[3], "medium") == 0) mode = MODE_MEDIUM;
    else if (strcmp(argv[3], "hard") == 0)   mode = MODE_HARD;
    else {
        fprintf(stderr, "unknown mode '%s'\n", argv[3]);
        return 2;
    }

    /* One tick per step keeps the emitted tick numbers readable and makes every
     * turn land on an exact step boundary. */
    tick_ms = mode_get(mode)->step_ms;

    if (!game_init_vita(&g, mode, seed)) {
        fprintf(stderr, "game_init_vita failed\n");
        return 1;
    }

    printf("# Generated by tools/gen_shot_script.c: %s %s %s %s\n", argv[1],
           argv[2], argv[3], argv[4]);
    printf("seed %u\n", (unsigned)seed);
    printf("mode %s\n", argv[3]);
    printf("tick_ms %u\n", (unsigned)tick_ms);

    if (strcmp(policy, "greedy") == 0) {
        return gen_greedy(&g, tick_ms, max_ticks);
    }
    if (strcmp(policy, "cycle") == 0) {
        return gen_cycle(&g, tick_ms, max_ticks);
    }
    fprintf(stderr, "unknown policy '%s'\n", policy);
    return 2;
}
