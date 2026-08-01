/*
 * Input translation tests (PLAN.md 0.1, 5.1).
 *
 * "Fully playable on the host" is an exit criterion for Phase 3, and pressing
 * keys by hand proves it once, for one person, on one machine. Everything about
 * src/shell/input.c is verifiable without a window: SDL events are plain
 * structs, so the tests below synthesise them and assert on the resulting game
 * state. No SDL_Init, no display, no pad.
 *
 * The assertions are on observable behavior - where the snake ends up after a
 * step - rather than on the core's internal direction fields, so a refactor of
 * the input queue cannot make these pass vacuously.
 */
#include "../src/shell/input.h"

#include "../src/platform/platform.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                             \
            printf("\n    condition: %s\n", #cond);                          \
        }                                                                    \
    } while (0)

static void banner(const char *name)
{
    printf("== %s\n", name);
}

static SDL_Event key_event(SDL_Keycode sym, Uint8 repeat)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type            = SDL_KEYDOWN;
    e.key.repeat      = repeat;
    e.key.keysym.sym  = sym;
    return e;
}

static SDL_Event hat_event(Uint8 value)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type       = SDL_JOYHATMOTION;
    e.jhat.value = value;
    return e;
}

static SDL_Event axis_event(Uint8 axis, Sint16 value)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type        = SDL_JOYAXISMOTION;
    e.jaxis.axis  = axis;
    e.jaxis.value = value;
    return e;
}

static SDL_Event button_event(Uint8 button)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type          = SDL_JOYBUTTONDOWN;
    e.jbutton.button = button;
    return e;
}

static SDL_Event button_up_event(Uint8 button)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type           = SDL_JOYBUTTONUP;
    e.jbutton.button = button;
    return e;
}

/* A game sitting in READY, i.e. placed but not moving. */
static void fresh(GameState *g, InputState *in)
{
    memset(in, 0, sizeof *in);
    in->stick_dir = DIR_NONE;
    game_init_vita(g, MODE_MEDIUM, 12345u);
    game_action(g, ACTION_CONFIRM);
}

/* Feeds one event and reports whether the shell wants to keep running. */
static bool feed(InputState *in, GameState *g, SDL_Event e)
{
    return input_handle(in, &e, g);
}

/*
 * The first direction press steps immediately (MECHANICS.md 6.6), so after
 * feeding a direction from READY the head has already moved one cell. These
 * helpers compare against the head position captured just before.
 */
static Cell head_of(const GameState *g)
{
    return snake_head(&g->snake);
}

static void test_keyboard_directions(void)
{
    struct {
        SDL_Keycode key;
        int         drow, dcol;
        const char *what;
    } cases[] = {
        { SDLK_RIGHT, 0,  1, "right arrow" },
        { SDLK_DOWN,  1,  0, "down arrow"  },
        { SDLK_d,     0,  1, "d"           },
        { SDLK_s,     1,  0, "s"           }
    };
    size_t i;

    banner("keyboard directions move the snake");

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        GameState  g;
        InputState in;
        Cell       before, after;

        fresh(&g, &in);
        before = head_of(&g);
        CHECK(feed(&in, &g, key_event(cases[i].key, 0)), "%s should not quit",
              cases[i].what);
        after = head_of(&g);

        CHECK(after.row == before.row + cases[i].drow &&
                  after.col == before.col + cases[i].dcol,
              "%s: head moved (%d,%d), want (%d,%d)", cases[i].what,
              after.row - before.row, after.col - before.col, cases[i].drow,
              cases[i].dcol);
        CHECK(g.state == STATE_PLAYING, "%s should start the game",
              cases[i].what);
    }

    /* UP and LEFT from the start would reverse against lastMove = RIGHT, so
     * they are tested from a state where they are legal instead. */
    {
        GameState  g;
        InputState in;
        Cell       before, after;

        fresh(&g, &in);
        feed(&in, &g, key_event(SDLK_DOWN, 0));
        before = head_of(&g);
        feed(&in, &g, key_event(SDLK_LEFT, 0));
        (void)game_tick(&g, mode_get(g.mode)->step_ms);
        after = head_of(&g);
        CHECK(after.col == before.col - 1, "left arrow: col moved %d, want -1",
              after.col - before.col);
    }
}

