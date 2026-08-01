/*
 * LiveArea asset generator (PLAN.md 7.3).
 *
 * Writes the three sce_sys images from the game's own palette so the bubble
 * and the LiveArea look like the thing they launch. Everything here is drawn
 * with the same rule src/shell/render.c uses for a snake segment - a block in
 * the playfield color with an inset interior - so the motif is the game's, not
 * a lookalike.
 *
 * The images are 8-bit INDEXED PNGs, and that is not a size optimisation.
 * Writing them as RGBA made VitaShell fail the install on hardware with
 * 0x8010113D at the end of the copy, while Vita3K installed and ran the same
 * .vpk happily - the emulator's PNG decoder is permissive and the real
 * firmware's is not. Every icon0/bg/startup in $VITASDK/share/.../samples is
 * colour type 3, which is the shape the firmware accepts. So this tool writes
 * PNGs itself against a small explicit palette rather than going through
 * IMG_SavePNG, which always emits truecolour.
 *
 * Host-only, like tools/bmp2png.c; nothing that ships depends on it.
 *
 * Build:  make livearea
 * Usage:  gen_livearea <sce_sys-dir> <font.ttf>
 */
#include <SDL.h>
#include <SDL_ttf.h>
#include <zlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Palette ------------------------------------------------------------ */

/*
 * Indices into the PNG palette. The three flat colours are MECHANICS.md 8,
 * the same values as src/shell/render.c's theme table. The rest is a ramp from
 * the playfield colour to white, which is what antialiased text needs: an
 * indexed PNG has no alpha channel to blend with, so the blend is baked in and
 * quantised to RAMP steps. 32 is well past the point the steps are visible.
 */
#define I_PLAYFIELD 0
#define I_SNAKE     1
#define I_FOOD      2
#define I_RAMP      3
#define RAMP        32
#define PAL_N       (I_RAMP + RAMP)

typedef struct {
    unsigned char r, g, b;
} Rgb;

static const Rgb C_PLAYFIELD = { 0x00, 0x00, 0xA8 };
static const Rgb C_SNAKE     = { 0xFC, 0xFC, 0x54 };
static const Rgb C_FOOD      = { 0xFF, 0x00, 0x00 };
static const Rgb C_TEXT      = { 0xFF, 0xFF, 0xFF };

static void build_palette(Rgb pal[PAL_N])
{
    int i;

    pal[I_PLAYFIELD] = C_PLAYFIELD;
    pal[I_SNAKE]     = C_SNAKE;
    pal[I_FOOD]      = C_FOOD;

    for (i = 0; i < RAMP; i++) {
        double t = (double)i / (RAMP - 1);
        pal[I_RAMP + i].r =
            (unsigned char)(C_PLAYFIELD.r + t * (C_TEXT.r - C_PLAYFIELD.r));
        pal[I_RAMP + i].g =
            (unsigned char)(C_PLAYFIELD.g + t * (C_TEXT.g - C_PLAYFIELD.g));
        pal[I_RAMP + i].b =
            (unsigned char)(C_PLAYFIELD.b + t * (C_TEXT.b - C_PLAYFIELD.b));
    }
}

/* ---- Indexed canvas ----------------------------------------------------- */

typedef struct {
    int            w, h;
    unsigned char *px; /* one palette index per pixel */
} Canvas;

static int canvas_init(Canvas *c, int w, int h, unsigned char bg)
{
    c->w  = w;
    c->h  = h;
    c->px = malloc((size_t)w * (size_t)h);
    if (!c->px) {
        fprintf(stderr, "out of memory for %dx%d canvas\n", w, h);
        return 1;
    }
    memset(c->px, bg, (size_t)w * (size_t)h);
    return 0;
}

static void canvas_free(Canvas *c)
{
    free(c->px);
    c->px = NULL;
}

