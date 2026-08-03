VITA SNAKE - BUILD PLAN
A PS Vita homebrew remake of patorjk's JavaScript Snake

Audience: Claude Code (Opus 5), executing on Windows 11 with WSL2.
Author of record: the maintainer. Target device: real hacked PS Vita (Ensō/HENkaku/h-encore).
Reference original: https://github.com/patorjk/JavaScript-Snake (MIT, Patrick Gillespie)
Live original: https://patorjk.com/games/snake/


==============================================================================
0. READ THIS FIRST - OPERATING RULES FOR THE AGENT
==============================================================================

0.1  You cannot run code on the Vita. You can compile for it, but you cannot
     observe it. Therefore: every piece of behavior that CAN be verified on the
     host MUST be verified on the host before it is allowed near the Vita build.
     The Vita build is a packaging step, not a debugging step.

0.2  Do not invent the original game's constants. The reference source is
     available and MIT licensed. Read it, extract real values, and record them
     in docs/MECHANICS.md with file and line citations. Any constant in this
     plan written as EXTRACT is a placeholder you must replace with the real
     value from the reference source.

0.3  Work in phases. Do not begin a phase until the previous phase's exit
     criteria are met. Report exit criteria explicitly before moving on.

0.4  Stop and ask the maintainer when you hit any of these:
     - VitaSDK package install fails or a required library is unavailable
     - The reference source contradicts something in this plan
     - A controller button index cannot be confirmed without hardware
     - You are about to guess at Vita-specific runtime behavior

0.5  Commit after every phase with a message naming the phase. Keep the
     reference repo in reference/ and never modify it.

0.6  Scope for v1 is fixed. Build exactly this and nothing more:
     - Core gameplay identical to the original
     - Three modes: Easy, Medium, Hard
     - HUD showing Length and Highscore
     - Highscore persistence across launches
     - Main theme only
     - Welcome screen, death screen, win screen, pause
     Impossible mode, Rush mode, alternate themes, fullscreen toggle, and the
     AI-driver hooks are OUT of scope, but the mode table and theme table must
     be written as data tables so adding them later is a data change, not a
     code change.


==============================================================================
1. DECISIONS ALREADY MADE (do not relitigate)
==============================================================================

1.1  Rendering stack: SDL2.

     Rationale, so you understand the constraint you are designing under:
     vita2d is lighter and more idiomatic for Vita-only homebrew, but it only
     builds for the Vita, which would mean zero testable iterations. SDL2 has
     a maintained PS Vita backend AND builds natively on the host, so the exact
     same rendering and game code can be run, screenshotted, and asserted
     against on the desktop before it is ever cross-compiled. Testability wins.

1.2  Architecture: strict three-layer separation.

     Layer A - src/core/     Pure C99. No SDL, no platform headers, no I/O,
                             no globals, no time calls. Deterministic. This is
                             where the entire game lives.
     Layer B - src/shell/    SDL2 rendering, input translation, timing loop.
                             Identical source on both targets.
     Layer C - src/platform/ The only place target differences exist: storage
                             paths, heap size, button index table, log sink.

     If you find yourself adding an #ifdef __vita__ outside src/platform/, the
     abstraction is wrong. Fix the abstraction.

1.3  Emulator: yes, install and use Vita3K. It is not for gameplay iteration
     (the desktop build covers that). It is for validating the things the
     desktop build structurally cannot: VPK packaging, LiveArea assets,
     TITLE_ID correctness, ux0: filesystem paths, and first-launch behavior.
     Catching a bad VPK in Vita3K is minutes; catching it on hardware is a
     transfer cycle.

1.4  Language: C99. Not C++. The original is ~1500 lines of plain JS with no
     dependencies; the port should be equally boring.

1.5  Licensing: the original is MIT. Vendor its LICENSE file to
     third_party/JavaScript-Snake-LICENSE and credit Patrick Gillespie in
     README.md, in docs/MECHANICS.md, and on the in-game welcome screen in
     small text. This is a derivative work and must say so.


==============================================================================
2. PHASE 0 - TOOLCHAIN AND ENVIRONMENT
==============================================================================

Exit criteria: an unmodified VitaSDK SDL2 sample compiles to a .vpk, and a
desktop SDL2 hello-window compiles and runs on the host.