static void test_key_repeat_ignored(void)
{
    GameState  g;
    InputState in;
    Cell       before, after;

    banner("held keys do not refill the premove queue");

    fresh(&g, &in);
    before = head_of(&g);
    feed(&in, &g, key_event(SDLK_RIGHT, 1)); /* repeat = 1 */
    after = head_of(&g);

    CHECK(after.row == before.row && after.col == before.col,
          "a repeat event moved the snake");
    CHECK(g.state == STATE_READY, "a repeat event started the game");
}

static void test_actions(void)
{
    GameState  g;
    InputState in;

    banner("confirm, pause and back");

    memset(&in, 0, sizeof in);
    game_init_vita(&g, MODE_MEDIUM, 1u);
    CHECK(g.state == STATE_WELCOME, "expected to start at the welcome screen");

    feed(&in, &g, key_event(SDLK_SPACE, 0));
    CHECK(g.state == STATE_READY, "space should dismiss the welcome screen");

    feed(&in, &g, key_event(SDLK_p, 0));
    CHECK(g.state == STATE_PAUSED, "p should pause");
    feed(&in, &g, key_event(SDLK_p, 0));
    CHECK(g.state == STATE_READY, "p should resume to where it paused from");

    feed(&in, &g, key_event(SDLK_ESCAPE, 0));
    CHECK(g.state == STATE_PAUSED, "escape should also pause");
    feed(&in, &g, key_event(SDLK_ESCAPE, 0));
    CHECK(g.state == STATE_READY, "escape should also unpause");

    /* BACK only applies from an end state (MECHANICS.md 6.6). */
    g.state = STATE_DEAD;
    feed(&in, &g, key_event(SDLK_BACKSPACE, 0));
    CHECK(g.state == STATE_WELCOME, "backspace should return to the welcome "
                                    "screen from death");
}

/* The desktop stand-ins for Square. x is already Circle, so the difficulty keys
 * are m and tab (src/shell/input.c). */
static void test_cycle_mode_keys(void)
{
    GameState  g;
    InputState in;

    banner("difficulty keys");

    memset(&in, 0, sizeof in);
    game_init_vita(&g, MODE_EASY, 2u);

    feed(&in, &g, key_event(SDLK_m, 0));
    CHECK(g.mode == MODE_MEDIUM, "m should cycle easy -> medium (got %d)",
          g.mode);
    feed(&in, &g, key_event(SDLK_TAB, 0));
    CHECK(g.mode == MODE_HARD, "tab should cycle medium -> hard (got %d)",
          g.mode);

    /* Held keys must not spin through the table (the same rule as directions). */
    feed(&in, &g, key_event(SDLK_m, 1));
    CHECK(g.mode == MODE_HARD, "a key repeat should not cycle difficulty");

    /* And the keys must not double as a start button. */
    CHECK(g.state == STATE_WELCOME, "cycling difficulty left the welcome "
                                    "screen");
}

static void test_quit(void)
{
    GameState  g;
    InputState in;
    SDL_Event  quit;

    banner("quit paths");

    fresh(&g, &in);
    CHECK(!feed(&in, &g, key_event(SDLK_q, 0)), "q should quit");

    memset(&quit, 0, sizeof quit);
    quit.type = SDL_QUIT;
    fresh(&g, &in);
    CHECK(!feed(&in, &g, quit), "SDL_QUIT should quit");
    CHECK(feed(&in, &g, key_event(SDLK_RIGHT, 0)), "a direction should not quit");
}

static void test_hat(void)
{
    GameState  g;
    InputState in;
    Cell       before, after;

    banner("d-pad hat directions");

    fresh(&g, &in);
    before = head_of(&g);
    feed(&in, &g, hat_event(SDL_HAT_RIGHT));
    after = head_of(&g);
    CHECK(after.col == before.col + 1, "hat right: col moved %d, want +1",
          after.col - before.col);

    /* A diagonal must resolve to exactly one direction, deterministically. */
    fresh(&g, &in);
    before = head_of(&g);
    feed(&in, &g, hat_event(SDL_HAT_RIGHTDOWN));
    after = head_of(&g);
    CHECK((after.row == before.row + 1 && after.col == before.col) ||
              (after.col == before.col + 1 && after.row == before.row),
          "a diagonal hat should resolve to one axis, moved (%d,%d)",
          after.row - before.row, after.col - before.col);
}

