/*
 * Snake body storage.
 *
 * The original uses a circular doubly-linked list and recycles the tail block
 * into the head on every move. A ring buffer of occupied cells is behaviorally
 * identical and is what PLAN.md 3.2 permits, provided the growth semantics in
 * MECHANICS.md 3 are preserved exactly:
 *
 *   - eating adds 5 to the displayed length immediately,
 *   - but those 5 blocks occupy no grid cell until they cycle to the head,
 *   - so the tail does not advance for the next 5 steps.
 */
#ifndef SNAKE_H
#define SNAKE_H

#include "snake_types.h"

/* One cell at (row, col); length 1, nothing pending. */
void snake_init(Snake *s, int row, int col);

/* Displayed Length: occupied cells plus not-yet-materialised growth. */
int snake_length(const Snake *s);

Cell snake_head(const Snake *s);
Cell snake_tail(const Snake *s);

/* Occupied cells only, oldest (tail) first at index 0. For rendering and
 * tests; the core never needs random access during a step. */
Cell snake_cell_at(const Snake *s, int index);
int  snake_occupied(const Snake *s);

bool snake_occupies(const Snake *s, int row, int col);

/*
 * Removes the tail cell and writes it to vacated, unless growth is pending, in
 * which case the tail stays put and false is returned.
 *
 * This must run BEFORE the destination cell is inspected: clearing the vacated
 * cell first is exactly what makes moving into it legal (MECHANICS.md 6.4).
 */
bool snake_pop_tail(Snake *s, Cell *vacated);

/* Adds a new head cell. Caller has already validated the destination. */
void snake_push_head(Snake *s, int row, int col);

/* Adds SNAKE_GROWTH to the length without occupying any cell yet. */
void snake_grow(Snake *s);

#endif /* SNAKE_H */
