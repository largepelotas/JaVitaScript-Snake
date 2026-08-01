#include "game.h"

#include <string.h>

/* Indexed by Direction 0..3 (MECHANICS.md 1). Row increases downward. */
static const int ROW_SHIFT[4] = { -1, 0, 1, 0 };
static const int COL_SHIFT[4] = { 0, 1, 0, -1 };

static int idiff_abs(int a, int b)
{
    int d = a - b;
    return d < 0 ? -d : d;
}

static GameEvent step(GameState *g);

/*
 * Rebuilds the board for a new game: grid, snake at the start cell, first food.
 *
 * Deviation 5 (MECHANICS.md 10, 11.2): the head's cell is marked occupied
 * BEFORE the food is placed. The original places food first and only then marks
 * the head, so with p = 1/1104 the food lands under the head, is overwritten,
 * and the board becomes unwinnable.
 */
static void reset_board(GameState *g)
{
    board_init(&g->board, g->board.cols, g->board.rows);

    snake_init(&g->snake, SNAKE_START_ROW, SNAKE_START_COL);
    board_set(&g->board, SNAKE_START_ROW, SNAKE_START_COL, CELL_OCCUPIED);

    g->has_food = board_place_food(&g->board, &g->rng, &g->food);

    /*
     * The original's reset() and rebirth() do not clear lastMove or
     * currentDirection, so they carry over from the previous game. Clearing
     * them here is not observable: first_game_move makes the next input bypass
     * the reversal test either way, and any premove recorded from the stale
     * pair collapses to the same direction on the first step (MECHANICS.md 4.4).
     */
    g->current_dir     = DIR_NONE;
    g->last_move       = DIR_RIGHT; /* snake.js:164 */
    g->pre_move        = DIR_NONE;
    g->first_game_move = true;
    g->accum_ms        = 0u;
}

bool game_init(GameState *g, int mode, uint32_t seed, int cols, int rows)
{
    memset(g, 0, sizeof *g);

    if (!board_init(&g->board, cols, rows)) {
        return false;
    }

    rng_seed(&g->rng, seed);
    g->mode      = mode;
    g->highscore = 0;

    reset_board(g);
    g->state = STATE_WELCOME;

    return true;
}

bool game_init_vita(GameState *g, int mode, uint32_t seed)
{
    return game_init(g, mode, seed, BOARD_MAX_COLS, BOARD_MAX_ROWS);
}

int game_length(const GameState *g)
{
    return snake_length(&g->snake);
}

void game_set_highscore(GameState *g, int value)
{
    g->highscore = value < 0 ? 0 : value;
}

/* The original's setDirection, snake.js:295-305. */
static void set_direction(GameState *g, Direction dir)
{
    if (g->current_dir != g->last_move) {
        /* A turn is already pending for the next step, so this one queues
         * behind it. Depth is 1: a second premove overwrites the first. */
        g->pre_move = dir;
    }

    if (idiff_abs((int)dir, (int)g->last_move) != 2 || g->first_game_move) {
        g->current_dir     = dir;
        g->first_game_move = false;
    }
}

GameEvent game_queue_input(GameState *g, Direction dir)
{
    if (dir == DIR_NONE) {
        return EVENT_NONE; /* deviation 4 */
    }

    if (g->state == STATE_READY) {
        /* The original's rebirth() at snake.js:1335, immediately before the
         * first handleArrowKeys and the synchronous first go(). */
        g->first_game_move = true;
        g->pre_move        = DIR_NONE;
        g->state           = STATE_PLAYING;

        set_direction(g, dir);

        /*
         * The original calls go() synchronously from the keydown handler
         * (snake.js:1338), so the starting press moves the snake immediately
         * rather than after one interval. Do the same and start the
         * accumulator from zero, so the second step lands a full interval
         * later.
         *
         * Priming the accumulator instead would double-count: game_tick adds
         * its own elapsed_ms on top and a coarse tick would fire two steps at
         * once.
         */
        g->accum_ms = 0u;
        return step(g);
    }

    if (g->state != STATE_PLAYING) {
        /* Dead, won, on the welcome screen, or paused. The original ignores
         * input while paused because premoveOnPause is false
         * (MECHANICS.md 4.5). */
        return EVENT_NONE;
    }

    set_direction(g, dir);
    return EVENT_NONE;
}

