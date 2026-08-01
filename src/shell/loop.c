/*
 * Main loop and headless screenshot mode (PLAN.md 5.1, 5.2, 5.3).
 *
 * Two entry paths share one renderer:
 *
 *   windowed   960x544 window with vsync, real elapsed time fed to game_tick
 *   headless   software renderer into a 960x544 surface, driven by a scripted
 *              replay, saving BMPs at the script's `shot` ticks
 *
 * The headless path is the project's only reliable visual feedback loop
 * (PLAN.md 5.3), so it is not a debug afterthought: it uses the same
 * render_frame as the window, and it reports the same state hash as the core
 * test harness, which is what proves the shell changed no behavior.
 *
 * Usage:
 *   snake [--mode easy|medium|hard] [--seed N]
 *   snake --headless --script FILE [--outdir DIR]
 */
#include "loop.h"

#include "input.h"
#include "render.h"
#include "score.h"
#include "script.h"

#include "../platform/platform.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * A single frame may not advance the snake by more than this, so a stall - a
 * dragged window, a suspend, a slow first frame while the font warms up -
 * cannot fast-forward the snake through a wall (PLAN.md 5.2).
 */
#define MAX_FRAME_MS 250

typedef struct {
    SDL_Renderer *ren;
    SDL_Surface  *surface;
    RenderCtx    *rc;
    const char   *outdir;
    /* Non-zero draws the L+R diagnostic over every shot with this held-button
     * mask. The panel only ever appears on hardware, where it cannot be
     * screenshotted, so this is how its layout gets checked at all. */
    uint32_t      diag_mask;
    int           saved;
    int           failed;
} ShotSink;

/* ---- Headless ----------------------------------------------------------- */

static void on_shot(void *user, const char *name, const GameState *g)
{
    ShotSink *sink = user;
    char      path[512];

    render_frame(sink->ren, sink->rc, g);
    if (sink->diag_mask != 0) {
        render_diagnostic(sink->ren, sink->rc, sink->diag_mask);
    }
    SDL_RenderPresent(sink->ren);

    snprintf(path, sizeof path, "%s/%s.bmp", sink->outdir, name);
    if (SDL_SaveBMP(sink->surface, path) != 0) {
        plat_log("shot: SDL_SaveBMP(%s): %s", path, SDL_GetError());
        sink->failed++;
        return;
    }
    sink->saved++;
    printf("  shot %-22s state=%-7s length=%d\n", path,
           script_state_name(g->state), game_length(g));
}

/*
 * Adds a `--shot name@tick` override. This is what lets the already-blessed
 * replays in tests/replays/ double as screenshot sources: the run that produces
 * the picture is the same run that verifies expect_hash, so a screenshot can
 * never be of a game the core tests did not also check.
 */
static bool add_cli_shot(Script *s, const char *spec)
{
    const char *at = strchr(spec, '@');
    size_t      n;

    if (!at || at == spec || s->shot_count >= SCRIPT_MAX_SHOTS) {
        return false;
    }
    n = (size_t)(at - spec);
    if (n >= sizeof s->shots[0].name) {
        return false;
    }
    memcpy(s->shots[s->shot_count].name, spec, n);
    s->shots[s->shot_count].name[n] = '\0';
    s->shots[s->shot_count].tick    = strtol(at + 1, NULL, 10);
    s->shot_count++;
    return true;
}