static void fill(Canvas *c, int x, int y, int w, int h, unsigned char idx)
{
    int row, col;

    for (row = y; row < y + h; row++) {
        if (row < 0 || row >= c->h)
            continue;
        for (col = x; col < x + w; col++) {
            if (col < 0 || col >= c->w)
                continue;
            c->px[(size_t)row * (size_t)c->w + (size_t)col] = idx;
        }
    }
}

/* ---- PNG writing -------------------------------------------------------- */

static void be32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int chunk(FILE *f, const char *type, const unsigned char *data,
                 unsigned long len)
{
    unsigned char hdr[8], crcbuf[4];
    unsigned long crc;

    be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    if (fwrite(hdr, 1, 8, f) != 8)
        return 1;
    if (len && fwrite(data, 1, len, f) != len)
        return 1;

    crc = crc32(0L, (const Bytef *)type, 4);
    if (len)
        crc = crc32(crc, (const Bytef *)data, (uInt)len);
    be32(crcbuf, crc);
    return fwrite(crcbuf, 1, 4, f) != 4;
}

/*
 * Colour type 3, bit depth 8, no interlacing, filter type 0 on every row -
 * the plainest PNG that exists. The images are flat colour, so the filter
 * choice costs nothing and keeps this readable.
 */
static int write_png(const char *path, const Canvas *c, const Rgb pal[PAL_N])
{
    static const unsigned char SIG[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    unsigned char  ihdr[13], plte[PAL_N * 3];
    unsigned char *raw = NULL, *comp = NULL;
    uLongf         comp_len;
    size_t         raw_len;
    FILE          *f   = NULL;
    int            rc  = 1, i, row;

    raw_len = ((size_t)c->w + 1) * (size_t)c->h;
    raw     = malloc(raw_len);
    if (!raw)
        goto done;
    for (row = 0; row < c->h; row++) {
        raw[(size_t)row * ((size_t)c->w + 1)] = 0; /* filter: none */
        memcpy(raw + (size_t)row * ((size_t)c->w + 1) + 1,
               c->px + (size_t)row * (size_t)c->w, (size_t)c->w);
    }

    comp_len = compressBound((uLong)raw_len);
    comp     = malloc(comp_len);
    if (!comp)
        goto done;
    if (compress2(comp, &comp_len, raw, (uLong)raw_len, 9) != Z_OK) {
        fprintf(stderr, "zlib compress failed for %s\n", path);
        goto done;
    }

    be32(ihdr, (unsigned long)c->w);
    be32(ihdr + 4, (unsigned long)c->h);
    ihdr[8]  = 8; /* bit depth   */
    ihdr[9]  = 3; /* colour type: indexed */
    ihdr[10] = 0; /* compression */
    ihdr[11] = 0; /* filter      */
    ihdr[12] = 0; /* interlace   */

    for (i = 0; i < PAL_N; i++) {
        plte[i * 3 + 0] = pal[i].r;
        plte[i * 3 + 1] = pal[i].g;
        plte[i * 3 + 2] = pal[i].b;
    }

    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno));
        goto done;
    }
    if (fwrite(SIG, 1, 8, f) != 8)
        goto done;
    if (chunk(f, "IHDR", ihdr, sizeof ihdr))
        goto done;
    if (chunk(f, "PLTE", plte, sizeof plte))
        goto done;
    if (chunk(f, "IDAT", comp, comp_len))
        goto done;
    if (chunk(f, "IEND", NULL, 0))
        goto done;

    rc = 0;
done:
    if (f && fclose(f) != 0)
        rc = 1;
    free(raw);
    free(comp);
    if (rc)
        fprintf(stderr, "failed writing %s\n", path);
    return rc;
}

/* ---- The motif ---------------------------------------------------------- */

typedef struct {
    int col, row;
} Cell;