2.1  Environment. All build work happens inside WSL2 Ubuntu, not Windows
     native. VitaSDK's Windows support is a source of avoidable friction.
     Confirm WSL2 Ubuntu is present; if not, tell the maintainer to run
     `wsl --install -d Ubuntu` from an elevated PowerShell and reboot.

2.2  Host dependencies:

     sudo apt update
     sudo apt install -y git cmake make python3 curl wget unzip xz-utils \
       build-essential pkg-config libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev

2.3  VitaSDK install. Prefix is ~/vitasdk, NOT /usr/local/vitasdk (the maintainer's
     decision, 2026-07-31 - see the deviation note at the end of this section).

     git clone https://github.com/vitasdk/vdpm ~/vdpm
     export VITASDK=$HOME/vitasdk
     export PATH=$VITASDK/bin:$PATH

     Do NOT run ./bootstrap-vitasdk.sh. Extract the toolchain directly - these
     are the same three operations it performs, minus a sudo call that is not
     needed for a prefix under $HOME:

       URL=$(wget -qO- https://github.com/vitasdk/vita-headers/raw/master/\
     .travis.d/last_built_toolchain.py | python3 - master linux)
       mkdir -p "$VITASDK"
       wget -qO- "$URL" | tar xj -C "$VITASDK" --strip-components=1

     Then, and only after confirming $VITASDK/bin/arm-vita-eabi-gcc exists:

       cd ~/vdpm && ./install-all.sh

     Why bootstrap is bypassed: include/install-vitasdk.sh calls sudo mkdir and
     sudo chown unconditionally when the target is absent - it never checks
     whether the parent directory is already writable - so it demands a
     password even for ~/vitasdk. Those are its only sudo calls in the whole
     repo; install-all.sh and vdpm itself need no privileges. Bypassing the one
     script keeps the entire toolchain install runnable by the agent with no
     human at a terminal.

     Two failure modes to recognise, both hit during the first attempt:

     - Do NOT pre-create $VITASDK if you do use bootstrap-vitasdk.sh: it aborts
       when the directory already exists. That is what broke attempt one. Worse,
       install-all.sh then "installed" 40+ packages into a prefix with no
       toolchain, printing "Successfully installed" for every one, because it
       never checks tar's exit status. Watch for repeated
         tar: <prefix>/arm-vita-eabi: Cannot open: No such file or directory
       interleaved with success messages. Trust the on-disk result, not vdpm's
       output: verify with the checks in 2.4/2.5 below.
     - If a re-install is ever needed, delete the whole prefix first. A stale
       etc/vdpm/packages.list will otherwise make install-all.sh skip
       everything it believes is already present.

     Deviation from the original plan text, and why: ~/vitasdk needs no sudo at
     any point, which means the agent can bootstrap, retry, and add SDK
     packages without a human at a terminal. /usr/local/vitasdk required a
     privileged step for every one of those. Nothing depends on the literal
     path - all builds resolve the toolchain through $VITASDK, including
     $VITASDK/share/vita.toolchain.cmake in 7.1. A leftover empty
     /usr/local/vitasdk from the failed attempt is inert and can be removed
     with sudo rm -rf whenever convenient.

     Append the two export lines to ~/.bashrc. install-all.sh pulls the common
     library set including SDL2. Verify with:

     arm-vita-eabi-gcc --version
     ls $VITASDK/arm-vita-eabi/lib | grep -i sdl2

2.4  Confirm SDL2_ttf is present for the Vita target. If it is NOT, do not
     improvise: fall back to the bitmap font path described in section 6.4 and
     tell the maintainer you switched. Check with:

     ls $VITASDK/arm-vita-eabi/lib | grep -iE 'ttf|freetype'

