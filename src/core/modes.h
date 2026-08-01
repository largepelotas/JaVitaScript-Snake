/*
 * Mode table (PLAN.md 4.5, MECHANICS.md 4.1).
 *
 * Values come from the reference's mode dropdown, src/index.html:155-161, NOT
 * from snake.js. snake.js's DEFAULT_SNAKE_SPEED = 80 is the initial value of a
 * variable, not any mode's speed - see MECHANICS.md 4.1 for the first-game
 * quirk that constant causes in the original.
 *
 * The table is open-ended on purpose: Impossible (25ms) and Rush (110ms) are
 * out of scope for v1 (PLAN.md 0.6) and must remain a data-only addition.
 */
#ifndef MODES_H
#define MODES_H

#include "snake_types.h"

#define MODE_EASY   0
#define MODE_MEDIUM 1
#define MODE_HARD   2

int              mode_count(void);
const SnakeMode *mode_get(int index); /* clamped; never returns NULL */

#endif /* MODES_H */
