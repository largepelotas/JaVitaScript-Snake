# Plan: themes, cycled with Triangle

Post-v1 work. v1 shipped one theme because PLAN.md 0.6 fixed the scope, and the
theme table was written as data specifically so this stays a data change
(`src/shell/render.h`, "themes must be data"). This plan is what that change
actually costs, which is slightly more than a table row: one of the three themes
uses a colour the current `Theme` struct has no field for.

Nothing here is invented. Every value below was read out of `reference/`, and
the ones that live in images were sampled from the images rather than guessed.

**Revised 2026-08-03**, after the §10 questions were answered. The plan
originally proposed four themes and two new struct fields; Dark and `snake_head`
were both cut. §10 records what was decided and the measurements behind it.

## 1. Scope

Themes come from two places, and the table records which:

| Index | Name | Author | Origin |
|---|---|---|---|
| 0 | Main | patorjk | `css/main-snake.css` (default, already shipped) |
| 1 | Matrix | Geahad Haymor | `css/matrix-snake.css` |
| 2 | Original | DylanLCrocker | `css/blue-snake.css` |
| 3 | Vita | largepelotas | original to this port — §12 |

Indices 0–2 are **copied from the reference and credited to the people who
contributed them there**. Their labels and authors are the reference's own,
from the `THEMES` array at `reference/src/index.html:90-104`. "Original Theme
by DylanLCrocker" maps to `blue-snake.css`, which is worth writing down because
the filename does not say so.

Index 3 is **original to this port, written by its developer**, and cites no
stylesheet because there is none. §12 covers what that changes and what it does
not.

Out of scope: the other eleven themes in that fourteen-entry array — including
Dark, cut in §10 — and the per-theme block artwork as artwork, see §3.

## 2. Extracted colours

Sampled 2026-08-01. Image-derived values are the **centre pixel** of the block,
which is the convention the shipped Main theme already follows: `snakeblock.png`
is a 20x20 tile whose outer ring is the playfield colour showing through, so the
centre is the body colour and the mean is not.

| Field | Main | Matrix | Original |
|---|---|---|---|
| `background` | `#FC5454` | `#00FF11` | `#004620` |
| `playfield` | `#0000A8` | `#000000` | `#149C36` |
| `snake` | `#FCFC54` | `#00C848` | `#FCFC54` |
| `snake_dead` | `#C0C0C0` | `#C0C0C0` | `#C0C0C0` |
| `food` | `#FF0000` | `#E80015` | `#CF2121` |
| `hud_text` | `#FFFFFF` | `#000000` | `#FFFFFF` |
| `overlay_bg` | `#000000` | `#000000` | `#000000` |
| `overlay_text` | `#FFFFFF` | `#00FF11` | `#FFFFFF` |
| `overlay_text_end` (new) | `#FFFFFF` | `#FF0000` | `#FFFFFF` |
| `pause_bg` (new) | `#000000` | `#000000` | `#004620` |
| `pause_text` (new) | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` |
| `button_border` | `#FFFFFF` | `#00FF11` | `#FFFFFF` |

Provenance, per column:

- **Main** — unchanged, already cited in `src/shell/render.c`; `pause_bg` and
  `pause_text` from `main-snake.css:28-29`.
- **Matrix** — `matrix-snake.css:1` body, `:54` playing field, `:29` panel,
  `:63` welcome, `:69` end-game, `:25-26` pause screen. `snake` is the centre of
  `matrix-snake-block.png` (mean `#00C349`), `food` the centre of
  `matrix-food-block.png` (mean `#DA000C`). Dead blocks fall back to the shared
  `deadblock.png`, hence `#C0C0C0`.
- **Original** — `blue-snake.css:1` body (`rgb(0,70,32)`), `:58` playing field
  (`rgb(20,156,54)`), `:52` food (`rgb(207,33,33)`), `:23` panel, `:63` and
  `:69` dialogs, `:19-20` pause screen. The body tile is the shared
  `snakeblock.png`, so the body is the same yellow as Main.

One provenance note that does not change any value: Original sets
`.snake-snakebody-block { background-color: #247FB4 }` (`blue-snake.css:41`),
a blue that shows through `snakeblock.png`'s transparent outer ring where Main
shows its playfield. Since this port flattens each tile to its centre pixel, the
ring is not modelled in either theme — it falls under the block-artwork
deviation in §3.

## 3. Struct changes

Three new colour fields, each required by a real theme rather than speculative,
plus the attribution string from §6:

