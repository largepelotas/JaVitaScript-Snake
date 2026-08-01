# Vita Snake

A PS Vita homebrew port of [patorjk's JavaScript Snake](https://patorjk.com/games/snake/),
built for a hacked Vita running Ensō / HENkaku / h-encore.

**This is a derivative work.** The original *JavaScript Snake* is by
[Patrick Gillespie](https://patorjk.com), MIT licensed. Its license is at
[`third_party/JavaScript-Snake-LICENSE`](third_party/JavaScript-Snake-LICENSE).

The port was implemented from [`docs/MECHANICS.md`](docs/MECHANICS.md), a
reverse-specification of the original with line citations against a pinned
upstream commit. Every constant in `src/core/` traces back to that document,
not to guesswork.

## Modes

| Mode | Step interval | Wall collision |
|---|---|---|
| Easy | 130 ms | no (wrap) |
| Medium | 75 ms | yes |
| Hard | 30 ms | yes |

Cycle difficulty on the welcome screen with Square (Vita) or M / Tab (desktop).
The choice persists across launches.

## Controls

| Action | Vita | Desktop |
|---|---|---|
| Move | D-Pad or left stick | Arrow keys or WASD |
| Start / play again | Cross | Space, Enter, or Z |
| Pause | START | P or Escape |
| Back to welcome screen | Circle | Backspace or X |
| Cycle difficulty (welcome only) | Square | M or Tab |
| Quit | — | Q, or close the window |

## Known deviations from the original

The full list with rationale is in [`docs/MECHANICS.md`](docs/MECHANICS.md) §10.
Player-visible differences:

- **Fixed 46×24 grid.** The Vita screen is 960×544; the original sizes its grid
  from the browser window. Applying the original's own layout formula to
  960×544 yields a 920×480 playfield at the original 20 px block size — a
  46×24 interior — with a 44 px HUD strip. Block size and proportions are
  preserved; only the extent differs.
- **Medium starts at 75 ms from the very first game.** The original starts at
  80 ms and only picks up the mode's 75 ms after the first death or a dropdown
  change, so the first game of every session runs slightly slow. That is a bug;
  shipping it would make Medium wrong exactly once per launch.
- **Win is decided by counting free cells**, not by giving up after 20 000
  failed food placements. The outcomes are identical to about 1 in e^18; this
  version is deterministic and cannot hang.
- **Food cannot spawn under the snake's head.** In the original it can, with
  probability 1/1104, silently producing an unwinnable board.
- **Controls and prompts are rewritten for the Vita** — D-Pad or left stick,
  Cross to start, START to pause.
- **Difficulty persists.** The original does not remember the choice across a
  reload; saving it is a handheld affordance.

## Building

### Host (desktop, for development and testing)

Requires SDL2 and SDL2_ttf via `pkg-config`.

```sh
make            # build core tests, replay harness, and the desktop binary
make test       # run unit tests, input tests, save-record tests, and replays
./build-host/snake                   # play on the saved difficulty
./build-host/snake --mode medium     # override for this session only
make shots      # headless screenshots into artifacts/, with pixel assertions
make parity     # run replay scripts through the SDL shell and check hashes
```

### Vita

Requires [VitaSDK](https://vitasdk.org/) with SDL2 and SDL2_ttf.

```sh
cmake -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
cmake --build build-vita
# produces build-vita/snake.vpk
```

The CMake file also drives the host build:

```sh
cmake -B build-cmake-host && cmake --build build-cmake-host
```

LiveArea artwork is checked in under `sce_sys/`. To regenerate it from the
game's own palette:

```sh
make livearea
```

## On-device paths

| What | Path |
|---|---|
| Save data (highscore + difficulty) | `ux0:data/VitaSnake/highscore.dat` |
| Log | `ux0:data/VitaSnake/log.txt` |
| Bundled font | `app0:assets/font.ttf` |

The log is flushed after every write and survives a lock-up.

**Button-index diagnostic.** Hold L + R on the welcome screen to display a
panel listing every joystick button index currently pressed, alongside what
this build maps each index to. This is the only way to verify the button
table on hardware without modifying the code. While the panel is visible,
buttons are probed only — no game actions fire.

## Layout

| Path | Contents |
|---|---|
| `src/core/` | Pure C99 game logic — no SDL, no I/O, fully deterministic |
| `src/shell/` | SDL2 rendering, input, timing loop — identical on both targets |
| `src/platform/` | The only place target differences live |
| `tests/` | Unit tests, replay harness, and headless screenshot layout checks |
| `tools/` | Host-only dev utilities (not part of the shipped build) |
| `assets/` | Bundled font and its license |
| `sce_sys/` | LiveArea artwork and metadata, packaged into the `.vpk` |
| `docs/` | `MECHANICS.md` — extracted game spec with citations |
| `third_party/` | License for the original JavaScript Snake |

## License

The original's MIT license is at `third_party/JavaScript-Snake-LICENSE` and
covers the gameplay design this port derives from.

The bundled font (`assets/font.ttf`) is DejaVu Sans, unmodified, under the
Bitstream Vera Fonts Copyright — see `assets/font-LICENSE.txt`.
