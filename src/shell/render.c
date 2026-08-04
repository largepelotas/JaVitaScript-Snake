/*
 * Drawing. Every constant here is cited to docs/MECHANICS.md; nothing is
 * eyeballed. The layout is MECHANICS.md 9, the colors and strings are
 * MECHANICS.md 8, and the CSS geometry of the dialogs is
 * `css/common-snake.css:72-111` as recorded in MECHANICS.md 8.4.
 */
#include "render.h"

#include "../platform/platform.h"

#include <stdio.h>

/* ---- Layout (MECHANICS.md 9) ------------------------------------------- */

#define BLOCK   20 /* MECHANICS.md 2: 20x20, unchanged from the original */
#define FIELD_X 20 /* the playing field div sits at (block, block)       */
#define FIELD_Y 20

/*
 * The playfield is sized from the board rather than hardcoded to 920x480.
 * For the shipping 48x26 board those are the same number (MECHANICS.md 9), and
 * for the small boards the replay scripts use it keeps the picture honest
 * instead of drawing a Vita-sized field around a 10x6 grid.
 */
#define FIELD_W(g) ((int)((g)->board.cols - 2) * BLOCK)
#define FIELD_H(g) ((int)((g)->board.rows - 2) * BLOCK)

/* A block at (row, col) is placed relative to the CONTAINER, not the field
 * (MECHANICS.md 2.1), so interior cell (1,1) lands exactly on the field's
 * top-left corner. */
#define CELL_X(col) ((col) * BLOCK)
#define CELL_Y(row) ((row) * BLOCK)

/*
 * The snake block images are 20x20 with a 1px border in the playfield color and
 * an 18x18 interior (MECHANICS.md 8.1). Drawing an 18x18 fill at a 1px inset
 * reproduces the 1px gap between adjacent segments exactly. Food has border: 0
 * and is a flush 20x20 square.
 */
#define SNAKE_INSET 1

/*
 * HUD panels: divs at left 30 and cWidth-140, padding 8px, top computed as
 * blockHeight + fHeight + round((bottomPanelHeight - 30) / 2)
 * (`snake.js:1211-1227`, `common-snake.css:65-70`). On the Vita board that
 * evaluates to 507, the value recorded in MECHANICS.md 9.
 */
#define HUD_PAD          8
#define HUD_PANEL_OFFSET 7 /* round((44 - 30) / 2) */
#define HUD_PANEL_TOP(g) (FIELD_Y + FIELD_H(g) + HUD_PANEL_OFFSET)
#define LENGTH_LEFT      30
#define HIGHSCORE_LEFT   (SCREEN_W - 140)

/* Dialogs are absolutely positioned at 50%/50% of the container with negative
 * margins (`common-snake.css:72-111`). The container is the whole screen. */
#define CENTER_X (SCREEN_W / 2)
#define CENTER_Y (SCREEN_H / 2)

#define DIALOG_CONTENT_W 300
#define DIALOG_PAD       8
#define DIALOG_BOX_W     (DIALOG_CONTENT_W + 2 * DIALOG_PAD) /* 316 */
#define DIALOG_LEFT      (CENTER_X - 158)                    /* margin-left -158 */

#define WELCOME_TOP  (CENTER_Y - 100) /* margin-top -100, height auto */
#define ENDGAME_TOP  (CENTER_Y - 75)  /* margin-top -75, height 100 + padding */
#define ENDGAME_BOX_H (100 + 2 * DIALOG_PAD)

#define PAUSE_W    300
#define PAUSE_H    80
#define PAUSE_LEFT (CENTER_X - 150)
#define PAUSE_TOP  (CENTER_Y - 40)
#define PAUSE_PAD  10 /* the pause div's inner `padding:10px` (snake.js:828) */

/* Button chrome. The original uses a real <button>; a 1px outline around the
 * label is the closest thing that does not pretend to be a widget. */
#define BUTTON_PAD_X 10
#define BUTTON_PAD_Y 4

/* ---- Strings (MECHANICS.md 8.5) ---------------------------------------- */

