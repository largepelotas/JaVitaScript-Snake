/*
 * Core tests (PLAN.md 4.6). Plain asserts, one binary, run via `make test`.
 * No SDL, no Vita headers, no clock.
 *
 * Every case here traces to a specific claim in docs/MECHANICS.md; the comment
 * on each test names the section it is defending.
 */
#include "../src/core/game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                             \
            printf("\n    condition: %s\n", #cond);                          \
        }                                                                    \
    } while (0)

static void banner(const char *name)
{
    printf("== %s\n", name);
}

/*
 * Drives the game from WELCOME to a moving snake heading in `dir`.
 *
 * Note that the starting press takes the first step immediately, exactly as
 * the original's synchronous go() does, so on return the snake has ALREADY
 * moved one cell. Tests below count from there.
 */
static GameEvent start_game(GameState *g, Direction dir)
{
    game_action(g, ACTION_CONFIRM);
    return game_queue_input(g, dir);
}

/* Runs exactly n steps at the current mode's interval. */
static GameEvent run_steps(GameState *g, int n)
{
    GameEvent ev = EVENT_NONE;
    uint32_t  step_ms = mode_get(g->mode)->step_ms;
    int       i;

    for (i = 0; i < n; i++) {
        ev |= game_tick(g, step_ms);
    }

    return ev;
}

/* ------------------------------------------------------------------ */

/* MECHANICS.md 3: start (2,2), length 1, and the snake does not move until a
 * direction is pressed. */
static void test_initial_state(void)
{
    GameState g;
    Cell      h;

    banner("initial state");

    CHECK(game_init_vita(&g, MODE_MEDIUM, 12345u), "init failed");
    CHECK(g.state == STATE_WELCOME, "expected WELCOME, got %d", (int)g.state);
    CHECK(game_length(&g) == 1, "length %d, want 1", game_length(&g));

    h = snake_head(&g.snake);
    CHECK(h.row == 2 && h.col == 2, "head (%d,%d), want (2,2)", h.row, h.col);

    /* Board is live behind the welcome overlay, so food exists already. */
    CHECK(g.has_food, "no food placed at init");

    /* MECHANICS.md 9: 46x24 playable. */
    CHECK(board_interior_cols(&g.board) == 46, "cols %d, want 46",
          board_interior_cols(&g.board));
    CHECK(board_interior_rows(&g.board) == 24, "rows %d, want 24",
          board_interior_rows(&g.board));

    /* Ticking in WELCOME must not move anything. */
    run_steps(&g, 10);
    h = snake_head(&g.snake);
    CHECK(h.row == 2 && h.col == 2, "snake moved while on welcome screen");

    /* Nor in READY. */
    game_action(&g, ACTION_CONFIRM);
    CHECK(g.state == STATE_READY, "expected READY, got %d", (int)g.state);
    run_steps(&g, 10);
    h = snake_head(&g.snake);
    CHECK(h.row == 2 && h.col == 2, "snake moved before first input");

    /* First direction starts it. */
    game_queue_input(&g, DIR_RIGHT);
    CHECK(g.state == STATE_PLAYING, "expected PLAYING, got %d", (int)g.state);
}

/* PLAN.md 4.6: a snake of length N moves N steps and the head lands on the
 * expected cell each step. */
static void test_head_positions(void)
{
    GameState g;
    int       i;

    banner("head lands on the expected cell each step");

    game_init_vita(&g, MODE_MEDIUM, 999u);
    start_game(&g, DIR_RIGHT);

    /* The starting press is itself step 1. */
    {
        Cell h = snake_head(&g.snake);
        CHECK(h.row == 2 && h.col == 3,
              "starting press: head (%d,%d), want (2,3)", h.row, h.col);
    }

    for (i = 2; i <= 20; i++) {
        Cell h;
        run_steps(&g, 1);
        h = snake_head(&g.snake);
        CHECK(h.row == 2 && h.col == 2 + i,
              "step %d: head (%d,%d), want (2,%d)", i, h.row, h.col, 2 + i);
        if (g.state != STATE_PLAYING) {
            break; /* ran into food or a wall; other tests cover those */
        }
    }

    /* Turning down then tracking the column. */
    game_queue_input(&g, DIR_DOWN);
    run_steps(&g, 1);
    CHECK(snake_head(&g.snake).row == 3, "turn down did not advance the row");
}

