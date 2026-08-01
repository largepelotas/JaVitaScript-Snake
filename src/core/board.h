/*
 * The grid: cell occupancy, the wall ring, and free-cell enumeration.
 *
 * Coordinates are (row, col) with row increasing downward, matching the
 * original's rowShift/columnShift tables (MECHANICS.md 1).
 */
#ifndef BOARD_H
#define BOARD_H

#include "rng.h"
#include "snake_types.h"

/*
 * Builds a grid of cols x rows INCLUDING the wall ring, so the playable
 * interior is rows 1..rows-2 and cols 1..cols-2 (MECHANICS.md 2.1).
 * Returns false if the dimensions do not fit or leave no interior.
 */
bool board_init(Board *b, int cols, int rows);

/* Interior extent, i.e. excluding the wall ring. */
int board_interior_cols(const Board *b);
int board_interior_rows(const Board *b);

/* Out-of-range reads return CELL_OCCUPIED so that a lookup outside the grid
 * behaves as a wall rather than as undefined memory. */
int8_t board_get(const Board *b, int row, int col);
void   board_set(Board *b, int row, int col, int8_t value);

bool board_in_bounds(const Board *b, int row, int col);

/* Number of CELL_EMPTY cells. Note that the food cell is CELL_FOOD, not
 * CELL_EMPTY, so it is not counted. */
int board_count_free(const Board *b);

/*
 * Places food uniformly at random over the free cells and writes its position
 * to out. Returns false when no free cell exists, which is the win condition
 * (MECHANICS.md 6.5).
 *
 * The original rejection-samples the whole grid until it hits a zero cell and
 * gives up after 20000 tries. Counting free cells and selecting the n-th is
 * the same distribution, but deterministic and unable to hang - see
 * MECHANICS.md 10, deviation 2.
 */
bool board_place_food(Board *b, Rng *rng, Cell *out);

#endif /* BOARD_H */