#define TXT_TITLE      "JavaScript Snake"
#define TXT_WELCOME    "Use the D-Pad or left stick to play."
#define TXT_START      "Press X to start."
/*
 * The original picks its difficulty from an HTML dropdown (`index.html:155-161`)
 * that has no Vita equivalent, so the welcome screen states the current mode and
 * names the button that changes it (MECHANICS.md 10, deviation 12). The name
 * comes from the mode table, so adding a mode stays a data change.
 */
#define TXT_DIFFICULTY "Difficulty: %s - SQUARE to change"
/*
 * Same shape as the difficulty line, for the same reason: the original picks
 * its theme from a dropdown (`index.html:90-104`) with no Vita equivalent.
 * Kept this short deliberately - the content column is DIALOG_CONTENT_W (300px)
 * and "Theme: Original by DylanLCrocker - TRIANGLE to change" does not fit at
 * 14px, so the author goes on its own line below (PLAN-THEMES.md 6).
 */
#define TXT_THEME      "Theme: %s - TRIANGLE to change"
#define TXT_CREDIT     "Based on JavaScript Snake by Patrick Gillespie, MIT licensed."
/* These are other people's contributions to an MIT project and the reference
 * credits each by name in its dropdown, so this port does too. */
#define TXT_THEME_BY   "%s theme by %s"
#define TXT_DIED       "You died :("
#define TXT_WON        "You win! :D"
#define TXT_AGAIN      "Play Again?"
#define TXT_PRESS_X    "Press X"
#define TXT_PAUSED     "[Paused]"
#define TXT_UNPAUSE    "Press START to unpause."

/* ---- Theme table -------------------------------------------------------- */

#define RGB(r, g, b) { (r), (g), (b), 0xFF }