2.5  Authoritative link flags. Never hand-write the Vita SDL2 link list from
     memory or from this document. Query it:

     $VITASDK/arm-vita-eabi/bin/sdl2-config --libs
     $VITASDK/arm-vita-eabi/bin/sdl2-config --cflags

     Use exactly what it returns, in the order it returns. Static link order
     matters on this toolchain.

     Verified 2026-07-31, --libs returns (order preserved):
       -L$VITASDK/arm-vita-eabi/lib $VITASDK/arm-vita-eabi/lib/libSDL2.a
       -lSceGxm_stub -lSceDisplay_stub -lSceCtrl_stub -lSceAppMgr_stub
       -lSceAppUtil_stub -lSceAudio_stub -lSceAudioIn_stub -lSceSysmodule_stub
       -lSceIofilemgr_stub -lSceCommonDialog_stub -lSceTouch_stub
       -lSceHid_stub -lSceMotion_stub -lScePower_stub -lSceProcessmgr_stub -lm
     and --cflags returns -I$VITASDK/arm-vita-eabi/include/SDL2.

     DO NOT use pkg-config for the Vita target. sdl2-config computes paths from
     its own location and is correct; the .pc files are not. 102 of the 107
     .pc files shipped by vdpm hardcode prefix=/usr/local/vitasdk, so with any
     other prefix they emit -L paths into a directory that does not hold the
     libraries. Separately, SDL2_ttf.pc uses prefix=${VITASDK}, which
     pkg-config does not expand from the environment - it emits the malformed
     -L/arm-vita-eabi/lib at every prefix, including the default one. This is
     an upstream packaging defect, not a consequence of the prefix choice in
     2.3. The failure is loud (cannot find -lSDL2) rather than silent, but do
     not spend time on it: use sdl2-config. Host-side pkg-config is unaffected
     and is fine to use for the desktop build.

2.6  Vita3K. Tell the maintainer to install the Windows build of Vita3K and complete
     its firmware setup. You will not drive it; you will hand the maintainer a .vpk and
     a checklist. Note that Vita3K runs on the Windows side, so the built .vpk
     must be copied out of WSL to a Windows-visible path.

2.7  Smoke test both targets:
     - Build any VitaSDK SDL2 sample to .vpk. Confirm a .vpk file is produced.
     - Build a 30-line desktop SDL2 window that clears to a color and exits
       after 60 frames. Confirm it runs. If WSLg is unavailable and no window
       appears, that is expected and fine - see 5.3, headless is the primary
       verification path anyway.


==============================================================================
3. PHASE 1 - REVERSE-SPECIFY THE ORIGINAL
==============================================================================

Exit criteria: docs/MECHANICS.md exists and every EXTRACT below is replaced
with a real value and a citation to reference/src/js/snake.js (file + line).

3.1  Clone the reference, read-only:

     git clone https://github.com/patorjk/JavaScript-Snake reference
     (branch: main; the game logic is in reference/src/js/snake.js)

3.2  Read snake.js in full before writing anything. It is a single file, no
     dependencies, DOM-based. Note that the snake body is implemented as a
     linked list - you may use a ring buffer instead, but only if behavior is
     provably identical.

3.3  Extract and document, with citations, each of the following:

     GRID
     - Block size in pixels                                        EXTRACT
     - How column and row counts are derived from board dimensions EXTRACT
     - Default non-fullscreen board size (README states 580x400)
     - Whether the playfield has a border/frame and its thickness  EXTRACT

     SNAKE
     - Starting length (HUD shows "Length: 1" at start)            EXTRACT
     - Starting position and starting direction                    EXTRACT
     - Whether the snake moves immediately on game start or waits
       for first input                                             EXTRACT
     - Growth amount per food eaten                                EXTRACT
     - Head vs body rendering difference (there is a head color
       feature in the repo history)                                EXTRACT

     MOVEMENT AND INPUT
     - Milliseconds per step for easy / medium / hard              EXTRACT
     - Whether step interval changes as the snake grows, and the
       exact formula if so                                         EXTRACT
     - The input queue / premove behavior. The original queues
       direction inputs so fast successive presses register on
       consecutive steps rather than being collapsed. Capture the
       queue depth and the exact rules.                            EXTRACT
     - 180-degree reversal rejection rule - is it checked against
       current direction or against last-applied direction?        EXTRACT
     - Behavior of input received while paused (repo has a
       premoveOnPause config option)                               EXTRACT

     FOOD
     - Spawn selection algorithm (uniform over free cells? retry
       loop? excludes cells adjacent to head?)                     EXTRACT
     - Whether more than one food exists at a time                 EXTRACT

     COLLISION AND END STATES
     - Wall collision behavior (die vs wrap) per mode              EXTRACT
     - Self-collision rule, specifically whether moving into the
       cell the tail is vacating this step is death or legal       EXTRACT
     - Win condition (board fully filled) and exact trigger        EXTRACT

     SCORING AND PERSISTENCE
     - What "Length" counts exactly                                EXTRACT
     - Whether highscore is global or per-mode                     EXTRACT
     - localStorage key name and value format                      EXTRACT

     PRESENTATION (main theme only)
     - Exact hex colors: background, playfield, border, snake body,
       snake head, food, HUD text, overlay text                    EXTRACT
     - Exact on-screen strings. Confirmed from the live page:
         "JavaScript Snake"
         "Play Game"
         "You died :("
         "Play Again?"
         "You win! :D"
         "[Paused] Press [space] to unpause."
         "Length: N"
         "Highscore: N"
       The pause string and the welcome instructions must be
       rewritten for Vita controls - see 6.5.