/* MECHANICS.md 3: growth is exactly +5 per food, so Length goes 1, 6, 11, ... */
static void test_growth(void)
{
    GameState g;
    int       len_before;

    banner("eating grows by exactly 5");

    game_init_vita(&g, MODE_MEDIUM, 4242u);
    game_action(&g, ACTION_CONFIRM);

    /* Put food directly in the snake's path instead of hunting for it. */
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.food.row = 2;
    g.food.col = 5;
    board_set(&g.board, 2, 5, CELL_FOOD);

    len_before = game_length(&g);
    CHECK(len_before == 1, "length %d before eating, want 1", len_before);

    game_queue_input(&g, DIR_RIGHT); /* steps to (2,3) */
    run_steps(&g, 2);                /* (2,4) then (2,5), where the food is */
    CHECK(game_length(&g) == 6, "length %d after one food, want 6",
          game_length(&g));

    /*
     * The 5 grown blocks are not on the board yet, so occupied cells lag the
     * displayed length (MECHANICS.md 3). The snake was a single cell when it
     * ate, and it still is: Length reads 6, but 5 of those blocks are the
     * original's (-1,-1) placeholders and occupy nothing.
     */
    CHECK(snake_occupied(&g.snake) == 1,
          "occupied %d right after eating, want 1", snake_occupied(&g.snake));

    /* After 5 more steps every grown block has materialised. */
    run_steps(&g, 5);
    CHECK(snake_occupied(&g.snake) == 6 || g.state != STATE_PLAYING,
          "occupied %d after growth drained, want 6",
          snake_occupied(&g.snake));
    CHECK(game_length(&g) >= 6, "length shrank after eating");
}

/* PLAN.md 4.6: food never spawns on an occupied cell, over 100k seeded spawns. */
static void test_food_never_on_occupied(void)
{
    Board    b;
    Rng      r;
    Cell     food;
    int      iter;
    int      bad = 0;

    banner("food never spawns on an occupied cell (100k spawns)");

    rng_seed(&r, 0xC0FFEEu);

    for (iter = 0; iter < 100000; iter++) {
        int row, col, blockers, i;

        board_init(&b, BOARD_MAX_COLS, BOARD_MAX_ROWS);

        /* Scatter a pseudo-random body so the free set differs every round. */
        blockers = (int)rng_below(&r, 900u);
        for (i = 0; i < blockers; i++) {
            row = 1 + (int)rng_below(&r, (uint32_t)board_interior_rows(&b));
            col = 1 + (int)rng_below(&r, (uint32_t)board_interior_cols(&b));
            board_set(&b, row, col, CELL_OCCUPIED);
        }

        if (!board_place_food(&b, &r, &food)) {
            continue; /* board full: covered by the win test */
        }

        /* Must be inside the interior and on a cell that was free. */
        if (food.row < 1 || food.row > b.rows - 2 ||
            food.col < 1 || food.col > b.cols - 2) {
            bad++;
            continue;
        }
        if (board_get(&b, food.row, food.col) != CELL_FOOD) {
            bad++;
        }
    }

    CHECK(bad == 0, "%d bad food placements out of 100000", bad);
}

/* PLAN.md 4.6: a full board produces WON, not an infinite food-spawn loop.
 * MECHANICS.md 6.5, deviation 2. */
static void test_board_full_wins(void)
{
    GameState g;
    int       row, col;
    GameEvent ev;

    banner("full board produces WON");

    /* Smallest board with a usable interior: 4x3 grid => 2x1 playable. */
    CHECK(game_init(&g, MODE_MEDIUM, 7u, 4, 3), "small board init failed");
    CHECK(board_interior_cols(&g.board) == 2, "interior cols %d, want 2",
          board_interior_cols(&g.board));

    /* Interior is (1,1) and (1,2). Snake starts at (2,2), which is the wall on
     * a board this small, so use a slightly larger one for the real check. */
    CHECK(game_init(&g, MODE_MEDIUM, 7u, 6, 5), "board init failed");

    game_action(&g, ACTION_CONFIRM);

    /* Fill everything except the head cell and one cell in front of it. */
    for (row = 1; row < g.board.rows - 1; row++) {
        for (col = 1; col < g.board.cols - 1; col++) {
            board_set(&g.board, row, col, CELL_OCCUPIED);
        }
    }
    board_set(&g.board, SNAKE_START_ROW, SNAKE_START_COL, CELL_OCCUPIED);
    board_set(&g.board, SNAKE_START_ROW, SNAKE_START_COL + 1, CELL_FOOD);
    g.food.row  = SNAKE_START_ROW;
    g.food.col  = SNAKE_START_COL + 1;
    g.has_food  = true;

    CHECK(board_count_free(&g.board) == 0,
          "expected no free cells, got %d", board_count_free(&g.board));

    /*
     * Growth must be pending, otherwise the tail vacates its cell on this very
     * step and frees exactly one cell for the next food - which is a legitimate
     * continuation, not a win. Winning requires the board to still be full
     * AFTER the move resolves.
     */
    g.snake.pending = 1;

    ev = game_queue_input(&g, DIR_RIGHT);

    CHECK((ev & EVENT_ATE) != 0, "did not register eating the last food");
    CHECK((ev & EVENT_WON) != 0, "did not emit EVENT_WON");
    CHECK(g.state == STATE_WON, "state %d, want WON", (int)g.state);

    /* Further ticks must be inert. */
    ev = run_steps(&g, 100);
    CHECK(ev == EVENT_NONE, "ticking after a win produced events");
}

