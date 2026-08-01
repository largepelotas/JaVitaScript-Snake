/*
 * Shared types for the pure game core.
 *
 * PLAN.md 4.1: enums and structs only, no functions. Nothing in src/core/ may
 * include SDL, platform headers, or anything that does I/O or reads a clock.
 *
 * Values here are specified by docs/MECHANICS.md; section references below
 * point at the justification. Do not change a constant without changing
 * MECHANICS.md first (PLAN.md 3.4).
 */
#ifndef SNAKE_TYPES_H
#define SNAKE_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* ---- Board geometry (MECHANICS.md 9) ---------------------------------- */

/* Including the wall ring. Playable interior is (COLS-2) x (ROWS-2) = 46x24. */
#define BOARD_MAX_COLS 48
#define BOARD_MAX_ROWS 26

/* Every cell the snake can ever occupy: 46 * 24 = 1104. */
#define SNAKE_MAX_CELLS ((BOARD_MAX_COLS - 2) * (BOARD_MAX_ROWS - 2))

/* MECHANICS.md 3: growth is 5 blocks per food, start is (2,2), length is 1. */
#define SNAKE_GROWTH 5
#define SNAKE_START_ROW 2
#define SNAKE_START_COL 2
#define SNAKE_START_LEN 1

/* Cell values, mirroring the original exactly (MECHANICS.md 2, 6.2).
 * CELL_FOOD must stay negative: the original tests "> 0" for death before it
 * tests for food, so a non-negative food value would read as a collision. */
#define CELL_EMPTY 0
#define CELL_OCCUPIED 1
#define CELL_FOOD (-1)

/* ---- Directions (MECHANICS.md 1) -------------------------------------- */

/*
 * These numeric values are load-bearing, not cosmetic. Opposite directions
 * differ by exactly 2, which is what makes the reversal test
 * abs(dir - last_move) != 2 work (MECHANICS.md 4.3). Renumbering silently
 * breaks collision-free turning.
 *
 *        0
 *      3   1
 *        2
 */
typedef enum {
    DIR_NONE  = -1,
    DIR_UP    = 0,
    DIR_RIGHT = 1,
    DIR_DOWN  = 2,
    DIR_LEFT  = 3
} Direction;

/* ---- State machine (MECHANICS.md 6.6) --------------------------------- */

typedef enum {
    STATE_WELCOME = 0, /* welcome overlay; board not yet set up  */
    STATE_READY,       /* snake and food placed, nothing moving  */
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_DEAD,
    STATE_WON
} GameStateId;

/* Non-direction inputs the shell can deliver. Kept separate from Direction so
 * a button press can never be mistaken for a turn (MECHANICS.md 4.6, which is
 * the bug in the original we deliberately do not port). */
typedef enum {
    ACTION_CONFIRM = 0, /* Cross: start / play again          */
    ACTION_PAUSE,       /* Start: toggle pause                */
    ACTION_BACK,        /* Circle: back to welcome from end   */
    ACTION_CYCLE_MODE   /* Square: next difficulty, welcome only */
} GameAction;

/* ---- Events returned by game_tick ------------------------------------- */

typedef enum {
    EVENT_NONE       = 0,
    EVENT_MOVED      = 1 << 0, /* the snake advanced at least one step */
    EVENT_ATE        = 1 << 1,
    EVENT_DIED       = 1 << 2,
    EVENT_WON        = 1 << 3,
    EVENT_HIGHSCORE  = 1 << 4  /* highscore was beaten during this tick */
} GameEvent;

/* ---- Aggregates -------------------------------------------------------- */

typedef struct {
    int16_t row;
    int16_t col;
} Cell;

typedef struct {
    uint32_t s;
} Rng;

typedef struct {
    int32_t cols; /* including the wall ring */
    int32_t rows;
    int8_t  cell[BOARD_MAX_ROWS][BOARD_MAX_COLS];
} Board;

/*
 * Body storage is a ring buffer of the cells the snake actually occupies.
 *
 * The original splices 5 fresh linked-list blocks in at the tail on eating,
 * each parked at (-1,-1) until it cycles round to become the head
 * (MECHANICS.md 3). Those blocks count toward the displayed Length but occupy
 * no grid cell. `pending` is that count; the visible effect is that the tail
 * does not advance for 5 steps after eating.
 *
 * Displayed length is count + pending.
 */
typedef struct {
    Cell    cells[SNAKE_MAX_CELLS];
    int32_t head;    /* ring index of the head cell */
    int32_t count;   /* occupied cells              */
    int32_t pending; /* grown-but-not-yet-materialised blocks */
} Snake;

typedef struct {
    const char *name;
    uint32_t    step_ms;
    bool        walls_kill;
} SnakeMode;

/*
 * The whole game. No pointers, no allocation: this struct is memcmp-able and
 * memset to zero by game_init, which is what makes the determinism test in
 * PLAN.md 4.6 meaningful.
 */
typedef struct {
    GameStateId state;
    /* Which state a pause was entered from. The original allows pausing before
     * the first input, and unpausing must return there rather than to PLAYING
     * (MECHANICS.md 6.6) - resuming into PLAYING would start the snake moving
     * with no direction ever having been pressed. */
    GameStateId resume_state;
    Board       board;
    Snake       snake;
    Rng         rng;
    int32_t     mode;

    /* Input model (MECHANICS.md 4.3, 4.4). Names match the original's. */
    Direction   current_dir;
    Direction   last_move;
    Direction   pre_move;
    bool        first_game_move;

    /* Timing. game_tick is a pure function of (state, elapsed_ms); nothing in
     * here ever reads a clock. */
    uint32_t    accum_ms;

    Cell        food;
    bool        has_food;

    int32_t     highscore;
} GameState;

#endif /* SNAKE_TYPES_H */