3.4  Write docs/MECHANICS.md as the single source of truth. src/core/ is
     implemented from MECHANICS.md, not from snake.js directly. If you later
     find MECHANICS.md is wrong, fix MECHANICS.md first, then the code.


==============================================================================
4. PHASE 2 - CORE GAME LOGIC (PURE C, FULLY TESTED)
==============================================================================

Exit criteria: tests/ passes 100%, built and run on the host with gcc. No SDL
linked. No Vita headers. Deterministic under a seeded RNG.

4.1  Files:

     src/core/snake_types.h    enums, structs, no functions
     src/core/rng.c/.h         xorshift32 or PCG. Seeded explicitly. Never
                               calls time(). The shell supplies the seed.
     src/core/board.c/.h       grid, cell occupancy, free-cell enumeration
     src/core/snake.c/.h       body storage, move, grow, self-collision
     src/core/modes.c/.h       const table of mode definitions
     src/core/game.c/.h        state machine, input queue, tick, scoring

4.2  The one function that matters:

     game_tick(GameState *g, uint32_t elapsed_ms) -> GameEvent flags

     It is a pure function of (state, elapsed_ms). It must not read a clock,
     allocate, or touch I/O. All rendering reads the resulting state; the core
     never draws. All randomness comes from g->rng, which the caller seeds.

4.3  State machine states: WELCOME, READY, PLAYING, PAUSED, DEAD, WON.
     Enumerate every legal transition in MECHANICS.md and assert illegal ones
     are unreachable in tests.

     AMENDED 2026-07-31 (the maintainer): READY was added. The original has a distinct
     phase where the dialog is dismissed and the snake and food are placed and
     drawn, but nothing moves until the first direction press. Folding it into
     WELCOME draws the overlay over a live board; folding it into PLAYING starts
     the snake by itself. See docs/MECHANICS.md 6.6 and 11.1.

4.4  Input model: the shell calls game_queue_input(g, DIR) and never mutates
     direction directly. The queue implements the original's premove behavior
     exactly as extracted in 3.3.

4.5  Mode table - shape only, values from MECHANICS.md:

     typedef struct {
         const char *name;        /* "Easy" */
         uint32_t    step_ms;     /* from EXTRACT */
         bool        walls_kill;  /* from EXTRACT */
         /* add fields only if the original has them */
     } SnakeMode;

     Ship easy/medium/hard. Leave the array open-ended so impossible and rush
     are a two-line addition later.

4.6  Tests - tests/test_core.c, plain asserts, one binary, run via
     `make test`. Required cases at minimum:

     - Snake of length N moves N steps; head lands on expected cell each step
     - Eating food increments length by the extracted growth amount
     - Food never spawns on an occupied cell (run 100k seeded spawns)
     - Board-full state produces WON, not a food-spawn infinite loop
     - Wall collision produces DEAD (or wrap, per mode) at the exact boundary
     - Reversal input is rejected under the extracted rule
     - Queued inputs apply on consecutive ticks, not collapsed
     - Moving into the tail's vacating cell resolves per the extracted rule
     - Sub-step-interval elapsed_ms produces zero moves; a large elapsed_ms
       produces the correct number of moves without skipping collisions
     - PAUSED consumes elapsed_ms without advancing the snake
     - Identical seed + identical input script produces identical final state
       (run twice, memcmp the state)