static const Theme g_themes[] = {
    {
        "Main", "patorjk",      /* main-snake.css                       */
        RGB(0xFC, 0x54, 0x54), /* background    body #fc5454            */
        RGB(0x00, 0x00, 0xA8), /* playfield     .snake-playing-field    */
        RGB(0xFC, 0xFC, 0x54), /* snake         snakeblock.png interior */
        RGB(0xC0, 0xC0, 0xC0), /* snake_dead    deadblock.png interior  */
        RGB(0xFF, 0x00, 0x00), /* food          .snake-food-block       */
        RGB(0xFF, 0xFF, 0xFF), /* hud_text      .snake-panel-component  */
        RGB(0x00, 0x00, 0x00), /* overlay_bg    dialog background       */
        RGB(0xFF, 0xFF, 0xFF), /* overlay_text  :61-63 welcome          */
        RGB(0xFF, 0xFF, 0xFF), /* ..._text_end  :67-70 try-again/win    */
        RGB(0x00, 0x00, 0x00), /* pause_bg      :28                     */
        RGB(0xFF, 0xFF, 0xFF), /* pause_text    :29                     */
        RGB(0xFF, 0xFF, 0xFF)  /* button_border                         */
    },
    {
        "Matrix", "Geahad Haymor", /* matrix-snake.css                  */
        RGB(0x00, 0xFF, 0x11), /* background    :1 body                 */
        RGB(0x00, 0x00, 0x00), /* playfield     :54                     */
        RGB(0x00, 0xC8, 0x48), /* snake         matrix-snake-block.png  */
        RGB(0xC0, 0xC0, 0xC0), /* snake_dead    shared deadblock.png    */
        RGB(0xE8, 0x00, 0x15), /* food          matrix-food-block.png   */
        RGB(0x00, 0x00, 0x00), /* hud_text      :29 panel               */
        RGB(0x00, 0x00, 0x00), /* overlay_bg    :63 welcome background  */
        RGB(0x00, 0xFF, 0x11), /* overlay_text  :63 welcome color       */
        RGB(0xFF, 0x00, 0x00), /* ..._text_end  :69 try-again/win color */
        RGB(0x00, 0x00, 0x00), /* pause_bg      :25                     */
        RGB(0xFF, 0xFF, 0xFF), /* pause_text    :26 white, not the green*/
        RGB(0x00, 0xFF, 0x11)  /* button_border                         */
    },
    {
        "Original", "DylanLCrocker", /* blue-snake.css                  */
        RGB(0x00, 0x46, 0x20), /* background    :1 rgb(0,70,32)         */
        RGB(0x14, 0x9C, 0x36), /* playfield     :58 rgb(20,156,54)      */
        RGB(0xFC, 0xFC, 0x54), /* snake         shared snakeblock.png   */
        RGB(0xC0, 0xC0, 0xC0), /* snake_dead    shared deadblock.png    */
        RGB(0xCF, 0x21, 0x21), /* food          :52 rgb(207,33,33)      */
        RGB(0xFF, 0xFF, 0xFF), /* hud_text      :23 panel               */
        RGB(0x00, 0x00, 0x00), /* overlay_bg    :63 welcome background  */
        RGB(0xFF, 0xFF, 0xFF), /* overlay_text  :63 welcome color       */
        RGB(0xFF, 0xFF, 0xFF), /* ..._text_end  :69 try-again/win color */
        RGB(0x00, 0x46, 0x20), /* pause_bg      :19 not black           */
        RGB(0xFF, 0xFF, 0xFF), /* pause_text    :20                     */
        RGB(0xFF, 0xFF, 0xFF)  /* button_border                         */
    },
    /*
     * The first theme with no stylesheet behind it. The three above are
     * transcriptions and every column cites the line it came from; this one is
     * original to this port, so its citation is PLAN-THEMES.md 12 and nothing
     * else. It is recorded that way rather than dressed up as a fourth
     * transcription, because a reader who goes looking for vita-snake.css must
     * not find a plausible-looking lie.
     *
     * What is borrowed is the structure: which selector feeds which field is
     * blue-snake.css's, already transcribed once as Original, so the mapping
     * below is the proven one and only the colours are new. The cited lines are
     * that file's, naming the role each colour plays, not its value.
     */
    {
        "Vita", "largepelotas",
        RGB(0x0B, 0x5F, 0xA5), /* background    :2  LiveArea blue       */
        RGB(0x01, 0x20, 0x3F), /* playfield     :59 deep navy           */
        RGB(0xFF, 0xFF, 0xFF), /* snake         :41 body block          */
        RGB(0xC0, 0xC0, 0xC0), /* snake_dead    shared deadblock.png    */
        RGB(0xE4, 0x37, 0x3E), /* food          :53 PlayStation red     */
        RGB(0xFF, 0xFF, 0xFF), /* hud_text      :26 panel               */
        RGB(0x00, 0x00, 0x00), /* overlay_bg    :64 welcome background  */
        RGB(0xFF, 0xFF, 0xFF), /* overlay_text  :65 welcome color       */
        RGB(0xFF, 0xFF, 0xFF), /* ..._text_end  :72 try-again/win color */
        RGB(0x0B, 0x5F, 0xA5), /* pause_bg      :19 body colour, as blue*/
        RGB(0xFF, 0xFF, 0xFF), /* pause_text    :20                     */
        RGB(0xFF, 0xFF, 0xFF)  /* button_border                         */
    }
};

int theme_count(void)
{
    return (int)(sizeof g_themes / sizeof g_themes[0]);
}

const Theme *theme_get(int index)
{
    if (index < 0) {
        index = 0;
    }
    if (index >= theme_count()) {
        index = theme_count() - 1;
    }
    return &g_themes[index];
}

int theme_advance(int index, int delta)
{
    int n = theme_count();
    int t;

    /* Reduce first so a large delta cannot overflow the sum, then bias by n
     * because C's % keeps the sign of its left operand. */
    t = (index % n + delta % n) % n;
    if (t < 0) {
        t += n;
    }
    return t;
}

/* ---- Helpers ------------------------------------------------------------ */

bool render_init(RenderCtx *rc, int theme)
{
    const char *path;

    rc->font  = NULL;
    rc->small = NULL;
    rc->theme = theme;

    if (!text_init()) {
        return false;
    }

    /* plat_asset_path reuses one static buffer, so the 14px font must be open
     * before the path for the 10px one is asked for. */
    path = plat_asset_path("font.ttf");
    rc->font = text_open(path, 14); /* MECHANICS.md 8.4: 14px body text */
    if (!rc->font) {
        return false;
    }

    path = plat_asset_path("font.ttf");
    rc->small = text_open(path, 10); /* PLAN.md 1.5: credit in small text */
    if (!rc->small) {
        text_close(rc->font);
        rc->font = NULL;
        return false;
    }
    return true;
}