void game_action(GameState *g, GameAction action)
{
    switch (action) {
    case ACTION_CONFIRM:
        if (g->state == STATE_WELCOME || g->state == STATE_DEAD ||
            g->state == STATE_WON) {
            reset_board(g);
            g->state = STATE_READY;
        }
        break;

    case ACTION_PAUSE:
        /* The original allows pause whenever the board is not showing a dialog,
         * which includes the pre-first-input READY state (snake.js:1297-1300).
         * Unpausing returns to whichever state it came from - resuming a READY
         * pause into PLAYING would start the snake with no direction input. */
        if (g->state == STATE_PLAYING || g->state == STATE_READY) {
            g->resume_state = g->state;
            g->state        = STATE_PAUSED;
        } else if (g->state == STATE_PAUSED) {
            g->state = g->resume_state;
        }
        break;

    case ACTION_BACK:
        if (g->state == STATE_DEAD || g->state == STATE_WON) {
            reset_board(g);
            g->state = STATE_WELCOME;
        }
        break;

    case ACTION_CYCLE_MODE:
        /* Welcome screen only - see game.h. The board is not rebuilt: the mode
         * decides the step interval and nothing else (§4.2: no speed ramp in
         * the three shipped modes), and the welcome board is already placed. */
        if (g->state == STATE_WELCOME) {
            int32_t next = g->mode + 1;

            /* Written as a range test rather than a modulo so that a mode
             * outside the table - a corrupt save file is the way that happens -
             * lands back on Easy instead of staying negative forever. */
            if (next < 0 || next >= (int32_t)mode_count()) {
                next = 0;
            }
            g->mode = next;
        }
        break;
    }
}

/*
 * One step. Mirrors snake.js:343-418 in order, because the order is what
 * decides whether following your own tail is legal.
 */
static GameEvent step(GameState *g)
{
    GameEvent ev = EVENT_NONE;
    Cell      head, vacated;
    int       nrow, ncol;
    int8_t    dest;

    head = snake_head(&g->snake);

    /* 1. Vacate the tail BEFORE the destination is read. This is precisely why
     *    moving into the cell the tail is leaving is legal (MECHANICS.md 6.4).
     *    Returns false while growth is pending, in which case the tail stays. */
    if (snake_pop_tail(&g->snake, &vacated)) {
        board_set(&g->board, vacated.row, vacated.col, CELL_EMPTY);
    }

    /* 2. Adopt the queued direction and consume the premove (snake.js:374-381).
     *    The move uses last_move, which was just assigned from current_dir. */
    if (g->current_dir != DIR_NONE) {
        g->last_move = g->current_dir;
        if (g->pre_move != DIR_NONE) {
            g->current_dir = g->pre_move;
            g->pre_move    = DIR_NONE;
        }
    }

    /* 3. Destination, from the OLD head plus the shift. */
    nrow = head.row + ROW_SHIFT[g->last_move];
    ncol = head.col + COL_SHIFT[g->last_move];

    dest = board_get(&g->board, nrow, ncol);

    /* 4. Branch. The "> 0" death test precedes the food test, which is only
     *    safe because CELL_FOOD is negative (snake.js:399-417). */
    if (dest == CELL_EMPTY) {
        snake_push_head(&g->snake, nrow, ncol);
        board_set(&g->board, nrow, ncol, CELL_OCCUPIED);
        ev |= EVENT_MOVED;
    } else if (dest > 0) {
        /* The original moves the head block onto the colliding cell and only
         * then dies, so the dead head renders on top of the wall or body it
         * hit. Reproduce that, but leave the grid alone - the game is over. */
        snake_push_head(&g->snake, nrow, ncol);
        g->state = STATE_DEAD;
        ev |= EVENT_MOVED | EVENT_DIED;
    } else {
        /* Food. */
        snake_push_head(&g->snake, nrow, ncol);
        board_set(&g->board, nrow, ncol, CELL_OCCUPIED);
        snake_grow(&g->snake);
        ev |= EVENT_MOVED | EVENT_ATE;

        if (game_length(g) > g->highscore) {
            g->highscore = game_length(g);
            ev |= EVENT_HIGHSCORE;
        }

        /* Deviation 2: a free-cell count instead of the original's 20000
         * failed samples. Same outcome, deterministic, cannot hang. */
        g->has_food = board_place_food(&g->board, &g->rng, &g->food);
        if (!g->has_food) {
            g->state = STATE_WON;
            ev |= EVENT_WON;
        }
    }

    return ev;
}