4.7  Add a scripted-replay harness: tests/replays/*.txt containing a seed, a
     mode, and a list of (tick, input) pairs, with an expected final state
     hash. This gives you regression coverage that survives refactors, and it
     is how you will prove the SDL shell has not changed behavior.


==============================================================================
5. PHASE 3 - DESKTOP SHELL AND VISUAL VERIFICATION
==============================================================================

Exit criteria: the game is fully playable and visually correct on the host,
and headless screenshots of all five states have been generated and inspected.

5.1  Files:

     src/shell/render.c/.h     all drawing, SDL_Renderer only
     src/shell/text.c/.h       text drawing behind a tiny interface
     src/shell/input.c/.h      SDL events -> core direction/action inputs
     src/shell/loop.c          main loop, timing accumulator
     src/platform/platform.h   the target-difference interface (see 6.1)
     src/platform/platform_desktop.c

5.2  Timing. Render at display refresh with vsync. Accumulate real elapsed ms
     and call game_tick with it. Do not tie step rate to frame rate. Clamp a
     single frame's elapsed_ms (e.g. 250ms) so a stall cannot fast-forward the
     snake through a wall.

5.3  Headless verification - this is the important part, build it early.

     Add a --headless mode: SDL_CreateSoftwareRenderer into an
     SDL_Surface sized 960x544 (the Vita's exact resolution), run a scripted
     replay, and SDL_SaveBMP a frame at specified ticks to
     artifacts/shot_NNN.bmp. Convert to PNG.

     Then actually look at the images. Generate and inspect, at minimum:
       welcome.png, playing_early.png, playing_long_snake.png,
       paused.png, dead.png, won.png
     Compare each against the live original at patorjk.com/games/snake for
     layout, color, and text placement. Iterate until they match.

     This is your only reliable visual feedback loop for the entire project.
     Treat a broken --headless mode as a P0 bug.

5.4  Layout for a 960x544 screen. The Vita is 960x544; render at exactly that
     on desktop so what you verify is what ships.
     - Reserve a HUD strip (Length left, Highscore right), matching the
       original's HUD placement and color.
     - The playfield is the remaining area, using the original's block size
       from MECHANICS.md. Compute cols = floor(playfield_w / block) and
       rows = floor(playfield_h / block), then center the resulting grid and
       letterbox the remainder in the background color. Do not scale blocks to
       fill - preserving the original block size preserves the feel.
     - Record the resulting grid dimensions in MECHANICS.md and note that they
       differ from the original's browser-window-dependent grid. This is the
       one intentional deviation; call it out in README.md.

5.5  Overlays (welcome, dead, won, paused) are drawn over the playfield in the
     original's style. Match the original's box, border, and text colors.


==============================================================================
6. PHASE 4 - VITA PLATFORM LAYER
==============================================================================

Exit criteria: the Vita build compiles clean, produces a .vpk, and every
Vita-specific behavior is isolated in src/platform/platform_vita.c.

6.1  The platform interface - keep it this small:

     void        plat_init(void);
     void        plat_shutdown(void);
     const char *plat_storage_dir(void);          /* trailing slash */
     bool        plat_read_file(const char *path, void *buf, size_t n);
     bool        plat_write_file(const char *path, const void *buf, size_t n);
     void        plat_log(const char *fmt, ...);
     uint32_t    plat_seed(void);
     int         plat_button_for(PadButton b);    /* SDL button index */

6.2  Heap. SDL2 on Vita needs the newlib heap raised or it will fail at init.
     In platform_vita.c, at file scope:

     unsigned int _newlib_heap_size_user = 192 * 1024 * 1024;

     Guard it with #ifdef __vita__.

6.3  Storage.
     - Desktop: ./ or an SDL_GetPrefPath() directory.
     - Vita: "ux0:data/VitaSnake/". Create it at startup before first write;
       do not assume it exists. Highscore file: highscore.dat, and write it as
       a tiny versioned text or binary record with a magic header so a corrupt
       or missing file degrades to zero rather than crashing. Never trust the
       file - validate on read.
     - Also open ux0:data/VitaSnake/log.txt as the plat_log sink on Vita. This
       is your only debugging channel on hardware. Log init steps, SDL error
       strings, storage results, and detected button indices.

6.4  Text rendering.
     Preferred: SDL2_ttf with a bundled open-licensed font (a monospace face
     under OFL or Apache-2.0; put the license file next to it). Load from
     "app0:assets/font.ttf" on Vita and "assets/font.ttf" on desktop, via
     plat_asset_path().
     Fallback, if 2.4 showed SDL2_ttf unavailable: bake a fixed-width bitmap
     font atlas PNG at the sizes you need and blit glyphs. Zero dependencies,
     pixel-identical across targets. If you take this path, do it in Phase 3
     on desktop first, not as a late Vita-only substitution.