void render_shutdown(RenderCtx *rc)
{
    text_close(rc->font);
    text_close(rc->small);
    rc->font  = NULL;
    rc->small = NULL;
    text_shutdown();
}

static void set_color(SDL_Renderer *r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fill(SDL_Renderer *r, SDL_Color c, int x, int y, int w, int h)
{
    SDL_Rect rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    set_color(r, c);
    SDL_RenderFillRect(r, &rect);
}

static void outline(SDL_Renderer *r, SDL_Color c, int x, int y, int w, int h)
{
    SDL_Rect rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    set_color(r, c);
    SDL_RenderDrawRect(r, &rect);
}

/*
 * A button: outlined box with the label centred inside. Returns its height.
 *
 * The label colour is passed in rather than taken from the theme because CSS
 * `color` inherits: a button inside the end-game dialog is that dialog's
 * colour, which in Matrix is red where the welcome dialog is green
 * (matrix-snake.css:63 vs :69).
 */
static int draw_button(SDL_Renderer *r, RenderCtx *rc, const Theme *th,
                       const char *label, int cx, int y, SDL_Color text)
{
    int tw = text_width(rc->font, label);
    int th_ = text_line_height(rc->font);
    int w   = tw + 2 * BUTTON_PAD_X;
    int h   = th_ + 2 * BUTTON_PAD_Y;

    outline(r, th->button_border, cx - w / 2, y, w, h);
    text_draw_center(r, rc->font, label, cx, y + BUTTON_PAD_Y, text);
    return h;
}

/* ---- Board ------------------------------------------------------------- */

static void draw_playfield(SDL_Renderer *r, const Theme *th, const GameState *g)
{
    int i, n;

    fill(r, th->playfield, FIELD_X, FIELD_Y, FIELD_W(g), FIELD_H(g));

    if (g->has_food) {
        fill(r, th->food, CELL_X(g->food.col), CELL_Y(g->food.row), BLOCK,
             BLOCK);
    }

    /* MECHANICS.md 8.2: on death only the HEAD turns grey; the body stays
     * yellow. MECHANICS.md 8.3: while alive the head is not distinguished at
     * all in the main theme. */
    n = snake_occupied(&g->snake);
    for (i = 0; i < n; i++) {
        Cell      c   = snake_cell_at(&g->snake, i);
        bool      is_head = (i == n - 1);
        SDL_Color col = (is_head && g->state == STATE_DEAD) ? th->snake_dead
                                                            : th->snake;

        fill(r, col, CELL_X(c.col) + SNAKE_INSET, CELL_Y(c.row) + SNAKE_INSET,
             BLOCK - 2 * SNAKE_INSET, BLOCK - 2 * SNAKE_INSET);
    }
}

static void draw_hud(SDL_Renderer *r, RenderCtx *rc, const Theme *th,
                     const GameState *g)
{
    char buf[64];

    int top = HUD_PANEL_TOP(g) + HUD_PAD;

    snprintf(buf, sizeof buf, "Length: %d", game_length(g));
    text_draw(r, rc->font, buf, LENGTH_LEFT + HUD_PAD, top, th->hud_text);

    snprintf(buf, sizeof buf, "Highscore: %d", g->highscore);
    text_draw(r, rc->font, buf, HIGHSCORE_LEFT + HUD_PAD, top, th->hud_text);
}

/* ---- Overlays (MECHANICS.md 8.5) --------------------------------------- */

/*
 * Welcome dialog. Height is auto in the original, so the box is sized from its
 * wrapped content: title, paragraph break, instructions, difficulty, theme,
 * paragraph break, button, then the derivative-work credit and the theme's own
 * credit (PLAN.md 1.5, PLAN-THEMES.md 6).
 *
 * The box grows on its own because content_h is computed from the same parts
 * that are then drawn - the two lines this gained cost one term each here and
 * nothing in MECHANICS 9's geometry, which fixes only the width.
 */
static void draw_welcome(SDL_Renderer *r, RenderCtx *rc, const Theme *th,
                         const GameState *g)
{
    int line   = text_line_height(rc->font);
    int small  = text_line_height(rc->small);
    int body_h = text_draw_wrapped(NULL, rc->font, TXT_WELCOME, 0, 0,
                                   DIALOG_CONTENT_W, th->overlay_text);
    int button_h = text_line_height(rc->font) + 2 * BUTTON_PAD_Y;
    int content_h = line          /* title             */
                    + line        /* paragraph break   */
                    + body_h
                    + line        /* difficulty        */
                    + line        /* theme             */
                    + line        /* paragraph break   */
                    + button_h
                    + small       /* gap before credit */
                    + small       /* credit            */
                    + small;      /* theme credit      */
    int y = WELCOME_TOP + DIALOG_PAD;
    char difficulty[64];
    char theme_line[64];
    char theme_credit[96];

    snprintf(difficulty, sizeof difficulty, TXT_DIFFICULTY,
             mode_get(g->mode)->name);
    snprintf(theme_line, sizeof theme_line, TXT_THEME, th->name);
    snprintf(theme_credit, sizeof theme_credit, TXT_THEME_BY, th->name,
             th->author);

    fill(r, th->overlay_bg, DIALOG_LEFT, WELCOME_TOP, DIALOG_BOX_W,
         content_h + 2 * DIALOG_PAD);

    text_draw_center(r, rc->font, TXT_TITLE, CENTER_X, y, th->overlay_text);
    y += line + line;
    y += text_draw_wrapped(r, rc->font, TXT_WELCOME, CENTER_X, y,
                           DIALOG_CONTENT_W, th->overlay_text);
    text_draw_center(r, rc->font, difficulty, CENTER_X, y, th->overlay_text);
    y += line;
    text_draw_center(r, rc->font, theme_line, CENTER_X, y, th->overlay_text);
    y += line + line;
    y += draw_button(r, rc, th, TXT_START, CENTER_X, y, th->overlay_text);
    y += small;
    text_draw_center(r, rc->small, TXT_CREDIT, CENTER_X, y, th->overlay_text);
    y += small;
    text_draw_center(r, rc->small, theme_credit, CENTER_X, y, th->overlay_text);
}

/* Death and win share one element in the original (`snake.js:966-969`): title,
 * message, then a "Play Again?" button. The box is a fixed 300x100 + padding. */
static void draw_endgame(SDL_Renderer *r, RenderCtx *rc, const Theme *th,
                         const char *message)
{
    int line = text_line_height(rc->font);
    int y    = ENDGAME_TOP + DIALOG_PAD;

    fill(r, th->overlay_bg, DIALOG_LEFT, ENDGAME_TOP, DIALOG_BOX_W,
         ENDGAME_BOX_H);

    text_draw_center(r, rc->font, TXT_TITLE, CENTER_X, y, th->overlay_text_end);
    y += line + line;
    text_draw_center(r, rc->font, message, CENTER_X, y, th->overlay_text_end);
    y += line + line / 2;
    text_draw_center(r, rc->font, TXT_AGAIN, CENTER_X, y, th->overlay_text_end);
    y += line;
    draw_button(r, rc, th, TXT_PRESS_X, CENTER_X, y, th->overlay_text_end);
}

/* 300x80 box, its inner div padded 10px (MECHANICS.md 8.4). */
static void draw_paused(SDL_Renderer *r, RenderCtx *rc, const Theme *th)
{
    int line = text_line_height(rc->font);
    int y    = PAUSE_TOP + PAUSE_PAD;

    fill(r, th->pause_bg, PAUSE_LEFT, PAUSE_TOP, PAUSE_W, PAUSE_H);
    text_draw_center(r, rc->font, TXT_PAUSED, CENTER_X, y, th->pause_text);
    y += line + line / 2;
    text_draw_center(r, rc->font, TXT_UNPAUSE, CENTER_X, y, th->pause_text);
}

/* ---- Button diagnostic (PLAN.md 6.5) ----------------------------------- */

/*
 * Lists every held button index and, for the ones the game binds, what the
 * platform table claims that index is. On hardware the two can disagree, and
 * this panel is how that gets found: press Cross, read the index, compare.
 */
void render_diagnostic(SDL_Renderer *r, RenderCtx *rc, uint32_t buttons)
{
    static const struct {
        PadButton   pad;
        const char *name;
    } named[] = {
        { PAD_CONFIRM,    "Cross"  }, { PAD_BACK,  "Circle" },
        { PAD_PAUSE,      "Start"  }, { PAD_UP,    "Up"     },
        { PAD_DOWN,       "Down"   }, { PAD_LEFT,  "Left"   },
        { PAD_RIGHT,      "Right"  }, { PAD_L,     "L"      },
        { PAD_R,          "R"      }, { PAD_CYCLE_MODE, "Square" }
    };
    const Theme *th   = theme_get(rc->theme);
    int          line = text_line_height(rc->font);
    int          x    = 40;
    int          y    = 40;
    char         buf[128];
    int          i;
    unsigned     bit;

    fill(r, th->overlay_bg, x - 10, y - 10, 420, line * 6 + 20);
    text_draw(r, rc->font, "Button index diagnostic (hold L+R)", x, y,
              th->overlay_text);
    y += line + line / 2;

    buf[0] = '\0';
    for (bit = 0; bit < 32u; bit++) {
        if (buttons & (1u << bit)) {
            char one[8];

            snprintf(one, sizeof one, "%u ", bit);
            SDL_strlcat(buf, one, sizeof buf);
        }
    }
    if (buf[0] == '\0') {
        SDL_strlcpy(buf, "(none)", sizeof buf);
    }
    text_draw(r, rc->font, "held indices:", x, y, th->overlay_text);
    text_draw(r, rc->font, buf, x + 130, y, th->overlay_text);
    y += line;

    /* What this build believes those indices mean. Two lines, so a wrong table
     * is visible next to the truth rather than a screen away from it. */
    buf[0] = '\0';
    for (i = 0; i < (int)(sizeof named / sizeof named[0]); i++) {
        int index = plat_button_for(named[i].pad);
        char one[32];

        if (index < 0 || !(buttons & (1u << (unsigned)index))) {
            continue;
        }
        snprintf(one, sizeof one, "%s=%d ", named[i].name, index);
        SDL_strlcat(buf, one, sizeof buf);
    }
    if (buf[0] == '\0') {
        SDL_strlcpy(buf, "(no bound button held)", sizeof buf);
    }
    text_draw(r, rc->font, "this build:", x, y, th->overlay_text);
    text_draw(r, rc->font, buf, x + 130, y, th->overlay_text);
    y += line + line / 2;

    text_draw(r, rc->small,
              "Press each button and report the index it shows.", x, y,
              th->overlay_text);
    y += line;

    /* Says out loud what input.c enforces: nothing acts while this is up, so
     * Cross can be probed without starting a game (TESTPLAN item 4). */
    text_draw(r, rc->small,
              "Buttons only report while L+R are held; they do not act.", x, y,
              th->overlay_text);
}

/* ---- Frame -------------------------------------------------------------- */

void render_frame(SDL_Renderer *r, RenderCtx *rc, const GameState *g)
{
    const Theme *th = theme_get(rc->theme);

    set_color(r, th->background);
    SDL_RenderClear(r);

    draw_playfield(r, th, g);
    draw_hud(r, rc, th, g);

    /* The original leaves the board visible behind every dialog. */
    switch (g->state) {
    case STATE_WELCOME:
        draw_welcome(r, rc, th, g);
        break;
    case STATE_PAUSED:
        draw_paused(r, rc, th);
        break;
    case STATE_DEAD:
        draw_endgame(r, rc, th, TXT_DIED);
        break;
    case STATE_WON:
        draw_endgame(r, rc, th, TXT_WON);
        break;
    case STATE_READY:
    case STATE_PLAYING:
        break;
    }
}
