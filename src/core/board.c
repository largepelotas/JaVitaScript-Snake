#include "board.h"

bool board_init(Board *b, int cols, int rows)
{
    int row, col;

    if (cols < 3 || rows < 3) {
        return false; /* no interior */
    }
    if (cols > BOARD_MAX_COLS || rows > BOARD_MAX_ROWS) {
        return false;
    }

    b->cols = cols;
    b->rows = rows;

    for (row = 0; row < rows; row++) {
        for (col = 0; col < cols; col++) {
            bool edge = (row == 0 || col == 0 ||
                         row == rows - 1 || col == cols - 1);
            b->cell[row][col] = edge ? CELL_OCCUPIED : CELL_EMPTY;
        }
    }

    return true;
}

int board_interior_cols(const Board *b)
{
    return b->cols - 2;
}

int board_interior_rows(const Board *b)
{
    return b->rows - 2;
}

bool board_in_bounds(const Board *b, int row, int col)
{
    return row >= 0 && row < b->rows && col >= 0 && col < b->cols;
}

int8_t board_get(const Board *b, int row, int col)
{
    if (!board_in_bounds(b, row, col)) {
        return CELL_OCCUPIED;
    }
    return b->cell[row][col];
}

void board_set(Board *b, int row, int col, int8_t value)
{
    if (!board_in_bounds(b, row, col)) {
        return;
    }
    b->cell[row][col] = value;
}

int board_count_free(const Board *b)
{
    int row, col, n = 0;

    for (row = 1; row < b->rows - 1; row++) {
        for (col = 1; col < b->cols - 1; col++) {
            if (b->cell[row][col] == CELL_EMPTY) {
                n++;
            }
        }
    }

    return n;
}

bool board_place_food(Board *b, Rng *rng, Cell *out)
{
    int free_count, pick, row, col;

    free_count = board_count_free(b);
    if (free_count == 0) {
        return false;
    }

    pick = (int)rng_below(rng, (uint32_t)free_count);

    for (row = 1; row < b->rows - 1; row++) {
        for (col = 1; col < b->cols - 1; col++) {
            if (b->cell[row][col] != CELL_EMPTY) {
                continue;
            }
            if (pick == 0) {
                b->cell[row][col] = CELL_FOOD;
                out->row = (int16_t)row;
                out->col = (int16_t)col;
                return true;
            }
            pick--;
        }
    }

    return false; /* unreachable: free_count > 0 guarantees a hit above */
}