/*
 * A snake running right, turning up, and heading for a food block two cells
 * ahead of its head. It is deliberately compact - 9 cols x 6 rows - so the
 * whole thing, food included, fits inside all three canvases without cropping.
 * Every image draws this same path at its own scale and offset, so the bubble
 * and the background are visibly the same picture rather than three separate
 * drawings.
 *
 *     . . . . . . . . .
 *     . . . . . . . . .
 *     . . . . o o o . @      o body   @ food
 *     . . . . o . . . .
 *     o o o o o . . . .
 *     . . . . . . . . .
 */
#define MOTIF_COLS 9
#define MOTIF_ROWS 6
static const Cell BODY[] = {
    { 0, 4 }, { 1, 4 }, { 2, 4 }, { 3, 4 }, { 4, 4 },
    { 4, 3 },
    { 4, 2 }, { 5, 2 }, { 6, 2 },
};
static const int  BODY_N = (int)(sizeof BODY / sizeof BODY[0]);
static const Cell FOOD   = { 8, 2 };

/*
 * A segment is a `block`-sized cell whose interior is inset by `inset` px,
 * leaving the playfield colour showing through as the 1px gap the original's
 * snakeblock.png has at 20px (MECHANICS.md 8.1). The inset scales with the
 * block so the gap stays visible at 56px and does not swamp the colour at 14px.
 */
static void segment(Canvas *c, int ox, int oy, int block, int inset, Cell cell,
                    unsigned char idx)
{
    fill(c, ox + cell.col * block + inset, oy + cell.row * block + inset,
         block - 2 * inset, block - 2 * inset, idx);
}

static void draw_motif(Canvas *c, int ox, int oy, int block, int inset)
{
    int i;

    for (i = 0; i < BODY_N; i++)
        segment(c, ox, oy, block, inset, BODY[i], I_SNAKE);
    /* Food has border: 0 in the original - it is a flush block, no inset. */
    segment(c, ox, oy, block, 0, FOOD, I_FOOD);
}

/* Centers the motif in a canvas at a given block size. */
static void centered(int canvas_w, int canvas_h, int block, int *ox, int *oy)
{
    *ox = (canvas_w - MOTIF_COLS * block) / 2;
    *oy = (canvas_h - MOTIF_ROWS * block) / 2;
}

/* ---- Text --------------------------------------------------------------- */

/*
 * Blits antialiased text onto the canvas by mapping each pixel's coverage onto
 * the palette ramp. SDL renders it white-on-transparent; the ramp already runs
 * playfield-to-white, so alpha indexes it directly.
 */