static void test_stick_deadzone_and_edges(void)
{
    GameState  g;
    InputState in;
    Cell       before, after;

    banner("analog stick: deadzone and edge triggering");

    fresh(&g, &in);
    before = head_of(&g);
    feed(&in, &g, axis_event(0, 4000)); /* inside the deadzone */
    after = head_of(&g);
    CHECK(after.row == before.row && after.col == before.col,
          "a stick inside the deadzone moved the snake");
    CHECK(in.stick_dir == DIR_NONE, "deadzone should leave stick_dir unset");

    feed(&in, &g, axis_event(0, 20000)); /* full right */
    CHECK(in.stick_dir == DIR_RIGHT, "stick right should register");
    after = head_of(&g);
    CHECK(after.col == before.col + 1, "stick right: col moved %d, want +1",
          after.col - before.col);

    /* Holding the same direction must not queue again: the premove queue is one
     * deep, and refilling it every frame would change turn timing. */
    before = head_of(&g);
    feed(&in, &g, axis_event(0, 32000));
    after = head_of(&g);
    CHECK(after.col == before.col && after.row == before.row,
          "holding the stick queued a second move");

    feed(&in, &g, axis_event(0, 0)); /* recentre */
    CHECK(in.stick_dir == DIR_NONE, "recentring should clear stick_dir");

    /* The dominant axis wins when both are deflected. */
    feed(&in, &g, axis_event(1, 30000)); /* down hard  */
    feed(&in, &g, axis_event(0, 9000));  /* right, weaker */
    CHECK(in.stick_dir == DIR_DOWN, "the larger deflection should win");
}

static void test_pad_buttons(void)
{
    GameState  g;
    InputState in;
    int        confirm = plat_button_for(PAD_CONFIRM);
    int        pause   = plat_button_for(PAD_PAUSE);
    int        back    = plat_button_for(PAD_BACK);
    int        square  = plat_button_for(PAD_CYCLE_MODE);

    banner("pad buttons go through the platform table");

    CHECK(confirm >= 0 && pause >= 0 && back >= 0 && square >= 0,
          "the desktop table should map confirm, pause, back and square");
    CHECK(square != confirm && square != pause && square != back,
          "square shares an index with another bound button");

    memset(&in, 0, sizeof in);
    game_init_vita(&g, MODE_MEDIUM, 7u);

    /* Square, on the welcome screen, before anything else has happened. */
    feed(&in, &g, button_event((Uint8)square));
    CHECK(g.mode == MODE_HARD, "square should cycle medium -> hard (got %d)",
          g.mode);
    CHECK(g.state == STATE_WELCOME, "square should not start the game");
    feed(&in, &g, button_event((Uint8)square));
    CHECK(g.mode == MODE_EASY, "square should wrap hard -> easy (got %d)",
          g.mode);

    feed(&in, &g, button_event((Uint8)confirm));
    CHECK(g.state == STATE_READY, "confirm button should dismiss the welcome "
                                  "screen");

    feed(&in, &g, button_event((Uint8)square));
    CHECK(g.mode == MODE_EASY, "square changed the difficulty off the welcome "
                               "screen (got %d)", g.mode);

    feed(&in, &g, button_event((Uint8)pause));
    CHECK(g.state == STATE_PAUSED, "pause button should pause");
    feed(&in, &g, button_event((Uint8)pause));
    CHECK(g.state == STATE_READY, "pause button should unpause");

    g.state = STATE_WON;
    feed(&in, &g, button_event((Uint8)back));
    CHECK(g.state == STATE_WELCOME, "back button should leave the win screen");

    /* An index nothing maps to must be inert rather than guessed at
     * (PLAN.md 6.5). */
    g.state = STATE_WELCOME;
    feed(&in, &g, button_event(31));
    CHECK(g.state == STATE_WELCOME, "an unmapped button should do nothing");
}

/*
 * The diagnostic is the answer to PLAN.md 11.1: the Vita's button indices
 * cannot be confirmed without hardware, so the game shows them. That makes it
 * the one feature whose whole job is to be correct on a device I cannot run -
 * which is all the more reason for its logic to be pinned down here.
 */