/* MECHANICS.md 6.3: walls always kill, at the exact boundary. */
static void test_wall_collision(void)
{
    GameState g;
    int       i;
    Cell      h;

    banner("wall collision kills at the exact boundary");

    game_init_vita(&g, MODE_MEDIUM, 31337u);
    game_action(&g, ACTION_CONFIRM);

    /* Move the food out of the way so this is purely a wall test. */
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;

    /* From row 2 the starting press reaches row 1 (last playable); the next
     * step hits the wall at row 0. */
    game_queue_input(&g, DIR_UP);
    h = snake_head(&g.snake);
    CHECK(h.row == 1, "head row %d after one step, want 1", h.row);
    CHECK(g.state == STATE_PLAYING, "died one cell early");

    run_steps(&g, 1);
    CHECK(g.state == STATE_DEAD, "state %d, want DEAD", (int)g.state);
    h = snake_head(&g.snake);
    CHECK(h.row == 0, "dead head at row %d, want 0 (on the wall)", h.row);

    /* Ticking after death is inert. */
    for (i = 0; i < 50; i++) {
        CHECK(game_tick(&g, 1000u) == EVENT_NONE, "tick after death moved");
    }
}

/* MECHANICS.md 4.3: reversal is rejected against last_move, and the first move
 * of a game bypasses the check. */
static void test_reversal_rejected(void)
{
    GameState g;
    Cell      h;

    banner("180-degree reversal is rejected against last_move");

    game_init_vita(&g, MODE_MEDIUM, 555u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;

    /* last_move starts RIGHT, but first_game_move lets the player open LEFT. */
    game_queue_input(&g, DIR_LEFT);
    h = snake_head(&g.snake);
    CHECK(h.col == 1, "opening LEFT was rejected: col %d, want 1", h.col);

    /* Now moving LEFT; RIGHT is a 180 and must be ignored. */
    game_queue_input(&g, DIR_RIGHT);
    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.col == 0 && g.state == STATE_DEAD,
          "reversal was accepted: head (%d,%d) state %d", h.row, h.col,
          (int)g.state);

    /* A perpendicular turn from the same position is accepted. */
    game_init_vita(&g, MODE_MEDIUM, 556u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;
    game_queue_input(&g, DIR_RIGHT); /* steps to (2,3) */
    game_queue_input(&g, DIR_DOWN);
    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 3, "perpendicular turn rejected: row %d, want 3", h.row);
}

/* MECHANICS.md 4.4: two inputs inside one step apply on consecutive steps
 * rather than the second overwriting the first. */
static void test_premove_queue(void)
{
    GameState g;
    Cell      h;

    banner("queued inputs apply on consecutive steps");

    game_init_vita(&g, MODE_MEDIUM, 4711u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;

    /* Get moving right and settle so current_dir == last_move. */
    game_queue_input(&g, DIR_RIGHT); /* steps to (2,3) */
    run_steps(&g, 1);                /* (2,4) */
    CHECK(g.current_dir == g.last_move,
          "expected a settled direction before queueing");
    h = snake_head(&g.snake);
    CHECK(h.row == 2 && h.col == 4, "setup: head (%d,%d), want (2,4)",
          h.row, h.col);

    /* Two turns inside a single step interval: DOWN then LEFT. */
    game_queue_input(&g, DIR_DOWN);
    CHECK(g.current_dir == DIR_DOWN, "first input not adopted");
    CHECK(g.pre_move == DIR_NONE,
          "first input should not queue: current_dir was still == last_move");

    game_queue_input(&g, DIR_LEFT);
    CHECK(g.pre_move == DIR_LEFT, "second input was not queued, got %d",
          (int)g.pre_move);

    /* Step 1 applies DOWN, step 2 applies LEFT. Neither is collapsed. */
    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 3 && h.col == 4, "step 1: head (%d,%d), want (3,4)",
          h.row, h.col);

    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 3 && h.col == 3, "step 2: head (%d,%d), want (3,3)",
          h.row, h.col);

    CHECK(g.pre_move == DIR_NONE, "premove was not consumed");
}

/* MECHANICS.md 4.4: the premove can never produce a 180, and specifically the
 * branch where the queued direction IS the reverse of last_move. */