6.5  Input mapping. This is the highest-risk item in the project because you
     cannot confirm button indices without hardware. Handle it defensively:

     - Put every mapping in one const table in platform_vita.c. Nothing else
       references raw indices.
     - Support both the D-pad and the left analog stick for direction. Apply a
       deadzone to the stick and convert to a discrete direction; feed both
       through game_queue_input so the queue semantics stay identical.
     - Intended mapping:
         D-pad / left stick  -> direction
         Cross               -> Play Game / Play Again / confirm
         Start               -> pause and unpause
         Circle              -> back to welcome from dead/won
         Select              -> nothing in v1
     - Ship a hidden diagnostic: hold L+R at the welcome screen to display,
       on screen, the index of every button currently pressed, and log the
       same to log.txt. the maintainer runs this once on hardware and reports the real
       indices; you then correct one table.
     - Do not hardcode indices you have not verified and do not present
       unverified indices as confirmed.

6.6  Rewrite Vita-inappropriate strings. The original's "Press [space] to
     unpause" and "press F11 for fullscreen" do not apply. Use:
       "[Paused] Press START to unpause."
       Welcome: "Use the D-Pad or left stick to play." plus
       "Press X to start." plus a small credit line naming Patrick Gillespie
       and the MIT license.
     Keep the original's typography and color for these; only the wording
     changes.

6.7  Do not implement suspend/resume, sleep handling, or touch controls in v1.
     Note them in README.md as known gaps.


==============================================================================
7. PHASE 5 - PACKAGING
==============================================================================

Exit criteria: a .vpk that installs in Vita3K and reaches the welcome screen.

7.1  CMake. One CMakeLists.txt driving both targets, selected by whether
     CMAKE_TOOLCHAIN_FILE points at vita.toolchain.cmake.

     Vita configure:
       cmake -B build-vita \
         -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
       cmake --build build-vita

     Desktop configure:
       cmake -B build-host && cmake --build build-host

7.2  Vita packaging macros: vita_create_self then vita_create_vpk.
     - VITA_APP_NAME  "Snake"
     - VITA_TITLEID   "SNEK00001"  (the maintainer's choice, 2026-07-31; supersedes the
                       earlier VSNK00001. Valid form: 4 uppercase letters +
                       5 digits. VitaDB collision check is deliberately
                       DEFERRED - see 11.5. Do not change this ID without
                       the maintainer.)
     - VITA_VERSION   "01.00"
     - Include assets/font.ttf (or the atlas) as a packaged FILE so it lands
       at app0:assets/.

7.3  LiveArea assets in sce_sys/:
       icon0.png                        128x128
       livearea/contents/bg.png         840x500
       livearea/contents/startup.png    280x158
       livearea/contents/template.xml
     Generate simple, clean assets in the game's own palette - a snake segment
     motif on the playfield background reads well at icon size. Copy a working
     template.xml from a VitaSDK sample and edit it; do not author one from
     scratch.

     HARDWARE FINDING 2026-07-31. These PNGs must be colour type 3 (8-bit
     indexed). Written as RGBA they install and run fine in Vita3K but fail on
     hardware: VitaShell aborts at the end of the copy with 0x8010113D. The
     emulator's PNG decoder is permissive; the firmware's is not. All 18
     sce_sys images across the VitaSDK samples are colour type 3, which is the
     confirmation that mattered - the error code itself is undocumented.

     This is why tools/gen_livearea.c writes PNGs itself instead of calling
     IMG_SavePNG, which only emits truecolour. Anything later added to sce_sys
     needs the same treatment.

7.4  Fast iteration on hardware. Have the maintainer install VitaShell and
     vitacompanion. Once the VPK is installed once, subsequent builds can be
     pushed by FTP as eboot.bin to ux0:app/SNEK00001/eboot.bin and relaunched
     without reinstalling. Document the exact FTP command sequence in
     docs/DEPLOY.md so the maintainer can run it as a one-liner.