```c
SDL_Color   overlay_text_end; /* Matrix's death/win dialogs are red   */
SDL_Color   pause_bg;         /* Original's pause screen is not black */
SDL_Color   pause_text;       /* Matrix's pause text is not its green */
const char *author;           /* credited in the welcome dialog       */
```

For themes that do not distinguish a new field from the old one it replaces, the
table simply repeats the value — that is the point of a table.

`pause_bg` and `pause_text` were missed when this plan was first written, and
they are worth the space because `draw_paused` currently borrows `overlay_bg`
and `overlay_text`, which is right for Main and wrong for both new themes.
The reference styles `.snake-pause-screen` separately from the welcome and
end-game dialogs, and all three disagree:

| Theme | pause | welcome | end-game |
|---|---|---|---|
| Main | `#000000` / `#FFFFFF` | `#000000` / `#FFFFFF` | `#000000` / `#FFFFFF` |
| Matrix | `#000000` / `#FFFFFF` | `#000000` / `#00FF11` | `#000000` / `#FF0000` |
| Original | `#004620` / `#FFFFFF` | `#000000` / `#FFFFFF` | `#000000` / `#FFFFFF` |

Matrix earns `pause_text` — its pause screen is white, not its dialog green.
Original earns `pause_bg` — its pause screen is the same dark green as its body
background, not black. Main needs neither, which is why the omission was
invisible in v1.

Note what this does **not** need: no change to `render_frame`'s drawing of the
snake. With `snake_head` cut (§10), the board is still drawn purely from cell
occupancy, exactly as today. Step 1 of §11 is therefore a pure data change.

**Deliberately not modelled**, and each becomes a row in MECHANICS §10:

- **Block artwork.** Every theme draws its snake as a tiled PNG with an inner
  gradient or texture. v1 already flattens `snakeblock.png` to one colour;
  extending that to two more themes is consistent, not a new deviation, though
  Matrix loses the most (its tiles are textured, not flat).
- **Rounded food corners.** Original gives food `border-radius: 6px`
  (`blue-snake.css:55`). At a 20px block that is a visible but purely decorative
  rounding, and modelling it would mean a non-rectangular blit for one theme.

Borders are no longer a deviation at all: with Dark cut, every remaining
stylesheet sets `border: 0px` on the body block, food block and playing field
(`main-snake.css:40,53,58`, `matrix-snake.css:38,50,56`,
`blue-snake.css:35,54,60`). The original draws no borders here, and neither do
we.

## 4. Selection and where the state lives

**Triangle cycles the theme, on any screen.** Index 0 on the Vita, confirmed on
hardware and currently unbound (PLAN.md 11.1). Desktop: `T`, plus `--theme
<name>` for the screenshot targets.

Unlike difficulty, this is *not* welcome-screen-only, and that matches the
reference: the theme `<select>` swaps the stylesheet the moment it changes
(`index.html:183-190`), at any point in a game.

**The theme must not enter `src/core/`.** The core is deterministic and has no
render concept; a theme change must not touch `game_state_hash` or any replay.
So, unlike `ACTION_CYCLE_MODE`, there is no `GameAction` for this:

- `InputState` gains `int theme_delta`, incremented on Triangle / `T`.
- `run_windowed` consumes and zeroes it each frame, advances `rc.theme`, and
  saves.

That keeps `input_handle`'s signature and the core untouched. The alternative —
passing a shell-side context into `input_handle` — churns every call site in
`tests/test_input.c` for no gain.

## 5. Persistence

**No save-record version bump.** The v2 record's meta word already reserves
bytes 6 and 7 as zero (`src/shell/score.c` header comment), and the checksum
covers the whole word. Byte 6 becomes the theme index:

- A v2 record written by the current build has byte 6 = 0 = Main, which is
  exactly the right default.
- A build without themes reading a themed record still validates: it checksums
  the same meta word and ignores byte 6.

So the format is compatible in both directions with no version change. An
unknown theme index falls back to Main while keeping the highscore, mirroring
what `score_load` already does for an unknown mode. This now matters slightly
more than when the plan was written: a record saved by a hypothetical build that
kept Dark at index 1 would land on Matrix here, so the clamp is what keeps an
out-of-range or stale index harmless.

`score_load`/`score_save` grow a third parameter. Both already accept `NULL`
outputs, so the callers that do not care stay honest.

## 6. Attribution

The copied themes are other people's contributions to an MIT project, and the
reference credits each one by name in its dropdown. So should we. The original
themes are credited by the same mechanism, so a player is told whose work they
are looking at without having to know which kind it is:

- `Theme` gains `const char *author`.
- The welcome dialog's small credit block gains a second line:
  `Main theme by patorjk` / `Matrix theme by Geahad Haymor` / etc.
- The welcome line itself stays short — `Theme: Matrix - TRIANGLE to change` —
  because the dialog content column is 300px (`DIALOG_CONTENT_W`) and
  `Theme: Original by DylanLCrocker - TRIANGLE to change` does not fit at 14px.

The welcome dialog then carries: title, instructions, difficulty, theme, button,
MIT credit, theme credit. That is two more lines than today; the box is sized
from its content, so it grows on its own.

## 7. Tests

Host-side, in the existing binaries:

- `tests/test_core.c` — one negative test: cycling the theme is impossible
  through `game_action`, and `game_state_hash` is unchanged by anything this
  feature does. The core should not learn the word "theme".
- `tests/test_input.c` — Triangle and `T` advance `theme_delta`; wrap at
  `theme_count()`; key repeat ignored; Triangle inert while the L+R diagnostic
  is up (same gate as Square); cycling works in `PLAYING` as well as `WELCOME`,
  which is the difference from difficulty.
- `tests/test_score.c` — round trip for every theme; byte 6 written where
  claimed; a themed record read by the two-parameter path still yields the right
  highscore and mode; an out-of-range theme falls back to Main and keeps the
  highscore; a v1 record still yields Main.
- `tests/test_render.c` (new, small) — every theme has a distinct name and
  author, no `NULL`s, and `theme_get` clamps at both ends. Cheap, but it is what
  catches a table row added with a missing field.

## 8. Screenshots

`make shots` gains a welcome, playing, paused and dead shot per theme — four
pictures each, so the count grows with the table — driven by the existing
blessed replays with `--theme` added, so the pictures stay tied to runs the
tests already verify. Paused and dead are here because `pause_bg`, `pause_text`
and `overlay_text_end` appear in no other frame.

`tests/check_layout.sh` asserts geometry by probing pixels for specific colours
(`0000A8` for the playfield, and so on). Those probes must keep running against
Main only — otherwise every theme needs its own copy of the geometry table for
no extra confidence. Per-theme shots get a much smaller check: the playfield
centre pixel is that theme's playfield colour, and the HUD strip is its
background colour. That catches a mis-transcribed table row, which is the actual
risk here.

## 9. Documentation

- MECHANICS.md — a new §8.6 with the table from §2 and its citations; §10 rows
  for flattened block art and Original's rounded food corners.
- README.md — Controls (Triangle / `T`), the theme list with authors, and the
  save-data row.
- TESTPLAN.md — one new item: cycle through every theme on hardware, check
  each is legible on the device screen, and confirm the choice survives a
  relaunch.
- PLAN.md 0.6 says v1 is fixed at one theme. This is post-v1, so 0.6 needs a
  line saying so rather than being quietly contradicted.

## 10. Decisions

Settled 2026-08-03. The three questions this section used to ask, and what was
decided:

1. **Dark's contrast — Dark is cut.** Measured WCAG contrast of snake against
   playfield, which is what decides whether the snake is findable on the board:

   | Theme | Snake on playfield | Contrast |
   |---|---|---|
   | Main | `#FCFC54` on `#0000A8` | 12.26:1 |
   | Matrix | `#00C848` on `#000000` | 9.36:1 |
   | Original | `#FCFC54` on `#149C36` | 3.29:1 |
   | Dark | `#15241F` on `#312E44` | **1.23:1** |

   Two mitigations this plan originally offered do not survive measurement.
   Using the tile's mean `#283532` instead of its centre was described as
   lifting the contrast slightly; it does the opposite, dropping it to
   **1.03:1**, because the mean sits closer to the playfield colour. And
   "add the border the CSS asks for" does not apply to the snake:
   `dark-snake.css:46` sets `.snake-snakebody-block { border: 0px solid black }`.
   The 3px and 2px borders in that file are on the playing field and the food.

   So Dark is dark-on-dark in the browser too, and only the JPEG tile's texture
   separates it from the board — precisely what this port flattens away. Making
   it playable would mean inventing an outline the original does not have.
   Rather than ship a theme that is hard to play or a theme that is not the one
   its author wrote, it is cut. Its colours stay recorded in this file's history
   if it is ever revisited.

