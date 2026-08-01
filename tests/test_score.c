/*
 * Save-record tests (PLAN.md 0.1, 6.3).
 *
 * The record is the one piece of state that outlives the process, so every way
 * it can be wrong has to be exercised here rather than discovered on hardware
 * with a lost highscore. That includes the format written by builds from before
 * difficulty was selectable: devices upgraded from before that version exist.
 *
 * The tests write real files through the real platform layer - a fake would
 * test the fake - but XDG_DATA_HOME is redirected first, so they cannot touch
 * a player's actual save.
 */
/* mkdtemp and setenv, which -std=c99 alone hides. Host-only test code: nothing
 * in src/ needs POSIX, and the Vita would not have it. */
#define _POSIX_C_SOURCE 200809L

#include "../src/shell/score.h"

#include "../src/core/modes.h"
#include "../src/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static char g_path[512];

static void record_path(void)
{
    snprintf(g_path, sizeof g_path, "%shighscore.dat", plat_storage_dir());
}

static void remove_record(void)
{
    remove(g_path);
}

/* Writes raw bytes over the record, for the corruption cases. */
static void write_raw(const unsigned char *bytes, size_t n)
{
    FILE *f = fopen(g_path, "wb");

    if (!f) {
        printf("  FAIL cannot write %s\n", g_path);
        failures++;
        return;
    }
    fwrite(bytes, 1, n, f);
    fclose(f);
}

static void put_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

static void test_missing_file(void)
{
    int highscore = 999, mode = 42;

    banner("no save file yet");

    remove_record();
    score_load(&highscore, &mode);
    CHECK(highscore == 0, "first run highscore %d, want 0", highscore);
    CHECK(mode == MODE_MEDIUM, "first run mode %d, want medium", mode);

    /* Either output may be NULL: the shell asks for what it needs. */
    score_load(NULL, NULL);
}

static void test_round_trip(void)
{
    int i;

    banner("round trip");

    for (i = 0; i < mode_count(); i++) {
        int highscore = -1, mode = -1;

        CHECK(score_save(146, i), "save failed for mode %d", i);
        score_load(&highscore, &mode);
        CHECK(highscore == 146, "highscore %d, want 146", highscore);
        CHECK(mode == i, "mode %d, want %d", mode, i);
    }

    /* A zero highscore is a real value, not an absent one. */
    {
        int highscore = -1, mode = -1;

        score_save(0, MODE_HARD);
        score_load(&highscore, &mode);
        CHECK(highscore == 0, "highscore %d, want 0", highscore);
        CHECK(mode == MODE_HARD, "mode %d, want hard", mode);
    }

    /* Out-of-range inputs are clamped on the way out, so a bad in-memory value
     * cannot poison the file. */
    {
        int highscore = -1, mode = -1;

        score_save(-5, 99);
        score_load(&highscore, &mode);
        CHECK(highscore == 0, "negative highscore stored as %d", highscore);
        CHECK(mode == MODE_MEDIUM, "out-of-range mode stored as %d", mode);
    }
}

/*
 * The format shipped before Square existed: version 1, no mode, checksum over
 * the value alone. Devices upgraded from that older build may have one. Losing the highscore on
 * upgrade would be a regression a hardware test might not even attribute
 * correctly, so it is pinned here.
 */
static void test_legacy_record(void)
{
    unsigned char rec[16];
    int           highscore = -1, mode = -1;

    banner("version 1 record still reads");

    memset(rec, 0, sizeof rec);
    memcpy(rec, "VSNK", 4);
    rec[4] = 1;
    put_u32(&rec[8], 271u);
    put_u32(&rec[12], 271u ^ 0x5A5A5A5Au);
    write_raw(rec, sizeof rec);

    score_load(&highscore, &mode);
    CHECK(highscore == 271, "legacy highscore %d, want 271", highscore);
    CHECK(mode == MODE_MEDIUM, "legacy record should default to medium, got %d",
          mode);
}