7.5  Crash handling. If it crashes on hardware, the maintainer will have a core dump in
     ux0:data/. Document how to run vita-parse-core against the dump and the
     unstripped ELF in docs/DEPLOY.md, and keep build-vita/ artifacts so the
     symbols are available.

     NOTE (verified 2026-07-31): vita-parse-core is NOT installed by vdpm's
     install-all.sh. vita-pack-vpk, vita-make-fself and vita-elf-create are all
     present in $VITASDK/bin; vita-parse-core is not. It lives in a separate
     repo (vitasdk/vita-parse-core) and is a standalone Python tool. Install it
     when writing docs/DEPLOY.md in Phase 5 - it is not needed before then, and
     it is not a Phase 0 blocker.

     DONE 2026-07-31 (Phase 5), with two corrections to the note above:
     - The repo is xyzz/vita-parse-core. There is no vitasdk/vita-parse-core.
     - It is Python 2 and unmaintained since 2022, so it does not run at all on
       this machine's Python 3.14. It is installed at ~/vita-parse-core with a
       Python 3 port applied; that port is checked in as
       tools/vita-parse-core-py3.patch so the install is reproducible.
     pyelftools is vendored under ~/vita-parse-core/vendor rather than
     pip-installed: this box has no pip and no python3-venv, and both need root.
     The whole pipeline was verified against a synthesised .psp2dmp before any
     real crash existed - see docs/DEPLOY.md 3. The Vita target now also builds
     with -g so dumps resolve to file:line; this does not change eboot.bin,
     which is built from the stripped SELF.


==============================================================================
8. PHASE 6 - VALIDATION (THE MAINTAINER RUNS THIS, YOU WRITE IT)
==============================================================================

Produce docs/TESTPLAN.md containing a numbered checklist the maintainer can execute in
one sitting on hardware. Include at minimum:

  1.  VPK installs; bubble and LiveArea art appear correct
  2.  First launch with no ux0:data/VitaSnake/ present - no crash, highscore 0
  3.  Welcome screen text and layout match the desktop screenshots
  4.  L+R diagnostic reports button indices; the maintainer records them
  5.  D-pad moves the snake in all four directions
  6.  Left stick moves the snake in all four directions; deadzone feels right
  7.  Rapid opposite-direction press does not reverse the snake into itself
  8.  Rapid two-direction press (e.g. up then right within one step) applies
      both on consecutive steps - this verifies the premove queue
  9.  START pauses and unpauses; pause overlay text correct
 10.  Each of easy/medium/hard has a visibly different speed
 11.  Death screen appears on wall and on self collision
 12.  Highscore updates, and survives quitting to LiveArea and relaunching
 13.  Extended play (5+ minutes) with no slowdown, no memory growth, no crash
 14.  log.txt exists and contains sane init output

Ask the maintainer to send back log.txt and the button indices from item 4.


==============================================================================
9. REPOSITORY LAYOUT
==============================================================================

vita-snake/
  CMakeLists.txt
  README.md                    what this is, credit, known deviations
  PLAN.md                      this file
  docs/
    MECHANICS.md               extracted spec, the source of truth
    DEPLOY.md                  build, FTP push, core dump analysis
    TESTPLAN.md                hardware checklist for the maintainer
  src/
    core/                      pure C, no dependencies, fully tested
    shell/                     SDL2 render/input/loop, shared both targets
    platform/                  platform.h + _desktop.c + _vita.c
    main.c
  tests/
    test_core.c
    replays/
  assets/
    font.ttf (+ its license)
  sce_sys/
    icon0.png
    livearea/contents/{bg.png,startup.png,template.xml}
  reference/                   clone of patorjk/JavaScript-Snake, read-only
  third_party/
    JavaScript-Snake-LICENSE
  artifacts/                   headless screenshots, gitignored


==============================================================================
10. ORDER OF OPERATIONS - SUMMARY
==============================================================================

  Phase 0  Toolchain up, both targets compile a hello world
  Phase 1  Read snake.js, write MECHANICS.md, no guessing
  Phase 2  src/core/ + tests, 100% on host, deterministic
  Phase 3  Desktop shell, headless screenshots, visual parity with original
  Phase 4  Platform layer, storage, logging, input table, strings
  Phase 5  CMake vita target, LiveArea, .vpk, Vita3K smoke test
  Phase 6  TESTPLAN.md, hand to the maintainer, fix what hardware reveals

