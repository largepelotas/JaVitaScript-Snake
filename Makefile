# Host-side build for the pure core, the desktop shell, and their tests
# (PLAN.md 4.6, 5.3).
#
# This is deliberately NOT the shipping build - CMake drives both real targets
# from Phase 5 (PLAN.md 7.1). Its job is a fast local loop: `make test` runs the
# core with no SDL linked at all, and `make shots` produces the headless
# screenshots that are this project's only visual feedback channel.

CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
           -Wstrict-prototypes -O2 -g
BUILD   := build-host
SHOTS   := artifacts

# Header dependencies. Without these, changing a struct in a header rebuilds
# only some of its users and links objects that disagree about a layout - which
# is not a build annoyance but a memory-corruption bug that looks like a game
# bug. Every object below is compiled through a rule carrying these flags.
DEPFLAGS := -MMD -MP

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
SDL_LIBS   := $(shell pkg-config --libs sdl2 SDL2_ttf)
IMG_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image)
IMG_LIBS   := $(shell pkg-config --libs sdl2 SDL2_image)

CORE_SRC := $(wildcard src/core/*.c)
CORE_OBJ := $(patsubst src/core/%.c,$(BUILD)/core_%.o,$(CORE_SRC))

SHELL_SRC := $(wildcard src/shell/*.c)
SHELL_OBJ := $(patsubst src/shell/%.c,$(BUILD)/shell_%.o,$(SHELL_SRC))
PLAT_OBJ  := $(BUILD)/plat_desktop.o
MAIN_OBJ  := $(BUILD)/main.o

# tests/replay.c reuses the shell's script parser (see src/shell/script.h), which
# is pure C99 and links without SDL.
SCRIPT_OBJ := $(BUILD)/shell_script.o

.PHONY: all test shots parity livearea clean

all: $(BUILD)/test_core $(BUILD)/replay $(BUILD)/snake

test: $(BUILD)/test_core $(BUILD)/test_input $(BUILD)/test_score $(BUILD)/replay
	@$(BUILD)/test_core
	@$(BUILD)/test_input
	@$(BUILD)/test_score
	@tests/run_replays.sh $(BUILD)/replay

$(BUILD):
	@mkdir -p $(BUILD)

# Objects. The core is compiled with no SDL include path at all, which is the
# mechanical half of PLAN.md 1.2's layering rule: a stray #include <SDL.h> in
# src/core/ fails to compile rather than passing review.
$(BUILD)/core_%.o: src/core/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/shell_%.o: src/shell/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD)/plat_desktop.o: src/platform/platform_desktop.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD)/main.o: src/main.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD)/test_core.o: tests/test_core.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/test_input.o: tests/test_input.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD)/test_score.o: tests/test_score.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD)/test_replay.o: tests/replay.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD)/tool_%.o: tools/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(SDL_CFLAGS) $(IMG_CFLAGS) -c $< -o $@

# Binaries.
$(BUILD)/test_core: $(BUILD)/test_core.o $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# Synthesised SDL events, no window: src/shell/input.c is fully testable on the
# host, so it is tested rather than pressed by hand (PLAN.md 0.1).
$(BUILD)/test_input: $(BUILD)/test_input.o $(BUILD)/shell_input.o $(PLAT_OBJ) \
                     $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS)

# The save record is the only state that outlives the process, so it is tested
# through the real platform layer against a redirected XDG_DATA_HOME rather than
# against a stub (tests/test_score.c).
$(BUILD)/test_score: $(BUILD)/test_score.o $(BUILD)/shell_score.o $(PLAT_OBJ) \
                     $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS)

$(BUILD)/replay: $(BUILD)/test_replay.o $(SCRIPT_OBJ) $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/snake: $(MAIN_OBJ) $(SHELL_OBJ) $(PLAT_OBJ) $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS)

$(BUILD)/bmp2png: $(BUILD)/tool_bmp2png.o
	$(CC) $(CFLAGS) $^ -o $@ $(IMG_LIBS)

$(BUILD)/pixel_probe: $(BUILD)/tool_pixel_probe.o
	$(CC) $(CFLAGS) $^ -o $@

# zlib, not SDL2_image: IMG_SavePNG only writes truecolour PNGs, and the Vita
# firmware rejects those in sce_sys (see the header of tools/gen_livearea.c),
# so the tool deflates and writes the indexed PNG itself.
$(BUILD)/gen_livearea: $(BUILD)/tool_gen_livearea.o
	$(CC) $(CFLAGS) $^ -o $@ $(SDL_LIBS) -lz $(SDL_LIBS)

$(BUILD)/gen_shot_script: $(BUILD)/tool_gen_shot_script.o $(CORE_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# The screenshots PLAN.md 5.3 requires. Most come from replays that
# tests/replays/ already blesses with an expected hash, so the picture and the
# regression check are the same run; the ones under tests/shots/ are specific to
# screenshots and carry their own expected hash, and the long-snake session is
# generated (tools/gen_shot_script.c) rather than guessed.
shots: $(BUILD)/snake $(BUILD)/bmp2png $(BUILD)/pixel_probe
	@mkdir -p $(SHOTS)
	@rm -f $(SHOTS)/*.bmp
	$(BUILD)/snake --headless --outdir $(SHOTS) \
	    --script tests/replays/01_straight_run.txt \
	    --shot welcome@0 --shot dead@999999
	$(BUILD)/snake --headless --outdir $(SHOTS) \
	    --script tests/replays/03_pause_resume.txt \
	    --shot paused@21
	$(BUILD)/snake --headless --outdir $(SHOTS) \
	    --script tests/replays/06_full_board_win.txt \
	    --shot won@999999 --shot won_nearly_full@63000
	$(BUILD)/snake --headless --outdir $(SHOTS) \
	    --script tests/shots/long_snake.txt \
	    --shot playing_early@120 --shot playing_long_snake@1035
	@# The welcome dialog naming a difficulty the player cycled to with Square.
	$(BUILD)/snake --headless --outdir $(SHOTS) \
	    --script tests/shots/welcome_hard.txt \
	    --shot welcome_hard@2
	@# The L+R button-index diagnostic. It only ever appears on hardware, so
	@# this headless run produces the reference picture of what it should look
	@# like. 0x34 = indices 2, 4 and 5 held.
	$(BUILD)/snake --headless --outdir $(SHOTS) \
	    --script tests/replays/01_straight_run.txt \
	    --shot diagnostic@0 --diag 0x34
	@for b in $(SHOTS)/*.bmp; do \
	    $(BUILD)/bmp2png $$b $${b%.bmp}.png || exit 1; \
	done
	@tests/check_layout.sh $(BUILD)/pixel_probe $(SHOTS)
	@ls -1 $(SHOTS)/*.png

# Proves the SDL shell drives the core to the same state the core-only harness
# does: same scripts, same expected hashes, two different binaries (PLAN.md 4.7).
parity: $(BUILD)/snake
	@for s in tests/replays/*.txt; do \
	    $(BUILD)/snake --headless --script $$s --outdir $(SHOTS) || exit 1; \
	done

# The LiveArea images (PLAN.md 7.3). Unlike shots/, the output is checked in -
# these are shipped assets, not evidence - so this target is run by hand when
# the motif changes, not as part of a build.
livearea: $(BUILD)/gen_livearea
	$(BUILD)/gen_livearea sce_sys assets/font.ttf

clean:
	rm -rf $(BUILD)

-include $(wildcard $(BUILD)/*.d)