static void test_rejects_damage(void)
{
    unsigned char rec[16];
    int           highscore, mode;

    banner("damaged records degrade to defaults");

    /* A power cut mid-write: all zeroes. */
    memset(rec, 0, sizeof rec);
    write_raw(rec, sizeof rec);
    highscore = -1; mode = -1;
    score_load(&highscore, &mode);
    CHECK(highscore == 0 && mode == MODE_MEDIUM,
          "zeroed file gave highscore %d mode %d", highscore, mode);

    /* Truncated: a short read must never be mistaken for a valid record. */
    score_save(500, MODE_HARD);
    {
        FILE *f = fopen(g_path, "rb");
        unsigned char partial[16];
        size_t n = f ? fread(partial, 1, sizeof partial, f) : 0;

        if (f) {
            fclose(f);
        }
        CHECK(n == 16, "setup: expected a 16-byte record, read %u",
              (unsigned)n);
        write_raw(partial, 12);
    }
    highscore = -1; mode = -1;
    score_load(&highscore, &mode);
    CHECK(highscore == 0 && mode == MODE_MEDIUM,
          "truncated file gave highscore %d mode %d", highscore, mode);

    /* A future format version. */
    score_save(500, MODE_HARD);
    {
        FILE *f = fopen(g_path, "r+b");

        if (f) {
            fseek(f, 4, SEEK_SET);
            fputc(99, f);
            fclose(f);
        }
    }
    highscore = -1; mode = -1;
    score_load(&highscore, &mode);
    CHECK(highscore == 0 && mode == MODE_MEDIUM,
          "future version gave highscore %d mode %d", highscore, mode);

    /* A flipped mode byte is caught by the checksum rather than silently
     * changing the difficulty - which is the reason meta is folded into it. */
    score_save(500, MODE_HARD);
    {
        FILE *f = fopen(g_path, "r+b");

        if (f) {
            fseek(f, 5, SEEK_SET);
            fputc(MODE_EASY, f);
            fclose(f);
        }
    }
    highscore = -1; mode = -1;
    score_load(&highscore, &mode);
    CHECK(highscore == 0 && mode == MODE_MEDIUM,
          "flipped mode byte gave highscore %d mode %d", highscore, mode);

    /* An implausible length: no board can produce it, so the file is not ours. */
    score_save(500, MODE_HARD);
    {
        FILE *f = fopen(g_path, "r+b");

        if (f) {
            fseek(f, 8, SEEK_SET);
            fputc(0xFF, f);
            fclose(f);
        }
    }
    highscore = -1; mode = -1;
    score_load(&highscore, &mode);
    CHECK(highscore == 0 && mode == MODE_MEDIUM,
          "implausible length gave highscore %d mode %d", highscore, mode);

    /*
     * A mode index this build does not have. Unlike the cases above this is not
     * damage - a build with Impossible added would write it (PLAN.md 0.6) - so
     * the highscore must survive it.
     */
    {
        unsigned char future[16];
        uint32_t      meta;

        memset(future, 0, sizeof future);
        memcpy(future, "VSNK", 4);
        future[4] = 2;
        future[5] = (unsigned char)mode_count(); /* one past the last mode */
        meta = (uint32_t)future[4] | ((uint32_t)future[5] << 8);
        put_u32(&future[8], 300u);
        put_u32(&future[12], (300u ^ 0x5A5A5A5Au) ^ meta);
        write_raw(future, sizeof future);
    }
    highscore = -1; mode = -1;
    score_load(&highscore, &mode);
    CHECK(highscore == 300, "unknown mode should keep the highscore, got %d",
          highscore);
    CHECK(mode == MODE_MEDIUM, "unknown mode should fall back to medium, got %d",
          mode);
}

int main(void)
{
    char tmpl[] = "/tmp/vita-snake-score-XXXXXX";
    char *dir   = mkdtemp(tmpl);

    printf("save record tests\n\n");

    if (!dir) {
        printf("cannot create a temporary XDG_DATA_HOME\n");
        return 1;
    }
    /* SDL_GetPrefPath honours XDG_DATA_HOME on Linux, so this keeps the tests
     * away from the player's real save file. */
    setenv("XDG_DATA_HOME", dir, 1);

    plat_init();
    record_path();
    printf("record: %s\n\n", g_path);

    test_missing_file();
    test_round_trip();
    test_legacy_record();
    test_rejects_damage();

    remove_record();
    plat_shutdown();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