static int run_headless(const char *script_path, const char *outdir, int theme,
                        char **shot_specs, int shot_spec_count,
                        uint32_t diag_mask)
{
    Script     script;
    GameState  game;
    RenderCtx  rc;
    ShotSink   sink;
    int        rc_code = 0;
    int        i;

    if (!script_parse(script_path, &script)) {
        return 2;
    }
    for (i = 0; i < shot_spec_count; i++) {
        if (!add_cli_shot(&script, shot_specs[i])) {
            fprintf(stderr, "bad --shot '%s', want name@tick\n", shot_specs[i]);
            return 2;
        }
    }

    memset(&sink, 0, sizeof sink);
    sink.outdir    = outdir;
    sink.diag_mask = diag_mask;

    sink.surface = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32,
                                                  SDL_PIXELFORMAT_ARGB8888);
    if (!sink.surface) {
        plat_log("headless: CreateRGBSurface: %s", SDL_GetError());
        return 1;
    }
    sink.ren = SDL_CreateSoftwareRenderer(sink.surface);
    if (!sink.ren) {
        plat_log("headless: CreateSoftwareRenderer: %s", SDL_GetError());
        SDL_FreeSurface(sink.surface);
        return 1;
    }

    if (!render_init(&rc, theme)) {
        plat_log("headless: render_init: %s", SDL_GetError());
        SDL_DestroyRenderer(sink.ren);
        SDL_FreeSurface(sink.surface);
        return 1;
    }
    sink.rc = &rc;

    printf("%s\n", script_path);
    if (!script_run(&script, &game, on_shot, &sink)) {
        rc_code = 2;
    } else {
        printf("  final state=%-7s length=%-4d hash=0x%08X\n",
               script_state_name(game.state), game_length(&game),
               game_state_hash(&game));

        /* A screenshot script may carry the same expectations as a core replay,
         * in which case the shell must land on the identical hash. */
        if (script.have_state &&
            strcmp(script.want_state, script_state_name(game.state)) != 0) {
            printf("  FAIL state %s, want %s\n",
                   script_state_name(game.state), script.want_state);
            rc_code = 1;
        }
        if (script.have_length && game_length(&game) != script.want_length) {
            printf("  FAIL length %d, want %ld\n", game_length(&game),
                   script.want_length);
            rc_code = 1;
        }
        if (script.have_hash && game_state_hash(&game) != script.want_hash) {
            printf("  FAIL hash 0x%08X, want 0x%08X\n",
                   game_state_hash(&game), script.want_hash);
            rc_code = 1;
        }
        if (sink.failed > 0) {
            rc_code = 1;
        }
        if (script.shot_count > 0 && sink.saved == 0) {
            printf("  FAIL %d shot(s) requested, none written\n",
                   script.shot_count);
            rc_code = 1;
        }
    }

    render_shutdown(&rc);
    SDL_DestroyRenderer(sink.ren);
    SDL_FreeSurface(sink.surface);
    return rc_code;
}

/* ---- Windowed ----------------------------------------------------------- */

/*
 * mode_override is a mode index, or -1 to play whatever difficulty was last
 * chosen. An override is a session choice and is deliberately NOT written back:
 * only Square (or M on the desktop) changes what the save file holds.
 */
static int run_windowed(int mode_override, uint32_t seed, int theme)
{
    SDL_Window   *win;
    SDL_Renderer *ren;
    RenderCtx     rc;
    InputState    in;
    GameState     game;
    uint32_t      prev_ms;
    uint32_t      logged_buttons = 0;
    int           saved_highscore, saved_mode;
    int           mode, cur_mode;
    bool          running = true;

    win = SDL_CreateWindow("Snake", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    if (!win) {
        plat_log("window: %s", SDL_GetError());
        return 1;
    }

    ren = SDL_CreateRenderer(win, -1,
                             SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        /* Vsync or acceleration may be unavailable under a software GL stack;
         * the accumulator does not depend on either. */
        ren = SDL_CreateRenderer(win, -1, 0);
    }
    if (!ren) {
        plat_log("renderer: %s", SDL_GetError());
        SDL_DestroyWindow(win);
        return 1;
    }

    if (!render_init(&rc, theme)) {
        plat_log("render_init: %s", SDL_GetError());
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        return 1;
    }

    score_load(&saved_highscore, &saved_mode);
    mode     = mode_override >= 0 ? mode_override : saved_mode;
    cur_mode = mode;

    if (!game_init_vita(&game, mode, seed)) {
        plat_log("game_init failed");
        render_shutdown(&rc);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        return 1;
    }

    game_set_highscore(&game, saved_highscore);
    plat_log("mode=%s (%s) seed=%u highscore=%d", mode_get(mode)->name,
             mode_override >= 0 ? "command line" : "saved", (unsigned)seed,
             saved_highscore);

    input_init(&in);
    prev_ms = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        uint32_t  now, elapsed;

        while (SDL_PollEvent(&e)) {
            if (!input_handle(&in, &e, &game)) {
                running = false;
            }
        }

        now     = SDL_GetTicks();
        elapsed = now - prev_ms;
        prev_ms = now;
        if (elapsed > MAX_FRAME_MS) {
            elapsed = MAX_FRAME_MS;
        }

        (void)game_tick(&game, elapsed);

        /* Watching the values rather than the event flags catches every path
         * that can raise them, including the immediate first step that
         * game_queue_input takes out of READY, and Square landing on a new
         * difficulty. Both live in one record, so one write covers both. */
        if (game.highscore > saved_highscore || game.mode != cur_mode) {
            if (game.highscore > saved_highscore) {
                saved_highscore = game.highscore;
            }
            if (game.mode != cur_mode) {
                cur_mode   = game.mode;
                saved_mode = game.mode;
                plat_log("difficulty: %s", mode_get(saved_mode)->name);
            }
            (void)score_save(saved_highscore, saved_mode);
        }

        render_frame(ren, &rc, &game);

        /* Hidden diagnostic: welcome screen only, so it cannot be summoned
         * mid-game by a stray grip (PLAN.md 6.5). */
        if (game.state == STATE_WELCOME && input_diag_active(&in)) {
            render_diagnostic(ren, &rc, in.buttons);
            if (in.buttons != logged_buttons) {
                logged_buttons = in.buttons;
                plat_log("diagnostic: held button mask 0x%08X",
                         (unsigned)in.buttons);
            }
        }

        SDL_RenderPresent(ren);
    }

    input_shutdown(&in);
    render_shutdown(&rc);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    return 0;
}

