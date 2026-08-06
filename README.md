# JaVitaScript Snake

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

## Themes

Cycled with Triangle (Vita) or T (desktop) on any screen — not just the welcome
one, which matches the original, where changing the theme swaps the stylesheet
mid-game. The choice persists.

Themes come from two places, and the difference is worth keeping straight.
Most are **copied from the original website and credited to the people who
contributed them there**; the rest are **original to this port, written by its
developer**. The welcome dialog names the theme's author either way, so a
player always sees whose work they are looking at.

| Theme | Author | Origin |
|---|---|---|
| Main | patorjk | `main-snake.css` (the original's default) |
| Matrix | Geahad Haymor | `matrix-snake.css` |
| Original | DylanLCrocker | `blue-snake.css` |
| Vita | largepelotas | original to this port |

For the copied themes, the names and authors are the reference's own, taken
from its theme dropdown, and every colour was read out of the stylesheet or
sampled from the block image rather than chosen — the table with citations is
in [`docs/MECHANICS.md`](docs/MECHANICS.md) §8.6.

The original themes cite no stylesheet, because there is none to cite. They
borrow an existing theme's *structure* — which selector feeds which field — so
the mapping stays one that has already been proven, and only the colours are
new. Vita is built on `blue-snake.css`'s structure, the same one Original was
transcribed from. Its rationale and measurements are in
[`docs/PLAN-THEMES.md`](docs/PLAN-THEMES.md) §12.

## Controls

| Action | Vita | Desktop |
|---|---|---|
| Move | D-Pad or left stick | Arrow keys or WASD |
| Start / play again | Cross | Space, Enter, or Z |
| Pause | START | P or Escape |
| Back to welcome screen | Circle | Backspace or X |
| Cycle difficulty (welcome only) | Square | M or Tab |
| Cycle theme (any screen) | Triangle | T |
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
- **Three of the original's fourteen themes, plus one this port added**, and
  the copied ones have their block artwork flattened to one colour per element
  rather than tiled with the original's textured images. Dark is deliberately
  absent: flattening its tile leaves its snake at a 1.23:1 contrast ratio
  against its own playfield, which is not playable on a handheld screen. See
  [`docs/MECHANICS.md`](docs/MECHANICS.md) §8.6.

## Installing (no build required)

Grab `snake.vpk` from the [Releases page](../../releases) and install it with
VitaShell. A new release is built automatically whenever a `vX.Y.Z` tag is
pushed (see `.github/workflows/release-vita.yml`).

The bubble reads **JaVitaScript Snake** — renamed from "Snake" in v1.1.2.
Upgrading over an earlier build keeps the same title ID, so the install
replaces it in place: one bubble, with the new label, and the highscore and
theme in `ux0:data/VitaSnake/` untouched.

## Building

### Host (desktop, for development and testing)

Requires SDL2 and SDL2_ttf via `pkg-config`.

```sh
make            # build core tests, replay harness, and the desktop binary
make test       # run unit, input, save-record, theme-table tests, and replays
./build-host/snake                   # play on the saved difficulty and theme
./build-host/snake --mode medium     # override for this session only
./build-host/snake --theme matrix    # likewise; not written back to the save
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

LiveArea artwork is hand-made and checked in under `sce_sys/`. Replacing it is
a matter of dropping in new files, but they must be **8-bit indexed PNGs**
(colour type 3). That is not a size optimisation: RGBA made VitaShell abort the
install on hardware with `0x8010113D`, while Vita3K installed the same `.vpk`
without complaint — so an emulator will not catch it. `icon0.png` is 128×128,
`livearea/contents/bg.png` 840×500, `livearea/contents/startup.png` 280×158.
[`docs/DEPLOY.md`](docs/DEPLOY.md) has a one-liner that checks an image before
you package it.

## On-device paths

| What | Path |
|---|---|
| Save data (highscore + difficulty + theme) | `ux0:data/VitaSnake/highscore.dat` |
| Log | `ux0:data/VitaSnake/log.txt` |
| Bundled font | `app0:assets/font.ttf` |

The log is flushed after every write and survives a lock-up. Its first line is
the build's version:

```
VitaSnake v1.1.1
```

That line is the only place on the device where the full version appears.
`param.sfo` holds `APP_VER` as `XX.YY` — major and minor, two digits each — so
the LiveArea bubble and Content Manager show `1.01` for every 1.1.x build and
cannot tell v1.1.0 from v1.1.1. A bug report that quotes this line names its
build exactly; one that quotes the bubble does not.

## Version

The version is written down once, in [`VERSION`](VERSION), as
`MAJOR.MINOR.PATCH`. Everything else derives from it: `CMakeLists.txt` computes
`param.sfo`'s two-field `APP_VER` and defines `SNAKE_VERSION` for the startup
log, and the Makefile reads the same file for the host build.

Releasing means editing `VERSION`, committing, and pushing a matching `v` tag.
If the two disagree, `release-vita.yml` fails the release before it builds
anything — including the case the two-field `APP_VER` cannot catch on its own,
where a v1.1.0 tag is pushed at a v1.1.1 tree and both would stamp `01.01`.

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

This port is **MIT licensed** — see [`LICENSE`](LICENSE). That covers what is
original here: the C implementation in `src/`, the tests and tools, the
documentation in `docs/`, and the Vita theme.

Two components come from elsewhere and keep their own licenses:

- **The original *JavaScript Snake*** by Patrick Gillespie, MIT licensed,
  covering the gameplay design this port derives from and the three themes
  copied from it. Its license is at
  [`third_party/JavaScript-Snake-LICENSE`](third_party/JavaScript-Snake-LICENSE)
  and must travel with any redistribution.
- **The bundled font** (`assets/font.ttf`), DejaVu Sans, unmodified, under the
  Bitstream Vera Fonts Copyright — see `assets/font-LICENSE.txt`.

All three are permissive and mutually compatible, so the `.vpk` as a whole is
free to use, modify and redistribute provided the three notices are kept.
