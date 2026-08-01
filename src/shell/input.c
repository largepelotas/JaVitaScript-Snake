/*
 * Input translation.
 *
 * Three sources feed the same two core entry points:
 *   keyboard   the desktop's primary control, mirroring the original's arrows
 *   d-pad/hat  a gamepad's discrete directions
 *   left stick converted to a discrete direction with a deadzone (PLAN.md 6.5)
 *
 * The stick is edge-triggered: a direction is queued when the stick ENTERS a
 * direction, not every frame it is held. Queueing per frame would flood the
 * premove queue and change turn timing, which is exactly the behavior the core
 * tests pin down.
 */
#include "input.h"

#include "../platform/platform.h"

/* Roughly a quarter deflection. Large enough that resting drift never turns the
 * snake, small enough that a deliberate flick registers immediately. */
#define STICK_DEADZONE 8000

/*
 * The return value of game_queue_input is deliberately dropped. From READY the
 * first direction takes a step immediately and reports its events, but the loop
 * detects a beaten highscore by watching g->highscore rather than by collecting
 * event flags, so no caller has to remember which entry points can emit them.
 */
static void queue_dir(GameState *g, Direction dir)
{
    (void)game_queue_input(g, dir);
}

static bool button_is(PadButton logical, int index)
{
    int mapped = plat_button_for(logical);
    return mapped >= 0 && mapped == index;
}

static bool handle_key(const SDL_Event *e, GameState *g)
{
    switch (e->key.keysym.sym) {
    case SDLK_UP:
    case SDLK_w:
        queue_dir(g, DIR_UP);
        break;
    case SDLK_DOWN:
    case SDLK_s:
        queue_dir(g, DIR_DOWN);
        break;
    case SDLK_LEFT:
    case SDLK_a:
        queue_dir(g, DIR_LEFT);
        break;
    case SDLK_RIGHT:
    case SDLK_d:
        queue_dir(g, DIR_RIGHT);
        break;

    /* Cross on the Vita; space and return are what the original's dialogs
     * accept (`snake.js:944-950`), so they stay bound on the desktop. */
    case SDLK_SPACE:
    case SDLK_RETURN:
    case SDLK_z:
        game_action(g, ACTION_CONFIRM);
        break;

    /* START on the Vita. The original unpauses with space, which is already
     * confirm here, so the desktop uses p and escape. */
    case SDLK_p:
    case SDLK_ESCAPE:
        game_action(g, ACTION_PAUSE);
        break;

    /* Circle on the Vita. */
    case SDLK_BACKSPACE:
    case SDLK_x:
        game_action(g, ACTION_BACK);
        break;

    /* Square on the Vita: cycle difficulty on the welcome screen. The original
     * uses a dropdown, which has no keyboard equivalent worth mimicking, so
     * these are chosen rather than ported - x is already Circle. */
    case SDLK_m:
    case SDLK_TAB:
        game_action(g, ACTION_CYCLE_MODE);
        break;

    case SDLK_q:
        return false;

    default:
        break;
    }
    return true;
}

static void handle_hat(uint8_t value, GameState *g)
{
    /* A hat can report a diagonal. Taking the vertical component first is
     * arbitrary but must be deterministic, and it matches how the original's
     * keyboard handler resolves a single event to a single direction. */
    if (value & SDL_HAT_UP) {
        queue_dir(g, DIR_UP);
    } else if (value & SDL_HAT_DOWN) {
        queue_dir(g, DIR_DOWN);
    } else if (value & SDL_HAT_LEFT) {
        queue_dir(g, DIR_LEFT);
    } else if (value & SDL_HAT_RIGHT) {
        queue_dir(g, DIR_RIGHT);
    }
}

static void handle_axis(InputState *in, const SDL_Event *e, GameState *g)
{
    Direction dir = DIR_NONE;
    int       x, y;

    if (e->jaxis.axis == 0) {
        in->axis_x = e->jaxis.value;
    } else if (e->jaxis.axis == 1) {
        in->axis_y = e->jaxis.value;
    } else {
        return; /* right stick and triggers do not steer */
    }
    x = in->axis_x;
    y = in->axis_y;

    if (x > STICK_DEADZONE || x < -STICK_DEADZONE ||
        y > STICK_DEADZONE || y < -STICK_DEADZONE) {
        int ax = x < 0 ? -x : x;
        int ay = y < 0 ? -y : y;

        if (ax >= ay) {
            dir = x > 0 ? DIR_RIGHT : DIR_LEFT;
        } else {
            dir = y > 0 ? DIR_DOWN : DIR_UP;
        }
    }

    if (dir != in->stick_dir) {
        in->stick_dir = dir;
        if (dir != DIR_NONE) {
            queue_dir(g, dir);
        }
    }
}

