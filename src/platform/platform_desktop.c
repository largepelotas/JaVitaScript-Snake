/*
 * Desktop implementation of the platform interface (PLAN.md 6.1).
 *
 * The Vita counterpart is Phase 4. Everything here is deliberately boring: the
 * point of the split is that the interesting code in src/shell/ and src/core/
 * is byte-identical on both targets, so what is verified on the host is what
 * ships.
 */
#include "platform.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define PATH_MAX_LEN 512

static char  g_storage[PATH_MAX_LEN];
static char  g_asset[PATH_MAX_LEN];
static char *g_pref_path;

/*
 * Desktop joystick mapping.
 *
 * Directions are absent on purpose: an XInput-style pad reports its d-pad as a
 * hat, not as buttons, so src/shell/input.c reads the hat and the analog axes
 * directly. The face-button indices below are the usual SDL layout for such a
 * pad, and they matter little here because the keyboard is the primary desktop
 * input - the table exists so that Phase 4 has exactly one place to correct
 * (PLAN.md 6.5) and so the shell never sees a raw index.
 */
static const int g_buttons[PAD_COUNT] = {
    -1, /* PAD_UP      - hat/axis */
    -1, /* PAD_DOWN    - hat/axis */
    -1, /* PAD_LEFT    - hat/axis */
    -1, /* PAD_RIGHT   - hat/axis */
    0,  /* PAD_CONFIRM    - A */
    1,  /* PAD_BACK       - B */
    7,  /* PAD_PAUSE      - Start */
    2,  /* PAD_CYCLE_MODE  - X, which sits where the Vita's square does */
    3,  /* PAD_CYCLE_THEME - Y, which sits where the Vita's triangle does */
    4,  /* PAD_L           - left shoulder  */
    5   /* PAD_R           - right shoulder */
};

void plat_init(void)
{
    g_pref_path = SDL_GetPrefPath("vita-snake", "snake");
    if (g_pref_path) {
        snprintf(g_storage, sizeof g_storage, "%s", g_pref_path);
    } else {
        /* SDL_GetPrefPath can fail on a read-only or odd HOME; the current
         * directory is a usable fallback on desktop and keeps the caller from
         * having to handle a NULL storage dir. */
        snprintf(g_storage, sizeof g_storage, "./");
    }
}

void plat_shutdown(void)
{
    if (g_pref_path) {
        SDL_free(g_pref_path);
        g_pref_path = NULL;
    }
}

const char *plat_storage_dir(void)
{
    if (g_storage[0] == '\0') {
        snprintf(g_storage, sizeof g_storage, "./");
    }
    return g_storage;
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

const char *plat_asset_path(const char *name)
{
    char *base;

    /* Run from the repo root (the usual case), from the build directory, or
     * from an installed layout beside the binary. First hit wins. */
    snprintf(g_asset, sizeof g_asset, "assets/%s", name);
    if (file_exists(g_asset)) {
        return g_asset;
    }

    base = SDL_GetBasePath();
    if (base) {
        snprintf(g_asset, sizeof g_asset, "%sassets/%s", base, name);
        if (file_exists(g_asset)) {
            SDL_free(base);
            return g_asset;
        }
        snprintf(g_asset, sizeof g_asset, "%s../assets/%s", base, name);
        if (file_exists(g_asset)) {
            SDL_free(base);
            return g_asset;
        }
        SDL_free(base);
    }

    /* Nothing found: hand back the plain relative path so the caller reports a
     * failure against a name a human can act on. */
    snprintf(g_asset, sizeof g_asset, "assets/%s", name);
    return g_asset;
}

bool plat_read_file(const char *path, void *buf, size_t n)
{
    FILE  *f;
    size_t got;

    f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    got = fread(buf, 1, n, f);
    fclose(f);
    return got == n;
}

bool plat_write_file(const char *path, const void *buf, size_t n)
{
    FILE  *f;
    size_t put;

    f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    put = fwrite(buf, 1, n, f);
    if (fclose(f) != 0) {
        return false;
    }
    return put == n;
}

void plat_log(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

uint32_t plat_seed(void)
{
    /* The core is forbidden from reading a clock (PLAN.md 4.2). This is the one
     * place entropy enters, and it is a platform concern precisely so that a
     * headless replay can bypass it and stay reproducible. */
    Uint64 c = SDL_GetPerformanceCounter();
    return (uint32_t)(c ^ (c >> 32)) ^ SDL_GetTicks();
}

int plat_button_for(PadButton b)
{
    if ((unsigned)b >= (unsigned)PAD_COUNT) {
        return -1;
    }
    return g_buttons[b];
}
