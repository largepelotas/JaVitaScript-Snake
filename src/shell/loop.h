/*
 * Shell entry point.
 *
 * src/main.c is a three-line file that calls this, so the Vita target can have
 * its own main() (SDL's Vita backend and the newlib heap declaration both live
 * near main) without the loop itself being duplicated or #ifdef'd.
 */
#ifndef LOOP_H
#define LOOP_H

int shell_main(int argc, char **argv);

#endif /* LOOP_H */
