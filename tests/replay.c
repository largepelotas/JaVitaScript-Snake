/*
 * Scripted-replay harness (PLAN.md 4.7).
 *
 * A replay file pins a seed, a mode and a list of (tick, input) pairs to an
 * expected final state hash. That gives regression coverage which survives
 * refactors, and it is how the SDL shell is proved not to have changed
 * behavior: `make parity` feeds these same files through src/shell/loop.c
 * --headless, which must report the identical hash.
 *
 * The parser and the driver live in src/shell/script.c so that both harnesses
 * run the identical code; that file is pure C99, so nothing here links SDL.
 * See src/shell/script.h for the file format.
 *
 * Usage:
 *   replay <file>            verify against the file's expect_ lines
 *   replay --bless <file>    rewrite the file's expect_ lines from this run
 */
#include "../src/shell/script.h"

#include <stdio.h>
#include <string.h>

#define MAX_LINE 256

/* Rewrites the expect_ lines in place. Everything else in the file, comments
 * included, is copied through untouched. */
static int bless(const char *path, const GameState *g)
{
    FILE *in, *out;
    char  line[MAX_LINE];
    char  tmp[512];

    snprintf(tmp, sizeof tmp, "%s.tmp", path);

    in = fopen(path, "r");
    if (!in) {
        return 0;
    }
    out = fopen(tmp, "w");
    if (!out) {
        fclose(in);
        return 0;
    }

    while (fgets(line, sizeof line, in)) {
        if (strncmp(line, "expect_", 7) == 0) {
            continue;
        }
        fputs(line, out);
    }
    fclose(in);

    fprintf(out, "expect_state %s\n", script_state_name(g->state));
    fprintf(out, "expect_length %d\n", game_length(g));
    fprintf(out, "expect_hash 0x%08X\n", game_state_hash(g));
    fclose(out);

    if (rename(tmp, path) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    Script      script;
    GameState   g;
    const char *path;
    int         do_bless = 0;
    int         failures = 0;

    if (argc == 3 && strcmp(argv[1], "--bless") == 0) {
        do_bless = 1;
        path = argv[2];
    } else if (argc == 2) {
        path = argv[1];
    } else {
        fprintf(stderr, "usage: %s [--bless] <replay-file>\n", argv[0]);
        return 2;
    }

    if (!script_parse(path, &script)) {
        return 2;
    }
    /* No shot callback: `shot` directives are a headless-renderer concern and
     * are parsed but ignored here. */
    if (!script_run(&script, &g, NULL, NULL)) {
        return 2;
    }

    if (do_bless) {
        if (!bless(path, &g)) {
            fprintf(stderr, "replay: could not rewrite %s\n", path);
            return 2;
        }
        printf("blessed %s: state=%s length=%d hash=0x%08X\n", path,
               script_state_name(g.state), game_length(&g),
               game_state_hash(&g));
        return 0;
    }

    if (script.have_state &&
        strcmp(script.want_state, script_state_name(g.state)) != 0) {
        printf("  FAIL %s: state %s, want %s\n", path,
               script_state_name(g.state), script.want_state);
        failures++;
    }
    if (script.have_length && game_length(&g) != script.want_length) {
        printf("  FAIL %s: length %d, want %ld\n", path, game_length(&g),
               script.want_length);
        failures++;
    }
    if (script.have_hash && game_state_hash(&g) != script.want_hash) {
        printf("  FAIL %s: hash 0x%08X, want 0x%08X\n", path,
               game_state_hash(&g), script.want_hash);
        failures++;
    }
    if (!script.have_state && !script.have_length && !script.have_hash) {
        printf("  FAIL %s: no expectations; run --bless first\n", path);
        failures++;
    }

    if (failures == 0) {
        printf("  ok   %-34s state=%-7s length=%-4d hash=0x%08X\n", path,
               script_state_name(g.state), game_length(&g),
               game_state_hash(&g));
    }

    return failures == 0 ? 0 : 1;
}