2. **`snake_head` is cut.** It existed for exactly one theme. Original's snake
   is now uniformly yellow, which is what its shared body tile actually is. This
   removes a struct field, a per-frame rect, and the separate head pass in
   `render_frame` — see §3 and the revised §11 step 2.

3. **Cycle order: as listed in §1.** Main stays at index 0, which the save
   record's default byte and every existing screenshot already assume. With
   Dark cut, the order is Main, Matrix, Original. The reference's own dropdown
   order was rejected: with eleven of fourteen themes missing it has gaps, and
   it would move Main off index 0 for no benefit.

## 11. Work order

1. Struct fields (`overlay_text_end`, `author`), the three-row table, and the
   `theme_get` clamp. With `snake_head` cut this is a pure data change — no
   drawing code moves.
2. Regenerate Main's shots and confirm they are byte-identical, proving step 1
   was inert. Cheaper than the original step 2, which had to prove a reworked
   `render_frame` still drew Main the same way.
3. `--theme`, then per-theme shots. **Look at all six pictures before wiring any
   input**, which is the same order Phase 3 used and the reason it worked.
4. Input: `theme_delta`, Triangle, `T`, diagnostic gate.
5. Persistence: byte 6, `score_load`/`score_save` signatures, tests.
6. Welcome-dialog theme line and attribution line.
7. Docs, then the Vita build, then a TESTPLAN item for the next hardware run.

Exit criteria: all three themes screenshotted and eyeballed, `make test` green
including the new cases, replay hashes unchanged (proving the core never saw
this), and the `.vpk` built.

## 12. Original themes

Added 2026-08-03, with Vita (index 3) as the first.

Every theme before this one is a transcription: §2 cites the stylesheet line or
the sampled pixel behind each colour, and that citation is the reason to believe
the port rather than take its word. An original theme has no such line. The
temptation is to give it one anyway — to invent a `vita-snake.css` and file it
alongside the rest — and that is precisely the thing not to do. A reader who
goes looking for that file must not find a plausible-looking lie.

So the rule is:

- **The colours are stated as chosen, not cited.** `render.c`'s table comment
  for an original theme says so in as many words, and points here.
- **The structure is borrowed, and the borrowing is cited.** Which selector
  feeds which struct field is a solved problem for any theme already
  transcribed, so an original theme reuses one of those mappings instead of
  inventing a fourth. The cited line numbers name the *role* each colour plays
  in the template, not its value.
- **The credit line does not distinguish them.** `Vita theme by largepelotas`
  reads exactly like `Matrix theme by Geahad Haymor`, because from the player's
  side both are true and the difference is a provenance question, not a
  gameplay one. §6's mechanism needed no change.

### Vita

Structure from `blue-snake.css`, already transcribed once as Original, so the
mapping below is the proven one and only the colours are new.

| Field | Value | Template line | Role |
|---|---|---|---|
| `background` | `#0B5FA5` | `:2` | LiveArea blue |
| `playfield` | `#01203F` | `:59` | deep navy |
| `snake` | `#FFFFFF` | `:41` | body block |
| `snake_dead` | `#C0C0C0` | — | shared `deadblock.png`, as every theme |
| `food` | `#E4373E` | `:53` | PlayStation red |
| `hud_text` | `#FFFFFF` | `:26` | panel |
| `overlay_bg` | `#000000` | `:64` | welcome background |
| `overlay_text` | `#FFFFFF` | `:65` | welcome colour |
| `overlay_text_end` | `#FFFFFF` | `:72` | try-again / win colour |
| `pause_bg` | `#0B5FA5` | `:19` | body colour, as the template does |
| `pause_text` | `#FFFFFF` | `:20` | |
| `button_border` | `#FFFFFF` | — | |

Measured on §10's method, against the values it published:

| Pair | Vita | Best shipped | Worst shipped |
|---|---|---|---|
| snake / playfield | **16.41:1** | Main 12.26 | Original 3.29 |
| food / playfield | **3.84:1** | Matrix 4.44 | Original 1.50 |
| snake / food | 4.27:1 | Original 4.93 | Matrix 2.11 |

An original theme has no author to be faithful to, so nothing argues for
shipping one that measures badly — §10 cut Dark at 1.23:1 and only faithfulness
had spoken for it. Vita clears every pair.

Two numbers §10 never took, recorded here because taking them turned something
up: it measured the snake alone, and **Original's food sits at 1.50:1 against
its own playfield** — below the 1.23:1 that cut Dark. It is faithful to
`blue-snake.css:53` and `:59`, so it stays; but a future original theme has no
such defence and should clear both pairs.
