/*
 * The target-difference interface (PLAN.md 6.1).
 *
 * This header is the entire contract between the shell and the two targets.
 * Every implementation of it lives in exactly one file per target:
 *
 *     platform_desktop.c   host builds (Phase 3)
 *     platform_vita.c      the Vita (Phase 4)
 *
 * PLAN.md 1.2: `#ifdef __vita__` is legal ONLY inside src/platform/. If the
 * shell ever needs to know which target it is on, that is a missing function
 * here, not a conditional over there.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Logical buttons. The shell speaks only in these; the mapping to SDL joystick
 * indices is a per-target table behind plat_button_for (PLAN.md 6.5).
 *
 * PAD_L / PAD_R exist for the hidden button-index diagnostic that Phase 4 adds
 * to the welcome screen; nothing in the game binds them.
 */
typedef enum {
    PAD_UP = 0,
    PAD_DOWN,
    PAD_LEFT,
    PAD_RIGHT,
    PAD_CONFIRM,    /* Cross  */
    PAD_BACK,       /* Circle */
    PAD_PAUSE,      /* Start  */
    PAD_CYCLE_MODE, /* Square: next difficulty on the welcome screen */
    PAD_L,
    PAD_R,
    PAD_COUNT
} PadButton;

void plat_init(void);
void plat_shutdown(void);

/* Writable directory for save data, with a trailing slash. Never NULL. */
const char *plat_storage_dir(void);

/*
 * Absolute (or process-relative) path to a packaged read-only asset, e.g.
 * "font.ttf". The returned pointer is to a static buffer that the next call
 * overwrites - copy it if you need to keep it (PLAN.md 6.4).
 */
const char *plat_asset_path(const char *name);

/*
 * Exactly n bytes are read or written, or the call fails. Partial reads are
 * reported as failure so a truncated save file can never be mistaken for a
 * valid one (PLAN.md 6.3: never trust the file).
 */
bool plat_read_file(const char *path, void *buf, size_t n);
bool plat_write_file(const char *path, const void *buf, size_t n);

/* On the Vita this is the only debugging channel there is (PLAN.md 6.3). */
void plat_log(const char *fmt, ...);

/*
 * The one deliberately non-deterministic value in the program. The core never
 * reads a clock (PLAN.md 4.2); the seed enters here and nowhere else, which is
 * what lets a replay reproduce a session exactly.
 */
uint32_t plat_seed(void);

/* SDL joystick button index for a logical button, or -1 if unmapped. */
int plat_button_for(PadButton b);

#endif /* PLATFORM_H */