static void test_premove_never_reverses(void)
{
    GameState g;
    Cell      before, after;

    banner("a queued reversal still cannot turn the snake back on itself");

    game_init_vita(&g, MODE_MEDIUM, 8080u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;

    game_queue_input(&g, DIR_RIGHT); /* steps to (2,3) */
    run_steps(&g, 1);                /* moving right, settled */

    /* Turn UP (accepted, pending), then press LEFT, which is the reverse of
     * last_move (RIGHT). LEFT is refused as current_dir but IS queued. */
    game_queue_input(&g, DIR_UP);
    game_queue_input(&g, DIR_LEFT);
    CHECK(g.pre_move == DIR_LEFT, "reversal was not queued as a premove");
    CHECK(g.current_dir == DIR_UP,
          "reversal wrongly overwrote current_dir, got %d",
          (int)g.current_dir);

    before = snake_head(&g.snake);
    run_steps(&g, 1); /* applies UP */
    after = snake_head(&g.snake);
    CHECK(after.row == before.row - 1 && after.col == before.col,
          "expected UP, moved (%d,%d) -> (%d,%d)", before.row, before.col,
          after.row, after.col);

    before = after;
    run_steps(&g, 1); /* applies the queued LEFT: legal 90 from UP */
    after = snake_head(&g.snake);
    CHECK(after.col == before.col - 1 && after.row == before.row,
          "expected LEFT, moved (%d,%d) -> (%d,%d)", before.row, before.col,
          after.row, after.col);
}

/*
 * What a THIRD input inside one step interval does (MECHANICS.md 4.4).
 *
 * This was flagged on hardware because the original claim "only the last two
 * matter" is only true for some sequences. The queue is depth 1, so the second press overwrites the first -
 * but set_direction also adopts each legal press as current_dir, so whether a
 * press survives depends on whether it is a reversal of last_move, not on how
 * recent it is. Both shapes are pinned here so the behavior stops being folklore.
 */
static void test_premove_third_input(void)
{
    GameState g;
    Cell      h;

    banner("a third input inside one step");

    /* (a) Three presses whose last one is a legal turn: only the last applies.
     * DOWN is adopted, LEFT is queued but refused as current_dir (it reverses
     * last_move), then UP overwrites both the queue and current_dir. */
    game_init_vita(&g, MODE_MEDIUM, 4711u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;
    game_queue_input(&g, DIR_RIGHT);
    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 2 && h.col == 4, "setup: head (%d,%d), want (2,4)",
          h.row, h.col);

    game_queue_input(&g, DIR_DOWN);
    game_queue_input(&g, DIR_LEFT);
    game_queue_input(&g, DIR_UP);
    CHECK(g.current_dir == DIR_UP, "third input not adopted, got %d",
          (int)g.current_dir);
    CHECK(g.pre_move == DIR_UP, "third input did not overwrite the queue, got %d",
          (int)g.pre_move);

    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 1 && h.col == 4, "step 1: head (%d,%d), want (1,4) - UP",
          h.row, h.col);
    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 0 && h.col == 4, "step 2: head (%d,%d), want (0,4) - UP again",
          h.row, h.col);

    /* (b) Three presses whose last one reverses last_move: the last two apply.
     * DOWN is adopted then overwritten by UP; LEFT cannot be adopted, so it
     * stays queued and lands on the step after. */
    game_init_vita(&g, MODE_MEDIUM, 4711u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;
    game_queue_input(&g, DIR_RIGHT);
    run_steps(&g, 1);

    game_queue_input(&g, DIR_DOWN);
    game_queue_input(&g, DIR_UP);
    game_queue_input(&g, DIR_LEFT);
    CHECK(g.current_dir == DIR_UP, "current_dir %d, want UP", (int)g.current_dir);
    CHECK(g.pre_move == DIR_LEFT, "pre_move %d, want LEFT", (int)g.pre_move);

    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 1 && h.col == 4, "step 1: head (%d,%d), want (1,4) - UP",
          h.row, h.col);
    run_steps(&g, 1);
    h = snake_head(&g.snake);
    CHECK(h.row == 1 && h.col == 3, "step 2: head (%d,%d), want (1,3) - LEFT",
          h.row, h.col);
}

