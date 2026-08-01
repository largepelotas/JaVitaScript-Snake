#include "snake.h"

static int ring_index(int i)
{
    /* i is always within one wrap of the valid range. */
    if (i < 0) {
        i += SNAKE_MAX_CELLS;
    }
    if (i >= SNAKE_MAX_CELLS) {
        i -= SNAKE_MAX_CELLS;
    }
    return i;
}

void snake_init(Snake *s, int row, int col)
{
    int i;

    for (i = 0; i < SNAKE_MAX_CELLS; i++) {
        s->cells[i].row = 0;
        s->cells[i].col = 0;
    }

    s->head = 0;
    s->cells[0].row = (int16_t)row;
    s->cells[0].col = (int16_t)col;
    s->count = 1;
    s->pending = 0;
}

int snake_length(const Snake *s)
{
    return s->count + s->pending;
}

Cell snake_head(const Snake *s)
{
    return s->cells[s->head];
}

int snake_occupied(const Snake *s)
{
    return s->count;
}

Cell snake_cell_at(const Snake *s, int index)
{
    /* index 0 is the tail, index count-1 is the head. */
    int tail = ring_index(s->head - s->count + 1);
    return s->cells[ring_index(tail + index)];
}

Cell snake_tail(const Snake *s)
{
    return snake_cell_at(s, 0);
}

bool snake_occupies(const Snake *s, int row, int col)
{
    int i;

    for (i = 0; i < s->count; i++) {
        Cell c = snake_cell_at(s, i);
        if (c.row == row && c.col == col) {
            return true;
        }
    }

    return false;
}

bool snake_pop_tail(Snake *s, Cell *vacated)
{
    if (s->pending > 0) {
        /* The block being recycled is one of the fresh ones parked off-board,
         * so nothing is vacated and the snake grows by one occupied cell. */
        s->pending--;
        return false;
    }

    *vacated = snake_cell_at(s, 0);
    s->count--;
    return true;
}

void snake_push_head(Snake *s, int row, int col)
{
    s->head = ring_index(s->head + 1);
    s->cells[s->head].row = (int16_t)row;
    s->cells[s->head].col = (int16_t)col;
    s->count++;
}

void snake_grow(Snake *s)
{
    s->pending += SNAKE_GROWTH;
}
