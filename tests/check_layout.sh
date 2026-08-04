#!/bin/sh
# Asserts the rendered frames against the extracted layout (MECHANICS.md 8, 9).
#
# Run by `make shots` right after the screenshots are produced. Every coordinate
# below is derived from the spec, not read off a picture:
#
#   playfield rect   x 20..940, y 20..500          MECHANICS.md 9
#   letterbox        #fc5454 everywhere outside     MECHANICS.md 8.1
#   playfield        #0000a8                        MECHANICS.md 8.1
#   snake block      18x18 of #fcfc54 at a 1px inset in the playfield color,
#                    which is what produces the 1px gap between segments
#   dead head        #c0c0c0, body unchanged        MECHANICS.md 8.2
#   food             flush 20x20 of #ff0000         MECHANICS.md 8.1
#   overlay box      #000000                        MECHANICS.md 8.1
#
# Usage: check_layout.sh <probe-binary> <artifact-dir>
set -e

PROBE="$1"
DIR="${2:-artifacts}"

fail=0
run() {
    if ! "$PROBE" "$@"; then
        fail=1
    fi
}

# --- Board geometry, on a frame with a long snake -------------------------
# The snake in long_snake.txt at tick 1035 occupies (row 16, col 1), i.e. the
# left edge of the playable interior, and the head is at (row 15, col 39).
run "$DIR/playing_long_snake.bmp" \
    "5,5=FC5454:letterbox, above and left of the field" \
    "19,19=FC5454:last background pixel before the field" \
    "20,20=0000A8:field top-left corner" \
    "939,499=0000A8:field bottom-right corner" \
    "940,500=FC5454:first pixel past the field" \
    "480,510=FC5454:HUD strip is background, not field"

# --- Snake block: 18x18 interior at a 1px inset ---------------------------
# Cell (16,1) covers x 20..39, y 320..339.
run "$DIR/playing_long_snake.bmp" \
    "20,320=0000A8:1px gap at the block's top-left" \
    "21,321=FCFC54:block interior starts one pixel in" \
    "38,338=FCFC54:block interior ends one pixel short" \
    "39,339=0000A8:1px gap at the block's bottom-right"

# --- Food is flush, with no gap -------------------------------------------
# Food sits at (15,39): x 780..799, y 300..319.
run "$DIR/playing_long_snake.bmp" \
    "780,300=FF0000:food fills its cell corner to corner" \
    "799,319=FF0000:food has no 1px border"

# --- Death colors only the head (MECHANICS.md 8.2) ------------------------
# 01_straight_run dies against the right wall: the head ends in the wall ring
# at (2,47), which the original also draws outside the field.
run "$DIR/dead.bmp" \
    "945,45=C0C0C0:dead head is grey" \
    "480,255=000000:death dialog box"

# --- A full board still shows the 1px grid of gaps ------------------------
# The win frame has every one of the 1104 cells occupied, so the playfield
# color survives only in the 1px borders between blocks. Cell (24,46), the
# bottom-right interior cell, covers x 920..939, y 480..499.
run "$DIR/won.bmp" \
    "921,481=FCFC54:last interior cell is filled" \
    "920,480=0000A8:its 1px gap is still playfield-colored" \
    "939,499=0000A8:and so is the corner against the field edge"

# --- Overlays --------------------------------------------------------------
# Welcome box: left 322, top 172, 316 wide (common-snake.css:84-96).
run "$DIR/welcome.bmp" \
    "322,172=000000:welcome box top-left" \
    "321,172=0000A8:one pixel left of the box is field" \
    "637,180=000000:welcome box right edge"

# Pause box: 300x80 centred, so x 330..630, y 232..312.
run "$DIR/paused.bmp" \
    "330,232=000000:pause box top-left" \
    "629,311=000000:pause box bottom-right" \
    "329,232=0000A8:one pixel left of the pause box is field"

# --- Per-theme spot checks (PLAN-THEMES.md 8) -----------------------------
# Deliberately not the geometry table above: that stays on Main, because a
# second copy per theme would cost maintenance and buy no extra confidence.
# The risk these catch is a mis-transcribed row in the theme table, so they
# probe one pixel per field that a row can get wrong, reusing coordinates the
# Main checks already established as playfield, HUD strip and pause box.
#
# pause_bg is checked because it is a field this port added: draw_paused used
# to borrow overlay_bg, which is right for Main and wrong for Original.
check_theme() {
    name="$1" playfield="$2" background="$3" pause_bg="$4"

    run "$DIR/theme_${name}_playing.bmp" \
        "939,499=$playfield:$name playfield" \
        "480,510=$background:$name HUD strip is background"
    run "$DIR/theme_${name}_paused.bmp" \
        "330,232=$pause_bg:$name pause box"
}

#          name      playfield  background  pause_bg
check_theme main     0000A8     FC5454      000000
check_theme matrix   000000     00FF11      000000
check_theme original 149C36     004620      004620

if [ "$fail" -ne 0 ]; then
    echo "layout checks FAILED"
    exit 1
fi
echo "layout checks ok"