/* MECHANICS.md 6.4: moving into the cell the tail vacates this step is legal. */
static void test_tail_vacate_is_legal(void)
{
    GameState g;
    int       i;

    banner("following the vacating tail is legal, not death");

    game_init_vita(&g, MODE_MEDIUM, 24680u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;

    /*
     * A body of exactly 4 driving a 2x2 cycle is the minimal case: on every
     * step the head enters the cell the tail is leaving that same step. The
     * body must equal the cycle length - a longer snake genuinely does not fit
     * and would die for real reasons.
     */
    g.snake.pending = 3;

    game_queue_input(&g, DIR_RIGHT); /* (2,3) */
    run_steps(&g, 2);                /* (2,4) (2,5) */
    CHECK(g.state == STATE_PLAYING, "died while growing");
    CHECK(snake_occupied(&g.snake) == 4, "occupied %d, want 4",
          snake_occupied(&g.snake));

    /* Fold the straight body into a 2x2 square: DOWN then LEFT. */
    game_queue_input(&g, DIR_DOWN);
    run_steps(&g, 1); /* (3,5) */
    game_queue_input(&g, DIR_LEFT);
    run_steps(&g, 1); /* (3,4); body is now (2,4)(2,5)(3,5)(3,4) */
    CHECK(g.state == STATE_PLAYING, "died folding into the square");

    /*
     * Now cycle. Each step the destination is the current tail cell, which is
     * vacated earlier in the same step. If the vacate happened after the
     * destination test, this would die on the very first iteration.
     */
    for (i = 0; i < 24 && g.state == STATE_PLAYING; i++) {
        static const Direction loop[4] = {
            DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT
        };
        game_queue_input(&g, loop[i % 4]);
        run_steps(&g, 1);
    }

    CHECK(g.state == STATE_PLAYING,
          "died chasing own tail after %d loop steps", i);
    CHECK(snake_occupied(&g.snake) == 4, "body changed size while looping");
}

/* Self-collision proper: running into the middle of the body is death. */
static void test_self_collision_kills(void)
{
    GameState g;

    banner("running into the body is death");

    game_init_vita(&g, MODE_MEDIUM, 13579u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;

    /*
     * Growth stays pending throughout, so the tail never moves off (2,2). The
     * path runs right, down, back left and then up into (2,2) - a cell still
     * held by the body, so this is a genuine self-collision rather than the
     * legal tail-chase above.
     */
    g.snake.pending = 20;

    game_queue_input(&g, DIR_RIGHT); /* (2,3) */
    run_steps(&g, 2);                /* (2,4) (2,5) */
    game_queue_input(&g, DIR_DOWN);
    run_steps(&g, 1);                /* (3,5) */
    game_queue_input(&g, DIR_LEFT);
    run_steps(&g, 3);                /* (3,4) (3,3) (3,2) */
    CHECK(g.state == STATE_PLAYING, "died before reaching the body");

    game_queue_input(&g, DIR_UP);
    run_steps(&g, 1);                /* (2,2): occupied by the body */

    CHECK(g.state == STATE_DEAD, "state %d, want DEAD after closing a loop",
          (int)g.state);
}

/* PLAN.md 4.6: sub-interval elapsed produces no move; a large elapsed produces
 * the right number of moves and does not skip a collision. */
static void test_timing_accumulator(void)
{
    GameState g;
    Cell      h;
    uint32_t  step_ms;

    banner("timing: sub-step does nothing, large elapsed does not skip");

    game_init_vita(&g, MODE_MEDIUM, 2244u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;
    step_ms = mode_get(g.mode)->step_ms;

    /* The starting press takes step one and leaves the accumulator at zero. */
    game_queue_input(&g, DIR_RIGHT);
    h = snake_head(&g.snake);
    CHECK(h.col == 3, "starting press: col %d, want 3", h.col);
    CHECK(g.accum_ms == 0u, "accumulator %u after the starting press, want 0",
          g.accum_ms);

    /* Sub-interval ticks accumulate but must not move. */
    game_tick(&g, step_ms - 1u);
    h = snake_head(&g.snake);
    CHECK(h.col == 3, "moved on a sub-interval tick");

    /* One more ms crosses the boundary: exactly one move. */
    game_tick(&g, 1u);
    h = snake_head(&g.snake);
    CHECK(h.col == 4, "boundary tick: col %d, want 4", h.col);

    /* A big elapsed produces exactly floor(elapsed/step) moves. */
    game_tick(&g, step_ms * 10u);
    h = snake_head(&g.snake);
    CHECK(h.col == 14, "10 steps at once: col %d, want 14", h.col);

    /* A very large elapsed must stop at the wall, not run through it. The
     * snake is at col 14 on a 46-wide interior, so the wall is at col 47. */
    game_tick(&g, step_ms * 10000u);
    CHECK(g.state == STATE_DEAD, "huge elapsed did not stop at the wall");
    h = snake_head(&g.snake);
    CHECK(h.col == 47, "stopped at col %d, want the wall at 47", h.col);
}

/* PLAN.md 4.6: PAUSED consumes elapsed_ms without advancing the snake. */
static void test_pause(void)
{
    GameState g;
    Cell      before, after;

    banner("pause consumes time without moving");

    game_init_vita(&g, MODE_MEDIUM, 6543u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.has_food = false;

    game_queue_input(&g, DIR_RIGHT);
    run_steps(&g, 2);

    before = snake_head(&g.snake);
    game_action(&g, ACTION_PAUSE);
    CHECK(g.state == STATE_PAUSED, "state %d, want PAUSED", (int)g.state);

    run_steps(&g, 100);
    after = snake_head(&g.snake);
    CHECK(before.row == after.row && before.col == after.col,
          "snake moved while paused: (%d,%d) -> (%d,%d)", before.row,
          before.col, after.row, after.col);

    /* Input is ignored while paused (MECHANICS.md 4.5). */
    game_queue_input(&g, DIR_DOWN);
    CHECK(g.current_dir != DIR_DOWN, "input was accepted while paused");

    game_action(&g, ACTION_PAUSE);
    CHECK(g.state == STATE_PLAYING, "state %d, want PLAYING after unpause",
          (int)g.state);
    run_steps(&g, 1);
    after = snake_head(&g.snake);
    CHECK(after.col == before.col + 1, "did not resume moving");
}

/* MECHANICS.md 6.6: a pause taken before the first input returns to READY. */
static void test_pause_before_first_input(void)
{
    GameState g;

    banner("pausing before the first input resumes to READY");

    game_init_vita(&g, MODE_MEDIUM, 77u);
    game_action(&g, ACTION_CONFIRM);
    CHECK(g.state == STATE_READY, "expected READY");

    game_action(&g, ACTION_PAUSE);
    CHECK(g.state == STATE_PAUSED, "expected PAUSED");

    game_action(&g, ACTION_PAUSE);
    CHECK(g.state == STATE_READY,
          "state %d, want READY - resuming into PLAYING would start the snake "
          "with no direction pressed", (int)g.state);

    run_steps(&g, 20);
    CHECK(snake_head(&g.snake).col == 2, "snake moved after resuming to READY");
}

/* State machine: illegal transitions are unreachable (PLAN.md 4.3). */
static void test_state_transitions(void)
{
    GameState g;

    banner("illegal state transitions are ignored");

    game_init_vita(&g, MODE_MEDIUM, 8u);

    /* Pause and back do nothing on the welcome screen. */
    game_action(&g, ACTION_PAUSE);
    CHECK(g.state == STATE_WELCOME, "pause changed the welcome state");
    game_action(&g, ACTION_BACK);
    CHECK(g.state == STATE_WELCOME, "back changed the welcome state");

    /* Direction input does nothing on the welcome screen. */
    game_queue_input(&g, DIR_DOWN);
    CHECK(g.state == STATE_WELCOME, "direction input started the game early");

    /* DIR_NONE is rejected outright (deviation 4). */
    game_action(&g, ACTION_CONFIRM);
    game_queue_input(&g, DIR_RIGHT);
    run_steps(&g, 1);
    {
        Direction cur = g.current_dir, last = g.last_move;
        game_queue_input(&g, DIR_NONE);
        CHECK(g.current_dir == cur && g.last_move == last,
              "DIR_NONE corrupted the direction state");
    }

    /* Death -> back -> welcome, and death -> confirm -> ready. */
    game_init_vita(&g, MODE_MEDIUM, 9u);
    game_action(&g, ACTION_CONFIRM);
    game_queue_input(&g, DIR_UP);
    run_steps(&g, 5);
    CHECK(g.state == STATE_DEAD, "expected DEAD after running up into a wall");

    game_action(&g, ACTION_BACK);
    CHECK(g.state == STATE_WELCOME, "back from DEAD did not reach WELCOME");

    game_action(&g, ACTION_CONFIRM);
    CHECK(g.state == STATE_READY, "confirm from WELCOME did not reach READY");
    CHECK(game_length(&g) == 1, "length %d after restart, want 1",
          game_length(&g));
    CHECK(snake_head(&g.snake).row == 2 && snake_head(&g.snake).col == 2,
          "snake not returned to the start cell");
}

/* MECHANICS.md 7: Length counts blocks, highscore is global and persists
 * across modes. */
static void test_scoring(void)
{
    GameState g;
    GameEvent ev;

    banner("scoring and highscore");

    game_init_vita(&g, MODE_MEDIUM, 321u);
    game_action(&g, ACTION_CONFIRM);

    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.food.row = 2;
    g.food.col = 4;
    board_set(&g.board, 2, 4, CELL_FOOD);

    ev  = game_queue_input(&g, DIR_RIGHT); /* (2,3) */
    ev |= run_steps(&g, 1);                /* (2,4), the food */

    CHECK((ev & EVENT_ATE) != 0, "no EVENT_ATE");
    CHECK((ev & EVENT_HIGHSCORE) != 0, "no EVENT_HIGHSCORE on a fresh board");
    CHECK(g.highscore == 6, "highscore %d, want 6", g.highscore);

    /* A pre-loaded highscore is not beaten by a shorter run. */
    game_init_vita(&g, MODE_HARD, 322u);
    game_set_highscore(&g, 500);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.food.row = 2;
    g.food.col = 4;
    board_set(&g.board, 2, 4, CELL_FOOD);
    ev  = game_queue_input(&g, DIR_RIGHT);
    ev |= run_steps(&g, 1);
    CHECK((ev & EVENT_HIGHSCORE) == 0, "beat a highscore of 500 with length 6");
    CHECK(g.highscore == 500, "highscore %d, want 500 unchanged", g.highscore);
}

/* MECHANICS.md 4.1: the three shipped modes have distinct intervals and the
 * step rate does not drift with length. */
static void test_modes(void)
{
    GameState g;
    int       i;

    banner("mode table");

    CHECK(mode_count() == 3, "mode_count %d, want 3", mode_count());
    CHECK(mode_get(MODE_EASY)->step_ms == 100u, "easy %u, want 100",
          mode_get(MODE_EASY)->step_ms);
    CHECK(mode_get(MODE_MEDIUM)->step_ms == 75u, "medium %u, want 75",
          mode_get(MODE_MEDIUM)->step_ms);
    CHECK(mode_get(MODE_HARD)->step_ms == 50u, "hard %u, want 50",
          mode_get(MODE_HARD)->step_ms);

    for (i = 0; i < mode_count(); i++) {
        CHECK(mode_get(i)->walls_kill, "mode %d does not kill on walls", i);
    }

    /* Out-of-range indices clamp rather than reading past the table. */
    CHECK(mode_get(-5) == mode_get(0), "negative index did not clamp");
    CHECK(mode_get(99) == mode_get(mode_count() - 1),
          "large index did not clamp");

    /* No speed ramp: after eating, the interval is unchanged. */
    game_init_vita(&g, MODE_EASY, 111u);
    game_action(&g, ACTION_CONFIRM);
    board_set(&g.board, g.food.row, g.food.col, CELL_EMPTY);
    g.food.row = 2;
    g.food.col = 4;
    board_set(&g.board, 2, 4, CELL_FOOD);
    game_queue_input(&g, DIR_RIGHT); /* (2,3) */
    run_steps(&g, 1);                /* (2,4), the food */
    CHECK(game_length(&g) == 6, "setup: did not eat");

    /* 99ms must still not be enough for a step at Easy. */
    {
        Cell before = snake_head(&g.snake);
        game_tick(&g, 99u);
        CHECK(snake_head(&g.snake).col == before.col,
              "Easy stepped in under 100ms after growing");
        game_tick(&g, 1u);
        CHECK(snake_head(&g.snake).col == before.col + 1,
              "Easy did not step at exactly 100ms");
    }
}

/*
 * Square cycles difficulty, and only from the welcome screen (game.h). The
 * assertions are on the resulting step interval rather than on g.mode alone, so
 * a cycle that updated the field without changing how fast the snake moves
 * would still fail.
 */
static void test_cycle_mode(void)
{
    GameState g;
    int       i;

    banner("difficulty cycling");

    game_init_vita(&g, MODE_EASY, 12u);
    CHECK(g.state == STATE_WELCOME, "setup: not on the welcome screen");

    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_MEDIUM, "easy did not cycle to medium (got %d)",
          g.mode);
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_HARD, "medium did not cycle to hard (got %d)", g.mode);
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_EASY, "hard did not wrap to easy (got %d)", g.mode);

    /* Every mode in the table is reachable, whatever the table holds: adding a
     * mode must stay a data change (PLAN.md 0.6). */
    {
        int seen[16];

        for (i = 0; i < mode_count() && i < 16; i++) {
            seen[i] = 0;
        }
        for (i = 0; i < mode_count(); i++) {
            CHECK(g.mode >= 0 && g.mode < mode_count(), "mode %d off the table",
                  g.mode);
            if (g.mode >= 0 && g.mode < 16) {
                seen[g.mode]++;
            }
            game_action(&g, ACTION_CYCLE_MODE);
        }
        for (i = 0; i < mode_count() && i < 16; i++) {
            CHECK(seen[i] == 1, "mode %d visited %d times in one full cycle", i,
                  seen[i]);
        }
    }

    /* A corrupt save can hand the core a mode outside the table. Cycling from
     * there lands back on the first mode rather than staying out of range. */
    g.mode = -7;
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_EASY, "cycling from an off-table mode gave %d", g.mode);

    /* The speed the snake actually moves at follows the cycled mode. */
    game_init_vita(&g, MODE_EASY, 13u);
    game_action(&g, ACTION_CYCLE_MODE); /* -> Medium, 75ms */
    game_action(&g, ACTION_CONFIRM);
    game_queue_input(&g, DIR_RIGHT);
    {
        Cell before = snake_head(&g.snake);

        game_tick(&g, 74u);
        CHECK(snake_head(&g.snake).col == before.col,
              "stepped before 75ms, so Easy's 100ms was still in force");
        game_tick(&g, 1u);
        CHECK(snake_head(&g.snake).col == before.col + 1,
              "did not step at 75ms after cycling to Medium");
    }

    /* Ignored everywhere else: changing the interval underneath a running snake
     * is exactly what the original's disabled-during-play dropdown prevents. */
    game_init_vita(&g, MODE_MEDIUM, 14u);
    game_action(&g, ACTION_CONFIRM); /* READY */
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_MEDIUM, "READY accepted a difficulty change");

    game_queue_input(&g, DIR_RIGHT); /* PLAYING */
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_MEDIUM, "PLAYING accepted a difficulty change");

    game_action(&g, ACTION_PAUSE); /* PAUSED */
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_MEDIUM, "PAUSED accepted a difficulty change");
    game_action(&g, ACTION_PAUSE);

    /* Death screen: the player is one Circle away from the welcome screen,
     * where it is allowed. */
    g.state = STATE_DEAD;
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_MEDIUM, "DEAD accepted a difficulty change");
    game_action(&g, ACTION_BACK);
    CHECK(g.state == STATE_WELCOME, "BACK did not return to the welcome screen");
    game_action(&g, ACTION_CYCLE_MODE);
    CHECK(g.mode == MODE_HARD, "welcome screen refused a difficulty change");

    /* Cycling does not disturb the board behind the overlay. */
    {
        GameState a, b;

        game_init_vita(&a, MODE_MEDIUM, 999u);
        game_init_vita(&b, MODE_MEDIUM, 999u);
        game_action(&b, ACTION_CYCLE_MODE);
        b.mode = a.mode; /* the one field that is meant to differ */
        CHECK(game_state_hash(&a) == game_state_hash(&b),
              "cycling changed something other than the mode");
    }

    /* The mode is part of the state hash, so a replay cannot silently drift
     * onto a different difficulty. */
    {
        GameState a, b;

        game_init_vita(&a, MODE_MEDIUM, 21u);
        game_init_vita(&b, MODE_MEDIUM, 21u);
        game_action(&b, ACTION_CYCLE_MODE);
        CHECK(game_state_hash(&a) != game_state_hash(&b),
              "the mode is not covered by game_state_hash");
    }
}

