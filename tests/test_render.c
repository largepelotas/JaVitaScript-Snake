/*
 * Theme table tests (PLAN-THEMES.md 7).
 *
 * Cheap, but this is what catches a table row added with a field missing or a
 * value pasted into the wrong column - which is the actual risk when the whole
 * feature is "themes are data". Nothing here opens a window or a font: the
 * table and the two functions over it are pure.
 *
 * Colour values are deliberately NOT asserted against literals. Doing so would
 * only restate render.c in a second file, and both would be wrong together if a
 * value were mis-transcribed. The screenshots in artifacts/ are what pin the
 * colours, checked by tests/check_layout.sh.
 */
#include "../src/shell/render.h"

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

static void test_table_is_complete(void)
{
    int i, j;

    banner("every theme has a name and an author");

    CHECK(theme_count() == 4, "expected four themes, got %d", theme_count());

    for (i = 0; i < theme_count(); i++) {
        const Theme *t = theme_get(i);

        CHECK(t != NULL, "theme_get(%d) returned NULL", i);
        CHECK(t->name != NULL && t->name[0] != '\0',
              "theme %d has no name", i);
        CHECK(t->author != NULL && t->author[0] != '\0',
              "theme %d (%s) has no author - these are other people's "
              "contributions and the reference credits them by name", i,
              t->name ? t->name : "?");
    }

    /* A duplicated name would make --theme ambiguous and the welcome line a
     * lie; a duplicated author is fine and not checked. */
    for (i = 0; i < theme_count(); i++) {
        for (j = i + 1; j < theme_count(); j++) {
            CHECK(strcmp(theme_get(i)->name, theme_get(j)->name) != 0,
                  "themes %d and %d share the name '%s'", i, j,
                  theme_get(i)->name);
        }
    }
}

static void test_main_is_index_zero(void)
{
    banner("Main is index 0");

    /* The save record's default byte is zero and every pre-theme screenshot
     * assumes it, so this is load-bearing rather than cosmetic
     * (PLAN-THEMES.md 5, 10). */
    CHECK(THEME_MAIN == 0, "THEME_MAIN should be 0, got %d", THEME_MAIN);
    CHECK(strcmp(theme_get(THEME_MAIN)->name, "Main") == 0,
          "index 0 should be Main, got '%s'", theme_get(THEME_MAIN)->name);
    CHECK(strcmp(theme_get(THEME_MATRIX)->name, "Matrix") == 0,
          "THEME_MATRIX should name matrix, got '%s'",
          theme_get(THEME_MATRIX)->name);
    CHECK(strcmp(theme_get(THEME_ORIGINAL)->name, "Original") == 0,
          "THEME_ORIGINAL should name original, got '%s'",
          theme_get(THEME_ORIGINAL)->name);
    CHECK(strcmp(theme_get(THEME_VITA)->name, "Vita") == 0,
          "THEME_VITA should name vita, got '%s'",
          theme_get(THEME_VITA)->name);
}

static void test_get_clamps(void)
{
    int last = theme_count() - 1;

    banner("theme_get clamps at both ends");

    /* An out-of-range index must never be a crash: it arrives from a save file
     * written by another build, which PLAN-THEMES.md 5 requires be harmless. */
    CHECK(theme_get(-1) == theme_get(0), "theme_get(-1) should clamp to 0");
    CHECK(theme_get(-99999) == theme_get(0), "a large negative should clamp");
    CHECK(theme_get(theme_count()) == theme_get(last),
          "one past the end should clamp to the last theme");
    CHECK(theme_get(99999) == theme_get(last), "a large index should clamp");
}

static void test_advance_wraps(void)
{
    int n = theme_count();
    int i;

    banner("theme_advance wraps at theme_count");

    CHECK(theme_advance(0, 1) == 1, "0 + 1 should be 1");
    CHECK(theme_advance(n - 1, 1) == 0, "the last theme should wrap to the "
                                        "first");
    CHECK(theme_advance(0, 0) == 0, "a zero delta should not move");

    /* Cycling all the way round returns to where it started, for every start,
     * which is the property Triangle actually relies on. */
    for (i = 0; i < n; i++) {
        CHECK(theme_advance(i, n) == i,
              "advancing by theme_count from %d should return to %d", i, i);
    }

    /* The loop consumes an accumulated delta, so more than one step per frame
     * is a real case, and a large one must not overflow or escape the range. */
    CHECK(theme_advance(0, n * 1000 + 2) == 2,
          "a large delta should still land in range");
    for (i = 0; i < n; i++) {
        int t = theme_advance(i, 1000003);

        CHECK(t >= 0 && t < n, "advance(%d, 1000003) left the range: %d", i, t);
    }

    /* Negative deltas cannot happen today - nothing decrements theme_delta -
     * but the wrap is written to survive one, so it is pinned here rather than
     * left to be discovered later. */
    CHECK(theme_advance(0, -1) == n - 1, "-1 from the first should wrap to the "
                                         "last");
    CHECK(theme_advance(0, -(n * 1000 + 1)) == n - 1,
          "a large negative delta should still land in range");
}

int main(void)
{
    test_table_is_complete();
    test_main_is_index_zero();
    test_get_clamps();
    test_advance_wraps();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
