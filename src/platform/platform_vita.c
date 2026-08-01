/*
 * Vita implementation of the platform interface (PLAN.md 6.1).
 *
 * Everything the Vita does differently lives here and nowhere else: the heap
 * size, ux0: paths, the log sink, the seed source, and the SDL joystick button
 * indices. If a target difference ever needs an #ifdef somewhere else, the
 * abstraction is wrong (PLAN.md 1.2).
 *
 * Nothing in this file can be executed on the host, which is exactly why it is
 * this small and why the rest of the program does not import its assumptions.
 */
#ifndef __vita__
#error "platform_vita.c is for the Vita target; the host builds platform_desktop.c"
#endif

#include "platform.h"

#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>

#include <stdio.h>
#include <string.h>

/*
 * SDL2 on the Vita fails at init if the newlib heap is left at its default
 * (PLAN.md 6.2). This symbol is read by the CRT before main runs.
 */
#ifdef __vita__
unsigned int _newlib_heap_size_user = 192 * 1024 * 1024;
#endif

#define STORAGE_DIR "ux0:data/VitaSnake/"
#define LOG_PATH    STORAGE_DIR "log.txt"
#define ASSET_DIR   "app0:assets/"

#define PATH_MAX_LEN 512

static char  g_asset[PATH_MAX_LEN];
static FILE *g_log;

/*
 * Logical button -> SDL joystick button index.
 *
 * These indices are NOT guessed and NOT copied from a forum post: they were
 * read out of the very libSDL2.a this build links against, by disassembling
 * VITA_JoystickUpdate and pairing each SCE_CTRL bit it tests with the index it
 * passes to SDL_PrivateJoystickButton. Cross-checked against the bit values in
 * $VITASDK/arm-vita-eabi/include/psp2common/ctrl.h. The full table:
 *
 *   0 triangle   4 L1     8 up        12 L2 / left trigger
 *   1 circle     5 R1     9 right     13 R2 / right trigger
 *   2 cross      6 down  10 select    14 L3
 *   3 square     7 left  11 start     15 R3
 *
 * The driver polls with sceCtrlPeekBufferPositive2, which binds the physical
 * L/R shoulder buttons to L1/R1 rather than to the trigger bits, so a Vita's
 * shoulders arrive as indices 4 and 5 - which is what PAD_L and PAD_R use.
 *
 * This is strong evidence, not confirmation: it is still the binary's opinion
 * of its own behavior, not an observation of hardware (PLAN.md 6.5, 11.1).
 * The L+R diagnostic on the welcome screen exists to settle it, and correcting
 * this table is a one-line change if hardware confirms a different index.
 */
static const int g_buttons[PAD_COUNT] = {
    8,  /* PAD_UP      */
    6,  /* PAD_DOWN    */
    7,  /* PAD_LEFT    */
    9,  /* PAD_RIGHT   */
    2,  /* PAD_CONFIRM    - cross  */
    1,  /* PAD_BACK       - circle */
    11, /* PAD_PAUSE      - start  */
    3,  /* PAD_CYCLE_MODE - square */
    4,  /* PAD_L          - L1     */
    5   /* PAD_R          - R1     */
};

void plat_init(void)
{
    int rc;

    /* ux0:data exists on every system, but our subdirectory does not until we
     * make it, and a first-run save must not be the thing that discovers that
     * (PLAN.md 6.3). An existing directory reports an error, which is fine. */
    rc = sceIoMkdir("ux0:data/VitaSnake", 0777);

    g_log = fopen(LOG_PATH, "a");
    plat_log("--- vita-snake start ---");
    plat_log("storage: %s (mkdir rc 0x%08X)", STORAGE_DIR, (unsigned)rc);
}

void plat_shutdown(void)
{
    if (g_log) {
        plat_log("--- vita-snake exit ---");
        fclose(g_log);
        g_log = NULL;
    }
}

const char *plat_storage_dir(void)
{
    return STORAGE_DIR;
}

const char *plat_asset_path(const char *name)
{
    snprintf(g_asset, sizeof g_asset, "%s%s", ASSET_DIR, name);
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

/*
 * The only debugging channel that exists on hardware (PLAN.md 6.3), so it is
 * flushed after every line: a log that is still buffered when the game locks up
 * tells us nothing about why it locked up.
 */
void plat_log(const char *fmt, ...)
{
    va_list ap;
    char    line[512];

    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);

    if (g_log) {
        fputs(line, g_log);
        fputc('\n', g_log);
        fflush(g_log);
    }
    /* Also to the debug console, which is visible under Vita3K and over a
     * PSVita debug link but not on a bare handheld. */
    sceClibPrintf("%s\n", line);
}

uint32_t plat_seed(void)
{
    /* Microseconds since process start. The core never reads a clock
     * (PLAN.md 4.2); this is the one place entropy enters the program. */
    SceUInt64 us = sceKernelGetProcessTimeWide();

    return (uint32_t)(us ^ (us >> 32));
}

int plat_button_for(PadButton b)
{
    if ((unsigned)b >= (unsigned)PAD_COUNT) {
        return -1;
    }
    return g_buttons[b];
}