/* ---- Entry -------------------------------------------------------------- */

static int mode_from_name(const char *s)
{
    if (strcmp(s, "easy") == 0)   return MODE_EASY;
    if (strcmp(s, "medium") == 0) return MODE_MEDIUM;
    if (strcmp(s, "hard") == 0)   return MODE_HARD;
    return -1;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--mode easy|medium|hard] [--seed N]\n"
            "       (without --mode, the saved difficulty is used;"
            " press M or Square on the welcome screen to change it)\n"
            "       %s --headless --script FILE [--outdir DIR]"
            " [--shot name@tick ...] [--diag MASK]\n",
            argv0, argv0);
}

#define MAX_CLI_SHOTS SCRIPT_MAX_SHOTS

int shell_main(int argc, char **argv)
{
    const char *script_path = NULL;
    const char *outdir      = "artifacts";
    char       *shots[MAX_CLI_SHOTS];
    int         shot_count  = 0;
    uint32_t    diag_mask   = 0;
    int         headless    = 0;
    int         mode        = -1; /* -1: play the difficulty last chosen */
    uint32_t    seed        = 0;
    int         have_seed   = 0;
    int         status;
    int         i;
    Uint32      sdl_flags;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = 1;
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            script_path = argv[++i];
        } else if (strcmp(argv[i], "--outdir") == 0 && i + 1 < argc) {
            outdir = argv[++i];
        } else if (strcmp(argv[i], "--diag") == 0 && i + 1 < argc) {
            diag_mask = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            if (shot_count >= MAX_CLI_SHOTS) {
                fprintf(stderr, "too many --shot options\n");
                return 2;
            }
            shots[shot_count++] = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = mode_from_name(argv[++i]);
            if (mode < 0) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed      = (uint32_t)strtoul(argv[++i], NULL, 0);
            have_seed = 1;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (headless && !script_path) {
        usage(argv[0]);
        return 2;
    }

    /* Headless needs no window system and no pad; asking for either would make
     * screenshots impossible on a machine without a display. */
    sdl_flags = headless ? 0u : (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    if (SDL_Init(sdl_flags) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    plat_init();

    if (headless) {
        status = run_headless(script_path, outdir, THEME_MAIN, shots,
                              shot_count, diag_mask);
    } else {
        if (!have_seed) {
            seed = plat_seed();
        }
        status = run_windowed(mode, seed, THEME_MAIN);
    }

    plat_shutdown();
    SDL_Quit();
    return status;
}