static void test_button_diagnostic(void)
{
    GameState  g;
    InputState in;
    int        l = plat_button_for(PAD_L);
    int        r = plat_button_for(PAD_R);

    banner("L+R button-index diagnostic");

    memset(&in, 0, sizeof in);
    game_init_vita(&g, MODE_MEDIUM, 3u);

    CHECK(!input_diag_active(&in), "nothing held should not arm the diagnostic");

    feed(&in, &g, button_event((Uint8)l));
    CHECK(!input_diag_active(&in), "L alone should not arm the diagnostic");
    feed(&in, &g, button_event((Uint8)r));
    CHECK(input_diag_active(&in), "L+R should arm the diagnostic");

    CHECK((in.buttons & (1u << l)) != 0 && (in.buttons & (1u << r)) != 0,
          "both shoulders should show in the held mask");

    /* The mask is what the panel prints, so a released button must leave it. */
    feed(&in, &g, button_up_event((Uint8)l));
    CHECK((in.buttons & (1u << l)) == 0, "releasing L should clear its bit");
    CHECK(!input_diag_active(&in), "releasing L should disarm the diagnostic");

    /* An index beyond the mask must not corrupt neighbouring bits. */
    in.buttons = 0;
    feed(&in, &g, button_event(INPUT_MAX_BUTTONS + 3));
    CHECK(in.buttons == 0, "an out-of-range index should not set any bit");

    CHECK(g.state == STATE_WELCOME,
          "the diagnostic's buttons should not disturb the game state");
}

/*
 * While the panel is up, a press reports its index and does nothing else.
 *
 * This is the fix for what hardware found (TESTPLAN item 4, 2026-08-01): Cross
 * started the game and closed the panel, so index 2 was the one index the
 * diagnostic could not be used to confirm - the exact question the diagnostic
 * exists to answer (PLAN.md 11.1).
 */
static void test_diagnostic_swallows_input(void)
{
    GameState  g;
    InputState in;
    int        l       = plat_button_for(PAD_L);
    int        r       = plat_button_for(PAD_R);
    int        confirm = plat_button_for(PAD_CONFIRM);
    int        square  = plat_button_for(PAD_CYCLE_MODE);
    int        pause   = plat_button_for(PAD_PAUSE);

    banner("held L+R turns every button into a probe");

    memset(&in, 0, sizeof in);
    game_init_vita(&g, MODE_MEDIUM, 5u);

    feed(&in, &g, button_event((Uint8)l));
    feed(&in, &g, button_event((Uint8)r));

    feed(&in, &g, button_event((Uint8)confirm));
    CHECK(g.state == STATE_WELCOME,
          "Cross should not start the game while the panel is up");
    CHECK((in.buttons & (1u << confirm)) != 0,
          "Cross should still report its index to the panel");

    feed(&in, &g, button_event((Uint8)square));
    CHECK(g.mode == MODE_MEDIUM,
          "Square should not cycle the difficulty while probing (got %d)",
          g.mode);
    CHECK((in.buttons & (1u << square)) != 0,
          "Square should still report its index");

    feed(&in, &g, button_event((Uint8)pause));
    CHECK(g.state == STATE_WELCOME, "Start should be inert while probing");

    /* Releasing a shoulder puts the game back in charge, in the same frame. */
    feed(&in, &g, button_up_event((Uint8)r));
    feed(&in, &g, button_event((Uint8)confirm));
    CHECK(g.state == STATE_READY,
          "Cross should work again once the panel is dismissed");

    /*
     * The gate must not reach past the welcome screen: a player resting their
     * hands on both shoulders mid-game still has to be able to steer and pause
     * (PLAN.md 6.5 - the panel is welcome-screen only, so the gate is too).
     */
    feed(&in, &g, button_event((Uint8)r));
    CHECK(input_diag_active(&in), "setup: both shoulders should be held");
    /* The desktop table steers by hat, not by button index (PAD_UP is -1
     * there), so the direction has to arrive the way the desktop sends it. */
    feed(&in, &g, hat_event(SDL_HAT_UP));
    CHECK(g.state == STATE_PLAYING,
          "a direction with both shoulders held should still start the snake");
    feed(&in, &g, button_event((Uint8)pause));
    CHECK(g.state == STATE_PAUSED,
          "pause with both shoulders held should still pause");
}

int main(void)
{
    test_keyboard_directions();
    test_key_repeat_ignored();
    test_actions();
    test_cycle_mode_keys();
    test_quit();
    test_hat();
    test_stick_deadzone_and_edges();
    test_pad_buttons();
    test_button_diagnostic();
    test_diagnostic_swallows_input();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
