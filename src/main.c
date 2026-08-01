/*
 * Entry point (PLAN.md 9).
 *
 * Deliberately empty of logic: everything a target might need to do around
 * main - SDL's Vita main shim, the newlib heap size - belongs in
 * src/platform/, and everything the game does belongs in src/shell/.
 */
#include "shell/loop.h"

int main(int argc, char **argv)
{
    return shell_main(argc, argv);
}