GameEvent game_tick(GameState *g, uint32_t elapsed_ms)
{
    GameEvent ev      = EVENT_NONE;
    uint32_t  step_ms = mode_get(g->mode)->step_ms;

    if (g->state == STATE_PAUSED) {
        /* Time passes and is consumed, but the snake does not advance
         * (PLAN.md 4.6). The original keeps rescheduling its step timer while
         * paused and simply does not move (snake.js:348-353), so draining the
         * accumulator rather than freezing it keeps the phase identical. */
        g->accum_ms += elapsed_ms;
        while (g->accum_ms >= step_ms) {
            g->accum_ms -= step_ms;
        }
        return EVENT_NONE;
    }

    if (g->state != STATE_PLAYING) {
        return EVENT_NONE;
    }

    g->accum_ms += elapsed_ms;

    while (g->accum_ms >= step_ms) {
        g->accum_ms -= step_ms;
        ev |= step(g);
        if (g->state != STATE_PLAYING) {
            break; /* never step past a death or a win */
        }
    }

    return ev;
}

/* FNV-1a over named fields. */
static void hash_bytes(uint32_t *h, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    size_t               i;

    for (i = 0; i < n; i++) {
        *h ^= (uint32_t)b[i];
        *h *= 16777619u;
    }
}

static void hash_i32(uint32_t *h, int32_t v)
{
    hash_bytes(h, &v, sizeof v);
}

uint32_t game_state_hash(const GameState *g)
{
    uint32_t h = 2166136261u;
    int      i, row, col;

    hash_i32(&h, (int32_t)g->state);
    hash_i32(&h, (int32_t)g->resume_state);
    hash_i32(&h, g->mode);
    hash_i32(&h, (int32_t)g->rng.s);
    hash_i32(&h, (int32_t)g->current_dir);
    hash_i32(&h, (int32_t)g->last_move);
    hash_i32(&h, (int32_t)g->pre_move);
    hash_i32(&h, g->first_game_move ? 1 : 0);
    hash_i32(&h, (int32_t)g->accum_ms);
    hash_i32(&h, g->has_food ? 1 : 0);
    hash_i32(&h, g->food.row);
    hash_i32(&h, g->food.col);
    hash_i32(&h, g->highscore);

    hash_i32(&h, g->snake.count);
    hash_i32(&h, g->snake.pending);
    for (i = 0; i < g->snake.count; i++) {
        Cell c = snake_cell_at(&g->snake, i);
        hash_i32(&h, c.row);
        hash_i32(&h, c.col);
    }

    hash_i32(&h, g->board.cols);
    hash_i32(&h, g->board.rows);
    for (row = 0; row < g->board.rows; row++) {
        for (col = 0; col < g->board.cols; col++) {
            hash_i32(&h, g->board.cell[row][col]);
        }
    }

    return h;
}