/* PLAN.md 4.6: identical seed plus identical input script produces an identical
 * final state. memcmp is meaningful because game_init memsets the struct. */
static void test_determinism(void)
{
    GameState a, b;
    int       i;

    banner("determinism: same seed and script produce identical state");

    game_init_vita(&a, MODE_MEDIUM, 0xABCDEFu);
    game_init_vita(&b, MODE_MEDIUM, 0xABCDEFu);

    game_action(&a, ACTION_CONFIRM);
    game_action(&b, ACTION_CONFIRM);
    game_queue_input(&a, DIR_RIGHT);
    game_queue_input(&b, DIR_RIGHT);

    for (i = 0; i < 4000; i++) {
        static const Direction script[8] = {
            DIR_RIGHT, DIR_DOWN, DIR_DOWN, DIR_LEFT,
            DIR_LEFT,  DIR_UP,   DIR_RIGHT, DIR_DOWN
        };
        if (i % 7 == 0) {
            game_queue_input(&a, script[(i / 7) % 8]);
            game_queue_input(&b, script[(i / 7) % 8]);
        }
        game_tick(&a, 16u);
        game_tick(&b, 16u);
    }

    CHECK(memcmp(&a, &b, sizeof a) == 0,
          "identical runs diverged (memcmp over %zu bytes)", sizeof a);
    CHECK(game_state_hash(&a) == game_state_hash(&b), "hashes diverged");

    /* And a different seed must actually change something, otherwise the test
     * above would pass trivially. */
    {
        GameState c;
        game_init_vita(&c, MODE_MEDIUM, 0x123456u);
        CHECK(game_state_hash(&c) != game_state_hash(&a),
              "a different seed produced an identical state");
    }
}