static int draw_text(Canvas *c, const char *font_path, int px, const char *str,
                     int x, int y)
{
    TTF_Font    *font;
    SDL_Surface *text, *rgba;
    SDL_Color    white = { 0xFF, 0xFF, 0xFF, 0xFF };
    int          row, col;

    font = TTF_OpenFont(font_path, px);
    if (!font) {
        fprintf(stderr, "TTF_OpenFont(%s): %s\n", font_path, TTF_GetError());
        return 1;
    }
    text = TTF_RenderUTF8_Blended(font, str, white);
    TTF_CloseFont(font);
    if (!text) {
        fprintf(stderr, "TTF_RenderUTF8_Blended: %s\n", TTF_GetError());
        return 1;
    }

    rgba = SDL_ConvertSurfaceFormat(text, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(text);
    if (!rgba) {
        fprintf(stderr, "SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
        return 1;
    }

    for (row = 0; row < rgba->h; row++) {
        const Uint32 *src =
            (const Uint32 *)((const Uint8 *)rgba->pixels + row * rgba->pitch);
        for (col = 0; col < rgba->w; col++) {
            int           dx = x + col, dy = y + row;
            unsigned      a  = (src[col] >> 24) & 0xFF;
            unsigned char idx;

            if (a == 0 || dx < 0 || dx >= c->w || dy < 0 || dy >= c->h)
                continue;
            idx = (unsigned char)(I_RAMP + (a * (RAMP - 1) + 127) / 255);
            c->px[(size_t)dy * (size_t)c->w + (size_t)dx] = idx;
        }
    }

    SDL_FreeSurface(rgba);
    return 0;
}

/* Width of a string at a given size, so the caller can right-align it. */
static int text_width(const char *font_path, int px, const char *str)
{
    TTF_Font *font = TTF_OpenFont(font_path, px);
    int       w = 0, h = 0;

    if (!font)
        return 0;
    TTF_SizeUTF8(font, str, &w, &h);
    TTF_CloseFont(font);
    return w;
}

/* ---- The three images --------------------------------------------------- */

static int report(const char *path, const Canvas *c, int rc)
{
    if (rc == 0)
        printf("  %-44s %dx%d indexed\n", path, c->w, c->h);
    return rc;
}

/*
 * icon0.png - 128x128 bubble art. 14px blocks: 9 cols x 14 = 126, which is as
 * large as the motif goes without clipping the food. Block sizes stay integers
 * everywhere so the segments are pixel-aligned and the 1px gap survives.
 */
static int gen_icon0(const char *dir, const Rgb pal[PAL_N])
{
    Canvas c;
    char   path[512];
    int    ox, oy, rc;

    if (canvas_init(&c, 128, 128, I_PLAYFIELD))
        return 1;
    centered(128, 128, 14, &ox, &oy);
    draw_motif(&c, ox, oy, 14, 1);

    SDL_snprintf(path, sizeof path, "%s/icon0.png", dir);
    rc = report(path, &c, write_png(path, &c, pal));
    canvas_free(&c);
    return rc;
}

/*
 * startup.png - 280x158, the image on the LiveArea gate. 20px blocks: the
 * game's real block size (MECHANICS.md 2), so the gate is literally a window
 * onto a playfield at 1:1 rather than a scaled illustration of one.
 */
static int gen_startup(const char *dir, const Rgb pal[PAL_N])
{
    Canvas c;
    char   path[512];
    int    ox, oy, rc;

    if (canvas_init(&c, 280, 158, I_PLAYFIELD))
        return 1;
    centered(280, 158, 20, &ox, &oy);
    draw_motif(&c, ox, oy, 20, 1);

    SDL_snprintf(path, sizeof path, "%s/livearea/contents/startup.png", dir);
    rc = report(path, &c, write_png(path, &c, pal));
    canvas_free(&c);
    return rc;
}

/*
 * bg.png - 840x500 LiveArea background. The gate is drawn over this, and in
 * the "a1" style it occupies the lower left, so the motif is bottom-right
 * aligned with a 30px margin and the title sits top-right above it. 56px
 * blocks leave ~30px of clearance between the title and the motif's top row.
 */
static int gen_bg(const char *dir, const Rgb pal[PAL_N], const char *font_path)
{
    Canvas      c;
    char        path[512];
    const char *title = "SNAKE";
    int         rc;

    if (canvas_init(&c, 840, 500, I_PLAYFIELD))
        return 1;
    draw_motif(&c, 840 - MOTIF_COLS * 56 - 30, 500 - MOTIF_ROWS * 56 - 30,
               56, 3);

    if (draw_text(&c, font_path, 64, title,
                  840 - text_width(font_path, 64, title) - 48, 40)) {
        canvas_free(&c);
        return 1;
    }

    SDL_snprintf(path, sizeof path, "%s/livearea/contents/bg.png", dir);
    rc = report(path, &c, write_png(path, &c, pal));
    canvas_free(&c);
    return rc;
}

int main(int argc, char **argv)
{
    Rgb         pal[PAL_N];
    const char *dir, *font_path;
    int         rc = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <sce_sys-dir> <font.ttf>\n", argv[0]);
        return 2;
    }
    dir       = argv[1];
    font_path = argv[2];

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    build_palette(pal);

    rc |= gen_icon0(dir, pal);
    rc |= gen_startup(dir, pal);
    rc |= gen_bg(dir, pal, font_path);

    TTF_Quit();
    SDL_Quit();
    return rc ? 1 : 0;
}