bool input_diag_active(const InputState *in)
{
    int l = plat_button_for(PAD_L);
    int r = plat_button_for(PAD_R);

    if (l < 0 || r < 0 || l >= INPUT_MAX_BUTTONS || r >= INPUT_MAX_BUTTONS) {
        return false;
    }
    return (in->buttons & (1u << l)) != 0 && (in->buttons & (1u << r)) != 0;
}

void input_init(InputState *in)
{
    in->pad       = NULL;
    in->stick_dir = DIR_NONE;
    in->axis_x    = 0;
    in->axis_y    = 0;
    in->buttons   = 0;

    if (SDL_NumJoysticks() > 0) {
        in->pad = SDL_JoystickOpen(0);
        if (in->pad) {
            plat_log("input: opened pad '%s', %d buttons, %d axes, %d hats",
                     SDL_JoystickName(in->pad),
                     SDL_JoystickNumButtons(in->pad),
                     SDL_JoystickNumAxes(in->pad),
                     SDL_JoystickNumHats(in->pad));
        }
    }
}

void input_shutdown(InputState *in)
{
    if (in->pad) {
        SDL_JoystickClose(in->pad);
        in->pad = NULL;
    }
}

bool input_handle(InputState *in, const SDL_Event *e, GameState *g)
{
    switch (e->type) {
    case SDL_QUIT:
        return false;

    case SDL_KEYDOWN:
        if (e->key.repeat) {
            break; /* held keys must not refill the premove queue */
        }
        return handle_key(e, g);

    case SDL_JOYBUTTONUP:
        if (e->jbutton.button < INPUT_MAX_BUTTONS) {
            in->buttons &= ~(1u << e->jbutton.button);
        }
        break;

    case SDL_JOYBUTTONDOWN: {
        int b = e->jbutton.button;

        if (b < INPUT_MAX_BUTTONS) {
            in->buttons |= 1u << b;
        }

        /*
         * While the diagnostic panel is up, a press is a probe and nothing
         * else. Without this, Cross starts the game and takes the panel with
         * it, so index 2 is the one index the diagnostic cannot report - which
         * is exactly what happened on hardware (TESTPLAN item 4, 2026-08-01).
         * Square had the same problem more quietly: cycling the difficulty
         * while probing wrote a mode the player had not chosen.
         *
         * The bit was recorded above, so the panel still shows the index. The
         * gate matches the panel's own condition (loop.c): both shoulder
         * buttons held, welcome screen only, so it cannot swallow input during
         * play (PLAN.md 6.5).
         */
        if (g->state == STATE_WELCOME && input_diag_active(in)) {
            break;
        }

        if (button_is(PAD_UP, b)) {
            queue_dir(g, DIR_UP);
        } else if (button_is(PAD_DOWN, b)) {
            queue_dir(g, DIR_DOWN);
        } else if (button_is(PAD_LEFT, b)) {
            queue_dir(g, DIR_LEFT);
        } else if (button_is(PAD_RIGHT, b)) {
            queue_dir(g, DIR_RIGHT);
        } else if (button_is(PAD_CONFIRM, b)) {
            game_action(g, ACTION_CONFIRM);
        } else if (button_is(PAD_PAUSE, b)) {
            game_action(g, ACTION_PAUSE);
        } else if (button_is(PAD_BACK, b)) {
            game_action(g, ACTION_BACK);
        } else if (button_is(PAD_CYCLE_MODE, b)) {
            game_action(g, ACTION_CYCLE_MODE);
        }
        break;
    }

    case SDL_JOYHATMOTION:
        handle_hat(e->jhat.value, g);
        break;

    case SDL_JOYAXISMOTION:
        handle_axis(in, e, g);
        break;

    default:
        break;
    }
    return true;
}