Do not skip Phase 3. It is the only phase that produces evidence.


==============================================================================
11. KNOWN OPEN ITEMS
==============================================================================

 11.1  RESOLVED 2026-08-01. Vita SDL2 controller button indices are confirmed on
       hardware; the shipped table is correct and unchanged. History below.

       Original item: indices are unverified until the maintainer runs the L+R
       diagnostic. Design around this rather than guessing.

       NARROWED 2026-07-31 (Phase 4). Not guessed, and not taken from the web:
       the table in platform_vita.c was read out of the libSDL2.a this build
       links, by disassembling VITA_JoystickUpdate and pairing each SCE_CTRL
       bit it tests with the index it hands SDL_PrivateJoystickButton, then
       cross-checking the bit values against psp2common/ctrl.h. Result:

         0 triangle   4 L1     8 up        12 L2 / left trigger
         1 circle     5 R1     9 right     13 R2 / right trigger
         2 cross      6 down  10 select    14 L3
         3 square     7 left  11 start     15 R3

       The driver polls with sceCtrlPeekBufferPositive2, which binds the
       physical shoulder buttons to L1/R1 rather than the trigger bits, so
       L and R arrive as 4 and 5.

       This is the binary's account of its own behavior, not an observation of
       hardware, so the item stays OPEN and the L+R diagnostic still ships.
       the maintainer confirms once on device; a disagreement is a one-line fix to
       g_buttons in src/platform/platform_vita.c.

       CONFIRMED ON HARDWARE 2026-08-01 (Phase 6, TESTPLAN item 4), except
       cross. Every index the game binds matched the disassembly - triangle 0,
       circle 1, square 3, L 4, R 5, down 6, left 7, up 8, right 9, start 11 -
       both in the panel and in the log's held-button masks, which agree
       independently.

       Cross could not be probed: pressing it started the game and closed the
       panel. Fixed the same day - buttons now only report while L+R are held
       (src/shell/input.c, tests/test_input.c).

       the maintainer re-ran item 4 on the fixed build the same day: cross reports 2.
       Every index the game binds is now observed on a device and matches what
       was read out of libSDL2.a, so the disassembly is vindicated and nothing
       in g_buttons changes. Item closed.
 11.2  RESOLVED 2026-07-31. SDL2_ttf IS available for the Vita target:
       $VITASDK/arm-vita-eabi/lib holds libSDL2_ttf.a and libfreetype.a. The
       bitmap-font fallback in 6.4 is not needed; build 6.4's preferred TTF
       path. This closes the contingency, but note the font still has to be
       bundled and licensed as 6.4 describes.
 11.3  Grid dimensions will differ from the browser original because the Vita
       screen is fixed at 960x544. This is unavoidable; document it.
 11.4  Several original mechanics (premove queue depth, tail-vacate collision,
       speed ramp) are subtle and easy to get almost-right. They are the
       difference between "a snake game" and "that snake game." Test them
       explicitly.
 11.5  RESOLVED 2026-07-31 (Phase 5). TITLE_ID SNEK00001 is free on VitaDB.

       Method: the canonical host vitadb.rinnegatamante.it is no longer the
       database - it now serves a parked "this domain is for sale" page. The
       live backend is www.rinnegatamante.eu/vitadb/, which is what the
       official VitaDB-Downloader client fetches from. Pulled all three
       catalogues from there and scanned every titleid field:

         list_hbs_by_titleid.php   1022 homebrew entries, all with a titleid
         list_tools_json.php         27 tools    (no titleid field)
         list_plugins_json.php      128 plugins  (no titleid field)

       No entry has titleid SNEK00001, and none uses the SNEK prefix at all.
       The nearest snake-related IDs in use are SNAKO0002 ("Snake!"),
       GRZB00002 (vitaSnake), ARIEL0001 (RejuveSnake) and JJH000002 ("Snake").
       Retail titles use PCS-family prefixes, so SNEK cannot collide there.

       Caveat: VitaDB is a voluntary catalogue, not a registry. It cannot
       prove an unlisted homebrew is not using the ID. This is as strong a
       check as exists.

       Changing the ID later still means touching exactly two places:
       VITA_TITLEID in CMakeLists.txt and the ux0:app path in docs/DEPLOY.md -
       keep it that way and do not scatter the literal.
