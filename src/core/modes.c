#include "modes.h"

/*
 * walls_kill is true for every shipped mode: the original has no wrap mode at
 * any difficulty (MECHANICS.md 6.3). The field exists so that adding one later
 * stays a data change.
 */
static const SnakeMode MODES[] = {
    { "Easy",   100u, true },
    { "Medium",  75u, true },
    { "Hard",    50u, true }
};

int mode_count(void)
{
    return (int)(sizeof MODES / sizeof MODES[0]);
}

const SnakeMode *mode_get(int index)
{
    if (index < 0) {
        index = 0;
    }
    if (index >= mode_count()) {
        index = mode_count() - 1;
    }
    return &MODES[index];
}
