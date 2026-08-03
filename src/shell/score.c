/*
 * Save record.
 *
 * Layout, 16 bytes, little-endian by construction rather than by memcpy so the
 * file is identical on the host and on the Vita:
 *
 *   0..3   'V' 'S' 'N' 'K'
 *   4..7   meta: version in byte 4, mode in byte 5, theme in byte 6, byte 7 zero
 *   8..11  highscore
 *   12..15 (highscore XOR SCORE_CHECK) XOR meta
 *
 * Version 1 was highscore-only: byte 4 was 1, bytes 5..7 zero, and the checksum
 * covered the value alone. Those files are still read - a player who had a
 * highscore before difficulty was selectable keeps it - they just start at the
 * default mode. Version 2 is what is written from now on.
 *
 * Byte 6 became the theme with no version bump, because version 2 already
 * reserved it as zero and already folded it into the checksum (PLAN-THEMES.md
 * 5). That makes the format compatible in both directions: a record written
 * before themes existed has byte 6 = 0, which is Main and exactly the right
 * default, and a build without themes reading a themed record checksums the
 * same meta word and simply ignores the byte.
 *
 * The checksum's job is not security, it is catching the two failure modes that
 * actually happen on a handheld: a file of zeroes from a power cut mid-write,
 * and a file from a future format version. Folding meta into it means a flipped
 * mode byte is caught rather than silently changing the difficulty.
 */
#include "score.h"

#include "../core/modes.h"
#include "../core/snake_types.h"
#include "../platform/platform.h"
#include "render.h" /* theme_count, THEME_MAIN: the same relationship this file
                     * already has with modes.h, for the same reason - the
                     * record has to know what a valid value is */

#include <stdio.h>
#include <string.h>

#define SCORE_VERSION_LEGACY 1u /* highscore only, no mode */
#define SCORE_VERSION        2u
#define SCORE_CHECK          0x5A5A5A5Au
#define SCORE_FILE           "highscore.dat"

#define SCORE_DEFAULT_MODE MODE_MEDIUM /* the original's dropdown default */

static void score_path(char *buf, size_t n)
{
    snprintf(buf, n, "%s%s", plat_storage_dir(), SCORE_FILE);
}

static uint32_t get_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void put_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

void score_load(int *highscore, int *mode, int *theme)
{
    unsigned char rec[SCORE_RECORD_BYTES];
    char          path[512];
    uint32_t      meta, value, want_check;
    int           got_mode  = SCORE_DEFAULT_MODE;
    int           got_theme = THEME_MAIN;

    if (highscore) {
        *highscore = 0;
    }
    if (mode) {
        *mode = SCORE_DEFAULT_MODE;
    }
    if (theme) {
        *theme = THEME_MAIN;
    }

    score_path(path, sizeof path);
    if (!plat_read_file(path, rec, sizeof rec)) {
        return;
    }
    if (memcmp(rec, "VSNK", 4) != 0) {
        plat_log("score: bad magic in %s, starting from zero", path);
        return;
    }
    if (rec[4] != SCORE_VERSION && rec[4] != SCORE_VERSION_LEGACY) {
        plat_log("score: unknown version %u in %s, starting from zero",
                 (unsigned)rec[4], path);
        return;
    }

    meta  = get_u32(&rec[4]);
    value = get_u32(&rec[8]);

    /* A version 1 record's checksum covers the value alone; folding meta in
     * would reject every file written before difficulty was selectable. */
    want_check = value ^ SCORE_CHECK;
    if (rec[4] == SCORE_VERSION) {
        want_check ^= meta;
        got_mode  = (int)rec[5];
        got_theme = (int)rec[6];
    }

    if (want_check != get_u32(&rec[12])) {
        plat_log("score: checksum mismatch in %s, starting from zero", path);
        return;
    }

    /* A value no board can produce means the file is not ours, whatever the
     * header claims. MECHANICS.md 9: 1104 cells is the ceiling. */
    if (value > (uint32_t)SNAKE_MAX_CELLS) {
        plat_log("score: implausible value %u in %s, starting from zero",
                 (unsigned)value, path);
        return;
    }

    /* A mode outside the table is the one field a future build can legitimately
     * write - adding Impossible and Rush is meant to stay a data change
     * (PLAN.md 0.6) - so it falls back to the default without discarding the
     * highscore alongside it. */
    if (got_mode < 0 || got_mode >= mode_count()) {
        plat_log("score: mode %d in %s is not in this build, using %s",
                 got_mode, path, mode_get(SCORE_DEFAULT_MODE)->name);
        got_mode = SCORE_DEFAULT_MODE;
    }

    /* The same courtesy for the theme, and it is not hypothetical: a record
     * written by a build whose table still had Dark at index 1 would name a
     * theme this build does not have. Falling back to Main keeps the highscore
     * rather than discarding the file over a cosmetic byte. */
    if (got_theme < 0 || got_theme >= theme_count()) {
        plat_log("score: theme %d in %s is not in this build, using %s",
                 got_theme, path, theme_get(THEME_MAIN)->name);
        got_theme = THEME_MAIN;
    }

    if (highscore) {
        *highscore = (int)value;
    }
    if (mode) {
        *mode = got_mode;
    }
    if (theme) {
        *theme = got_theme;
    }
}

bool score_save(int highscore, int mode, int theme)
{
    unsigned char rec[SCORE_RECORD_BYTES];
    char          path[512];
    uint32_t      meta;

    if (highscore < 0) {
        highscore = 0;
    }
    if (mode < 0 || mode >= mode_count()) {
        mode = SCORE_DEFAULT_MODE;
    }
    if (theme < 0 || theme >= theme_count()) {
        theme = THEME_MAIN;
    }

    memset(rec, 0, sizeof rec);
    memcpy(rec, "VSNK", 4);
    rec[4] = (unsigned char)SCORE_VERSION;
    rec[5] = (unsigned char)mode;
    rec[6] = (unsigned char)theme;

    meta = get_u32(&rec[4]);
    put_u32(&rec[8], (uint32_t)highscore);
    put_u32(&rec[12], ((uint32_t)highscore ^ SCORE_CHECK) ^ meta);

    score_path(path, sizeof path);
    if (!plat_write_file(path, rec, sizeof rec)) {
        plat_log("score: could not write %s", path);
        return false;
    }
    return true;
}