/* The RNG itself: uniform enough that food does not favour one region. */
static void test_rng(void)
{
    Rng      r;
    int      buckets[8];
    int      i;
    uint32_t v;

    banner("rng");

    memset(buckets, 0, sizeof buckets);
    rng_seed(&r, 1u);

    for (i = 0; i < 80000; i++) {
        v = rng_below(&r, 8u);
        CHECK(v < 8u, "rng_below(8) returned %u", v);
        buckets[v]++;
    }

    for (i = 0; i < 8; i++) {
        /* 10000 expected; allow a generous band so this never flakes. */
        CHECK(buckets[i] > 9000 && buckets[i] < 11000,
              "bucket %d got %d, expected ~10000", i, buckets[i]);
    }

    /* A zero seed must not collapse the generator to all zeroes. */
    rng_seed(&r, 0u);
    CHECK(rng_next(&r) != 0u, "zero seed produced zero output");

    CHECK(rng_below(&r, 0u) == 0u, "rng_below(0) must be 0, not a division trap");
    CHECK(rng_below(&r, 1u) == 0u, "rng_below(1) must always be 0");
}

int main(void)
{
    printf("core tests\n\n");

    test_initial_state();
    test_head_positions();
    test_growth();
    test_food_never_on_occupied();
    test_board_full_wins();
    test_wall_collision();
    test_reversal_rejected();
    test_premove_queue();
    test_premove_never_reverses();
    test_premove_third_input();
    test_tail_vacate_is_legal();
    test_self_collision_kills();
    test_timing_accumulator();
    test_pause();
    test_pause_before_first_input();
    test_state_transitions();
    test_scoring();
    test_modes();
    test_cycle_mode();
    test_determinism();
    test_rng();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
