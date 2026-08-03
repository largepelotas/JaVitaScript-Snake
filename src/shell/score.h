/*
 * Save-record persistence (PLAN.md 6.3).
 *
 * The core never does I/O, so the record lives out here: the shell reads it at
 * startup and hands the values to game_set_highscore and game_init. The
 * encoding is target independent, which is why it is in src/shell/ rather than
 * in a platform file - only the directory it lands in differs per target.
 *
 * MECHANICS.md 7: the highscore is a single global value shared by all three
 * modes, exactly as in the original's one localStorage key. The chosen
 * difficulty rides in the same record because it is saved at the same moments
 * and losing one without the other has no meaning.
 */
#ifndef SCORE_H
#define SCORE_H

#include <stdbool.h>

/* Fixed-size record: magic, version, mode, value, checksum. A short read is a
 * failed read, so a truncated file can never be mistaken for a valid one. */
#define SCORE_RECORD_BYTES 16

/*
 * Reads the record. Any output may be NULL.
 *
 * A missing, truncated, corrupt or implausible file is not an error: highscore
 * degrades to 0, mode to MODE_MEDIUM, the original's dropdown default, and
 * theme to Main (PLAN.md 6.3, never trust the file). Records written by the
 * pre-difficulty format are still read; they simply carry no mode.
 *
 * The theme rides in the same record for the same reason the mode does: it is
 * saved at the same moments, and it needs no format version of its own, because
 * byte 6 of the meta word was already reserved as zero and already covered by
 * the checksum (PLAN-THEMES.md 5). Zero is Main, so every record written before
 * themes existed already says the right thing.
 */
void score_load(int *highscore, int *mode, int *theme);

/* False if the record could not be written; the caller may keep playing. */
bool score_save(int highscore, int mode, int theme);

#endif /* SCORE_H */
