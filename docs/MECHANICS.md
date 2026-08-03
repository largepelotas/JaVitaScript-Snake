# MECHANICS - extracted specification

This is the single source of truth for `src/core/`. Implement from this file,
not from `snake.js` directly. If this file turns out to be wrong, fix this file
first, then the code (PLAN.md 3.4).

**Reference:** [patorjk/JavaScript-Snake](https://github.com/patorjk/JavaScript-Snake)
by Patrick Gillespie, MIT licensed. This project is a derivative work.

**Pinned at commit** `c2f26f9b3aab4c0e14fb54a0213089815c0673af` (branch `main`,
2026-07-25, "Adjust speed for mobile (#148)"). Every citation below is a line
number in that commit. Re-verify all citations if the reference is ever updated.

Citations are written `snake.js:NNN` for `reference/src/js/snake.js`, and name
the file explicitly otherwise.

---

## 1. Direction encoding

Adopt the original's numbering exactly — the collision and reversal rules are
arithmetic on these values, so renumbering silently breaks them.

| Name | Value | Row shift | Col shift |
|---|---|---|---|
| `MOVE_UP` | 0 | -1 | 0 |
| `MOVE_RIGHT` | 1 | 0 | +1 |
| `MOVE_DOWN` | 2 | +1 | 0 |
| `MOVE_LEFT` | 3 | 0 | -1 |
| `MOVE_NONE` | -1 | — | — |

`snake.js:23-27` (constants), `snake.js:160-161`:
```js
const columnShift = [0, 1, 0, -1];
const rowShift    = [-1, 0, 1, 0];
```

Rows increase downward. Opposite directions differ by exactly 2, which is what
makes the reversal test in §4.3 work.

---

## 2. Grid

| Item | Value | Citation |
|---|---|---|
| Block size | 20 x 20 px | `snake.js:759-760` |
| Max board | 250 x 250 blocks | `snake.js:757-758` |
| Food grid value | `-1` (must be negative) | `snake.js:761` |
| Edge cell value | `1` | `snake.js:1252` |
| Empty cell value | `0` | `snake.js:1254` |
| Snake cell value | `1` | `snake.js:400` |
| Playfield border/frame | **none** — `border: 0px` | `css/main-snake.css:56-59` |

### 2.1 How the grid is derived

`snake.js:1174-1186, 1240-1241`:

```
wEdgeSpace   = blockWidth * 2 + (cWidth % blockWidth)
fWidth       = min(250*blockWidth - wEdgeSpace, cWidth - wEdgeSpace)
hEdgeSpace   = blockHeight * 3 + (cHeight % blockHeight)     // non-compact
fHeight      = min(250*blockHeight - hEdgeSpace, cHeight - hEdgeSpace)

numBoardCols = fWidth  / blockWidth  + 2
numBoardRows = fHeight / blockHeight + 2
```

The `+ 2` is the wall ring. Row 0, col 0, row `numBoardRows-1` and col
`numBoardCols-1` are set to `1` (wall); everything else starts `0`
(`snake.js:1243-1257`). **Playable interior is rows 1..numBoardRows-2 and cols
1..numBoardCols-2.**

The wall is *virtual* — it exists only as grid values. Nothing is drawn for it.
Visually the boundary is simply where the playfield background ends.

A block at (row, col) is drawn at `left = col * 20`, `top = row * 20` relative to
the **container**, not the playfield (`snake.js:219-227`). The playfield div sits
at `left = 20, top = 20` (`snake.js:1192-1193`), so interior cell (1,1) lands
exactly on the playfield's top-left corner and the interior tiles it precisely.

### 2.2 Board size actually used

The live page runs `fullScreen: true` (`js/init.js:12`), so the grid depends on
browser window size. The 580x400 in `reference/README.md:23-28` is a usage
example in the reference's own docs — and it is explicitly `fullScreen: false`
there — **not** the live configuration. `SNAKE.Board`'s own defaults are 400x400
(`snake.js:1423-1425`) and are also unused by the live page.

This is why PLAN.md 11.3 is unavoidable: there is no single "correct" grid to
copy. See §9 for the Vita grid.

---

## 3. Snake

| Item | Value | Citation |
|---|---|---|
| Starting length | 1 | `snake.js:207` |
| Starting row, col | 2, 2 | `snake.js:804-810` via `895-901` |
| Starting direction | none; `lastMove` = 1 (RIGHT) | `snake.js:164-167` |
| Moves immediately? | **No** — waits for first direction input | `snake.js:1266-1339` |
| Growth per food | **5** | `snake.js:159`, `snake.js:426-447` |
| Head vs body rendering | **no difference in any theme this port carries** | see §7.3, §8.3, §8.6 |

`SNAKE.Snake` defaults `startRow`/`startCol` to 1 (`snake.js:197-198`), but
`SNAKE.Board` always passes 2 (`snake.js:804-810`, `895-901`). **The effective
start is (2,2)** — one cell in from the top-left playable corner.

Body storage is a circular doubly-linked list recycled per move: the tail block
is unlinked and becomes the new head (`snake.js:344-347`). A ring buffer is
behaviorally identical and is what `src/core/` should use.

Growth is implemented by splicing 5 pooled blocks in at the tail
(`snake.js:430-447`). Fresh blocks carry `row = col = -1` (`snake.js:108-109`),
so they occupy no grid cell until they cycle around to become the head. The
practical effect: after eating, the tail does not advance for 5 moves.

---

## 4. Movement and input

### 4.1 Step interval per mode

From the mode dropdown, `src/index.html:155-161`:

| Mode | ms per step | In v1 scope |
|---|---|---|
| Easy | 100 | yes |
| Medium | 75 (dropdown default) | yes |
| Hard | 50 | yes |
| Impossible | 25 | no (PLAN.md 0.6) |
| Rush | 110 | no (PLAN.md 0.6) |

Related constants: `DEFAULT_SNAKE_SPEED = 80`, `MIN_SNAKE_SPEED = 25`,
`RUSH_INCR = 5` (`snake.js:29-32`).

**Quirk — first game runs at 80ms, not 75ms.** `snakeSpeed` initialises to
`DEFAULT_SNAKE_SPEED` = 80 (`snake.js:168`) and is only re-read from the dropdown
on a `change` event (`snake.js:174-191`) or on death (`snake.js:472-478`). The
dropdown's `selected` option is Medium = 75. So a player who never touches the
dropdown plays their first game at 80ms and every later game at 75ms. This is a
bug in the original. **Do not port it** — see §10.

### 4.2 Speed vs. snake length

**The step interval does not change as the snake grows** in Easy/Medium/Hard.
The only speed ramp is Rush mode, which decrements by `RUSH_INCR` per food down
to a floor of `MIN_SNAKE_SPEED` (`snake.js:453-463`), and Rush is out of scope.

### 4.3 Reversal rejection

`snake.js:295-305`:
```js
me.setDirection = (direction) => {
  if (currentDirection !== lastMove) {
    preMove = direction;              // queue depth 1
  }
  if (Math.abs(direction - lastMove) !== 2 || isFirstGameMove) {
    currentDirection = direction;
    isFirstGameMove = false;
  }
};
```

**The reversal test is against `lastMove` — the last direction actually applied
to a move — not against `currentDirection`.** This is the specific question
PLAN.md 3.3 raises, and the distinction matters: `currentDirection` may already
hold an un-applied turn, and testing against it would wrongly reject valid input.

`isFirstGameMove` (`snake.js:166`, reset in `rebirth`, `snake.js:492-496`) lets
the very first input of a game bypass the check, so the player may open by moving
LEFT even though `lastMove` initialises to RIGHT.

### 4.4 Premove queue

**Depth is exactly 1.** A premove is recorded only when
`currentDirection !== lastMove`, i.e. when a turn is already pending for the next
step. A second premove overwrites the first.

Consumption, at the top of each move (`snake.js:374-381`):
```js
if (currentDirection !== MOVE_NONE) {
  lastMove = currentDirection;
  if (preMove !== MOVE_NONE) {
    currentDirection = preMove;
    preMove = MOVE_NONE;
  }
}
newHead.col = oldHead.col + columnShift[lastMove];
newHead.row = oldHead.row + rowShift[lastMove];
```

The move uses `lastMove`, which was just assigned from `currentDirection`.

**A third input inside one step interval loses the first one.** `setDirection`
does two independent things per press — it may queue, and it may adopt — so the
outcome of three presses is not "the last two win". Starting from a settled
heading of RIGHT:

| Presses | `currentDirection` / `preMove` after | Next two steps |
|---|---|---|
| `DOWN, LEFT, UP` | `UP` / `UP` | UP, UP |
| `DOWN, UP, LEFT` | `UP` / `LEFT` | UP, LEFT |

The difference is only whether the last press reverses `lastMove`: a reversal
cannot be adopted, so it stays queued and surfaces a step later, while a legal
turn overwrites both fields at once. Pinned by `test_premove_third_input`
(tests/test_core.c).

**The premove is never validated against reversal, and never needs to be.** It
cannot produce a 180 — this falls out of the invariants rather than from any
explicit check, so reproduce the structure exactly rather than "improving" it:

- If `|d - lastMove| != 2`, then `currentDirection` is set to `d` as well, so
  premove and current agree and the next two steps share a direction.
- If `|d - lastMove| == 2`, `currentDirection` keeps its prior value `c`, and `c`
  is necessarily perpendicular to `lastMove` (it passed this same test when it
  was set). `d` is opposite `lastMove`, therefore also perpendicular to `c`. The
  step after next turns `c -> d`, a legal 90 degrees.

Test both branches explicitly (PLAN.md 4.6, 11.4).

### 4.5 Input while paused

Ignored. `handleArrowKeys` returns early when `isPaused && !config.premoveOnPause`
(`snake.js:319-322`), and the live page sets `premoveOnPause: false`
(`js/init.js:14`). `SNAKE.Board`'s own default is also `false`
(`snake.js:1426-1429`).

Pause itself is space (keycode 32) and is accepted whenever board state is not
`BOARD_NOT_READY` (`snake.js:1297-1300`) — so pausing works both while playing
and while waiting for the first input. Pausing does not change `boardState`, so
unpausing resumes exactly where it left off; a pause taken before the first
direction press returns to the waiting state, not to a moving snake.

### 4.6 Quirk: non-direction keys reach `setDirection`

`handleArrowKeys` maps an unrecognised key to `MOVE_NONE` and still calls
`setDirection(-1)` (`snake.js:335-336`). With `lastMove = 0` (UP),
`|-1 - 0| = 1 != 2`, so `currentDirection` is set to `MOVE_NONE`, which then
suppresses the `lastMove` update and premove consumption on the next step. It can
also clobber a queued premove.

This is an artifact of routing every keypress through one handler. **Do not
port** — see §10.

---

## 5. Food

| Item | Behavior | Citation |
|---|---|---|
| Count on board | exactly one, ever | `snake.js:902`, `snake.js:625-660` |
| Selection | rejection sampling over the whole grid | `snake.js:641-652` |
| Excludes cells near head? | no | `snake.js:641-652` |
| Give-up threshold | 20000 tries -> returns false -> win | `snake.js:648-651` |

`snake.js:634-652`:
```js
let row = 0, col = 0, numTries = 0;
const maxRows = playingBoard.grid.length - 1;
const maxCols = playingBoard.grid[0].length - 1;

while (playingBoard.grid[row][col] !== 0) {
  row = getRandomPosition(1, maxRows);
  col = getRandomPosition(1, maxCols);
  numTries++;
  if (numTries > 20000) { return false; }
}
```

`getRandomPosition(x, y)` is inclusive of both bounds (`snake.js:577-579`), so
the sampled range includes the far wall row/col. Wall cells hold `1`, so the loop
simply rejects them. Starting at `(0,0)` — always a wall — guarantees at least
one iteration.

Uniform over free cells is the correct model: rejection sampling over a
superset, rejecting non-zero cells, is exactly a uniform draw from the free set.

---

## 6. Collision and end states

### 6.1 Move resolution order

`snake.js:343-418`, in order:

1. `oldHead = snakeHead`, `newHead = snakeTail` (the tail block is recycled).
2. `snakeTail = newHead.prev`, `snakeHead = newHead`.
3. **Clear the vacated tail cell:** `grid[newHead.row][newHead.col] = 0`
   (`snake.js:370-372`), guarded so the `(-1,-1)` blocks from §3 are skipped.
4. Consume premove and set `lastMove` (§4.4).
5. Compute the new head cell from `oldHead + shift[lastMove]`.
6. Read the destination cell and branch.

### 6.2 Destination branch

`snake.js:399-417`:

| Cell value | Result |
|---|---|
| `0` | free — occupy it (`= 1`), schedule next step |
| `> 0` | **death** (wall and body are both `1`) |
| `-1` (food) | occupy it, `eatFood()`; if that returns false -> **win** |

The `> 0` test precedes the food test, which is only safe because
`GRID_FOOD_VALUE` is negative — hence the "MUST BE NEGATIVE" comment at
`snake.js:761`.

### 6.3 Walls

**Walls always kill. There is no wrap mode in any difficulty.** The mode table
therefore carries `walls_kill = true` for Easy, Medium and Hard. Keep the field
(PLAN.md 4.5) so a future wrap mode stays a data change.

### 6.4 Self-collision and the vacating tail cell

**Moving into the cell the tail is vacating this step is LEGAL.** Step 3 above
zeroes that cell before step 6 reads it. A snake moving in a tight cycle follows
its own tail without dying.

The one exception is implicit: for 5 steps after eating, the tail block being
recycled sits at `(-1,-1)`, the guard in step 3 skips the clear, and the real
tail does not move. The snake genuinely occupies more cells during that window.

### 6.5 Win

Triggered when `randomlyPlaceFood()` fails (`snake.js:410-412` via
`snake.js:449-451` and `snake.js:1377-1379`). It is a probabilistic give-up after
20000 failed samples, not a free-cell count.

With one free cell out of ~1100, the chance of 20000 consecutive misses is about
`e^-18`. The heuristic is therefore observationally identical to "no free cells
remain", and `src/core/` should implement the deterministic version — PLAN.md 4.6
explicitly requires the board-full case to produce WON rather than an infinite
loop. Recorded as a deliberate deviation in §10.

### 6.6 State machine

The original tracks three board states (`snake.js:44-46`, `snake.js:1077-1091`)
plus independent `isPaused` and `isDead` flags:

| Original | Meaning |
|---|---|
| `BOARD_NOT_READY` (0) | a dialog is up: welcome, died, or won |
| `BOARD_READY` (1) | dialog dismissed; snake placed; awaiting first direction |
| `BOARD_IN_PLAY` (2) | snake moving |

Mapped to the states PLAN.md 4.3 names, **plus a READY state the plan does not
list** (see §11):

| State | Legal transitions |
|---|---|
| `WELCOME` | -> `READY` on confirm |
| `READY` | -> `PLAYING` on first direction input; -> `PAUSED` on pause |
| `PLAYING` | -> `PAUSED` on pause; -> `DEAD` on collision; -> `WON` on board full |
| `PAUSED` | -> back to whichever of `READY` or `PLAYING` it was entered from |
| `DEAD` | -> `READY` on confirm; -> `WELCOME` on back |
| `WON` | -> `READY` on confirm; -> `WELCOME` on back |

Everything else must be unreachable and asserted so in tests (PLAN.md 4.3).

`rebirth()` clears `isDead`, sets `isFirstGameMove = true` and `preMove = NONE`
(`snake.js:492-496`); `reset()` returns the body to length 1 at the start cell
(`snake.js:502-544`). Entering `READY` must do both.

---

## 7. Scoring and persistence

| Item | Value | Citation |
|---|---|---|
| What "Length" counts | block count, i.e. `snakeLength` | `snake.js:207`, `436` |
| Starting value | 1 | `snake.js:207` |
| Sequence | 1, 6, 11, 16, ... (+5 per food) | `snake.js:430-436` |
| Highscore scope | **global, not per-mode** | `snake.js:48` |
| Storage key | `"jsSnakeHighScore"` | `snake.js:48` |
| Value format | the bare integer length | `snake.js:265`, `1373` |
| Seeded on first run | `0` | `snake.js:152-153` |

Updated in two places: `foodEaten` writes it live whenever the current length
exceeds it (`snake.js:1372-1376`), and `recordScore` writes it again at end of
game (`snake.js:257-267`). `recordScore` also raises a browser `alert()` — not
ported.

Since the key is a single global value, a Hard run and an Easy run compete for
the same highscore. Preserve that.

---

## 8. Presentation

§8.1 to §8.5 describe the main theme, which is the original's default and the
only one v1 shipped. §8.6 adds the other two this port carries; everything
outside §8.6 — geometry, typography, strings — is identical across all three.

### 8.1 Colors

All from `css/main-snake.css` at the pinned commit.

| Element | Hex | Citation |
|---|---|---|
| Page background (letterbox on Vita) | `#fc5454` | `main-snake.css:6-8` |
| Playfield | `#0000a8` | `main-snake.css:56-59` |
| Snake block fill (alive) | `#fcfc54` | `images/snakeblock.png` |
| Snake block fill (dead) | `#c0c0c0` | `images/deadblock.png` |
| Snake block gap/border | `#0000a8` | both images |
| Food | `#ff0000` | `main-snake.css:51-54` |
| HUD text | `#ffffff` | `main-snake.css:32-36` |
| Overlay box background | `#000000` | `main-snake.css:61-73` |
| Overlay text | `#ffffff` | `main-snake.css:61-73` |
| Pause box background / text | `#000000` / `#ffffff` | `main-snake.css:25-30` |

**The snake is yellow, not red.** `.snake-snakebody-block` declares
`background-color: #ff0000` (`main-snake.css:38-41`), but `snakeblock.png` is a
20x20 opaque image that covers the block completely, so the red never shows.
Reading only the CSS gives the wrong answer here.

Both block images decode to a 1px `#0000a8` border with an 18x18 interior — 324
interior pixels to 76 border pixels, verified by decoding the PNGs. Because the
border is the playfield color, adjacent segments appear separated by a 1px gap.
Draw it as an 18x18 fill at a 1px inset, not as a stroked rect.

Food has `border: 0px` (`main-snake.css:51-54`) and no image: a solid 20x20
`#ff0000` square, flush, with no gap.

### 8.2 Death coloring

`handleEndCondition` swaps the dead class onto **`me.snakeHead` only**
(`snake.js:269-280`). On death the head turns `#c0c0c0` and the rest of the body
stays `#fcfc54`.

The killing block is drawn **where it died, inside the wall ring**, i.e. one cell
outside the playfield. `go()` moves the head element and writes its `left`/`top`
*before* it reads the destination cell and branches to `handleDeath`
(`snake.js:382-405`), and blocks are positioned against the container rather than
against the playing field (§2.1), so nothing clips it. A wall death therefore
shows a grey block sitting on the page background just outside the field. This is
faithful behavior, not an off-by-one — do not "fix" it.

### 8.3 Head vs body

`#snake-snakehead-alive` is assigned as an element id (`snake.js:214`, `393-394`)
but **`main-snake.css` never styles it** — only `head-snake.css`,
`golden-snake.css`, `blue-snake.css`, `og-snake.css` and `cotton-candy-snake.css`
do. In the main theme the head and body are visually identical while alive.

This answers PLAN.md 3.3's "there is a head color feature in the repo history":
the feature exists, but not in the main theme.

`blue-snake.css` is one of the stylesheets that does distinguish them, and it is
carried here as Original (§8.6), so this port could have modelled a head colour.
It deliberately does not: the field would have existed for exactly one theme,
and Original's body tile is the shared `snakeblock.png`, so a uniformly yellow
snake is what that tile actually is. Its head block
(`green-head-snakeblock.png`, centre `#00620C`) is not drawn. Recorded here
rather than in §10 because it is a property of one theme, not of the port.

### 8.4 Typography

Verdana / Arial / Helvetica / sans-serif, 14px, for HUD, pause box and overlays
(`main-snake.css:25-36`, `61-73`). Overlay boxes are 300px wide, centered, with
8px padding (`common-snake.css:84-111`). The pause box is 300x80
(`common-snake.css:72-82`).

### 8.5 Exact strings

Confirmed in source:

| String | Citation |
|---|---|
| `JavaScript Snake` | `snake.js:932`, `967` |
| `Use the arrow keys on your keyboard to play the game.` | `snake.js:932-933` |
| `Play Game` | `snake.js:936` |
| `You died :(` | `snake.js:998` |
| `Play Again?` | `snake.js:969` |
| `You win! :D` | `snake.js:1005` |
| `[Paused]` / `Press [space] to unpause.` | `snake.js:828-829` |
| `Length: 1` | `snake.js:838`, `1072` |
| `Highscore: N` | `snake.js:842-843`, `1374-1375` |

Both end-game overlays are `"JavaScript Snake"`, then the message, then the
button (`snake.js:966-969`).

Vita rewrites per PLAN.md 6.5/6.6 — wording only, typography and color unchanged:

| Original | Vita |
|---|---|
| `Use the arrow keys on your keyboard to play the game.` | `Use the D-Pad or left stick to play.` |
| `Play Game` | `Press X to start.` |
| `Press [space] to unpause.` | `Press START to unpause.` |
| the difficulty `<select>` (`index.html:155-161`) | `Difficulty: %s - SQUARE to change`, on the welcome screen only (§10 row 12) |
| — | credit line naming Patrick Gillespie and the MIT license |

`Play Again?` is retained on the death and win overlays, with `Press X` as the
prompt.

### 8.6 Themes

The reference builds a dropdown from a fourteen-entry `THEMES` array
(`index.html:90-104`), each entry a label naming its contributor and a
stylesheet. This port carries three of them, cycled with Triangle on any screen.
The labels and authors below are the reference's own.

| Index | Name | Author | Stylesheet |
|---|---|---|---|
| 0 | Main | patorjk | `css/main-snake.css` (the original's default) |
| 1 | Matrix | Geahad Haymor | `css/matrix-snake.css` |
| 2 | Original | DylanLCrocker | `css/blue-snake.css` |

"Original Theme by DylanLCrocker" maps to `blue-snake.css`, which is worth
recording because the filename does not say so.

Values that live in a stylesheet are cited to a line. Values that live in a
block image are the **centre pixel** of that image, which is the convention §8.1
already uses: `snakeblock.png` is a 20x20 tile whose outer ring is the playfield
colour showing through, so its centre is the body colour and its mean is not.

| Field | Main | Matrix | Original |
|---|---|---|---|
| Background (letterbox) | `#FC5454` | `#00FF11` | `#004620` |
| Playfield | `#0000A8` | `#000000` | `#149C36` |
| Snake | `#FCFC54` | `#00C848` | `#FCFC54` |
| Dead head | `#C0C0C0` | `#C0C0C0` | `#C0C0C0` |
| Food | `#FF0000` | `#E80015` | `#CF2121` |
| HUD text | `#FFFFFF` | `#000000` | `#FFFFFF` |
| Dialog background | `#000000` | `#000000` | `#000000` |
| Welcome text | `#FFFFFF` | `#00FF11` | `#FFFFFF` |
| Death / win text | `#FFFFFF` | `#FF0000` | `#FFFFFF` |
| Pause background | `#000000` | `#000000` | `#004620` |
| Pause text | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` |
| Button border | `#FFFFFF` | `#00FF11` | `#FFFFFF` |

- **Matrix** — `matrix-snake.css:1` body, `:54` playing field, `:29` panel,
  `:63` welcome, `:69` death and win, `:25-26` pause screen. Snake is the centre
  of `matrix-snake-block.png`, food the centre of `matrix-food-block.png`. Dead
  blocks fall back to the shared `deadblock.png`.
- **Original** — `blue-snake.css:1` body, `:58` playing field, `:52` food,
  `:23` panel, `:63` welcome, `:69` death and win, `:19-20` pause screen. The
  body tile is the shared `snakeblock.png`, so the snake is the same yellow as
  Main.

Three colours here have no counterpart in §8.1 because the main theme cannot
show them. The reference styles `.snake-pause-screen` separately from the
welcome and death dialogs, and all three disagree in the themes this port adds:
Matrix's pause text is white rather than its dialog green, its death and win
dialogs are red rather than its welcome green, and Original's pause background
is its dark green rather than black. Main is identical in all three places,
which is why v1 could use one value for all of them.

**Dark, listed at `index.html:92`, is deliberately not carried.** Its snake
(`#15241F`, the centre of `dark-snakeblock.png` — a JPEG despite its `.png`
name) against its playfield (`#312E44`) is a WCAG contrast ratio of 1.23:1,
against 12.26:1 for Main and 9.36:1 for Matrix. In the browser the tile's
texture separates snake from board, and §10 row 13 flattens exactly that away.
Its own stylesheet offers no remedy: `dark-snake.css:46` sets the body block's
border to `0px`, and the 3px and 2px borders in that file are on the playing
field and the food. Carrying it would have meant either a theme that is hard to
play or an invented outline that is not the theme its author wrote.

---

## 9. Vita grid (PLAN.md 5.4, 11.3)

Applying §2.1 unchanged with the Vita screen as the container, `cWidth = 960`,
`cHeight = 544`:

```
960 % 20 = 0    wEdgeSpace = 40 + 0 = 40    fWidth  = 960 - 40 = 920
544 % 20 = 4    hEdgeSpace = 60 + 4 = 64    fHeight = 544 - 64 = 480

numBoardCols = 920/20 + 2 = 48     playable cols = 46
numBoardRows = 480/20 + 2 = 26     playable rows = 24
```

| Quantity | Value |
|---|---|
| Playfield rect | x 20..940, y 20..500 (920 x 480) |
| Playable interior | 46 x 24 = **1104 cells**, rows 1..24, cols 1..46 |
| Block size | 20 px (unchanged from the original) |
| HUD strip | y 500..544 (44 px) |
| HUD panel top | y = 507 |
| `Length:` panel left edge | x = 30 |
| `Highscore:` panel left edge | x = 820 (`cWidth - 140`) |
| Letterbox color | `#fc5454` |

Derived from `snake.js:1211-1227` for the HUD placement:
`bottomPanelHeight = 64 - 20 = 44`, `pLabelTop = 20 + 480 + round((44-30)/2) = 507`.

The two figures above are the panel *divs*. Each carries `padding: 8px`
(`common-snake.css:65-70`), so the glyphs themselves start at x = 38 / 828 and
y = 515. `src/shell/render.c` computes all of this from the board dimensions
rather than hardcoding 920x480, which keeps the small boards used by the replay
scripts renderable; for the shipping 48x26 board it reduces to this table
exactly, and `tests/check_layout.sh` asserts that against real rendered pixels.

Dialog geometry, all centred on the 960x544 container
(`common-snake.css:72-111`):

| Dialog | Box |
|---|---|
| Welcome | x 322, y 172, 316 wide, height from content |
| Death / win | x 322, y 197, 316 x 116 |
| Pause | x 330, y 232, 300 x 80 |

The interior tiles the playfield exactly at 20px with no remainder, so no
centering or letterboxing is needed inside the playfield — the 20px margins on
three sides and the 44px HUD strip are the whole difference. Maximum attainable
length is bounded by 1104 cells.

This grid differs from any browser instance of the original (PLAN.md 11.3). Call
it out in README.md.

---

## 10. Deliberate deviations from the reference

Each is a conscious decision, not an oversight. Anything not listed here should
match the original exactly.

| # | Deviation | Rationale |
|---|---|---|
| 1 | Fixed 46x24 grid | Vita screen is fixed at 960x544; the original's grid is window-dependent. PLAN.md 11.3. |
| 2 | Win when free-cell count reaches 0, not after 20000 failed samples | §6.5. Observationally identical, deterministic, and required by PLAN.md 4.6. |
| 3 | Mode speed applies from the first game | §4.1. The 80ms-first-game behavior is a bug; shipping it would make Medium wrong exactly once per launch. |
| 4 | Non-direction buttons do not reach the direction logic | §4.6. A DOM keyboard artifact with no analogue on a gamepad. |
| 5 | Place the snake head before the first food | §11.2 below — the original can spawn food under the head and soft-lock. |
| 6 | No `alert()` on beating the highscore | `snake.js:260-264`. No modal dialogs on Vita. |
| 7 | Vita control strings | §8.5, PLAN.md 6.5/6.6. |
| 8 | Seeded RNG supplied by the shell | PLAN.md 4.2 requires a deterministic pure core; `Math.random()` cannot be replayed. |
| 9 | No touch speed factor | `snake.js:34-42` doubles the interval on coarse-pointer devices. The Vita is not a touch device for these purposes and v1 ships no touch controls (PLAN.md 6.7). |
| 10 | Dialog buttons are outlined labels, not widgets | The original's `Play Game` / `Play Again?` are real `<button>` elements styled by the browser. There is no widget toolkit here, so §8.5's prompt text is drawn inside a 1px outline in the same position. Color and typography are unchanged. |
| 11 | Bundled DejaVu Sans instead of Verdana | Verdana cannot be redistributed. DejaVu Sans descends from Bitstream Vera, whose proportions were drawn close to Verdana's, and it is freely licensed (`assets/font-LICENSE.txt`). |
| 12 | Difficulty is cycled with Square on the welcome screen, and persists | The original's `<select>` (`index.html:155-161`) has no Vita equivalent. §8.5 has the welcome-screen wording. Square cycles Easy → Medium → Hard → Easy, and the choice is saved, and the cycle is refused outside `WELCOME`. Note: an earlier version of this row claimed the original disables its dropdown during play. It does not — `snake.js:172-192` applies a mode change to `snakeSpeed` immediately, so the original *can* change speed underneath a running snake, and `handleDeath` (`snake.js:473-475`) re-reads the dropdown afterwards. Refusing it mid-game is therefore a deliberate departure, not a port: a step interval that changes while the snake is moving is a fairness problem on a handheld with one save slot for the highscore. The original also does not persist the choice across a reload; saving it is a handheld affordance. |
| 13 | Block artwork is flattened to one colour per theme | Every theme draws its snake and food as tiled PNGs with an inner gradient or texture. v1 already flattened `snakeblock.png` to its centre pixel; §8.6 extends the same rule to Matrix and Original, whose tiles are textured rather than flat. This is also what makes Dark unplayable here and therefore uncarried — see §8.6. |
| 14 | Original's rounded food corners are not modelled | `blue-snake.css:55` gives food `border-radius: 6px`. At a 20px block that is visible but purely decorative, and modelling it would mean a non-rectangular blit for one theme. Every other border in the three carried stylesheets is `0px`, so no other border is skipped. |
| 15 | Theme is cycled with Triangle, on any screen, and persists | The reference's theme `<select>` (`index.html:90-104`) has no Vita equivalent. Unlike the difficulty (row 12) this is *not* refused during play, which matches the original: its `<select>` swaps the stylesheet the moment it changes (`index.html:183-190`), at any point in a game. A theme has no effect on gameplay, so there is no fairness objection to changing it mid-run. The choice is saved, which the original does not do across a reload; that is the same handheld affordance as row 12. |

---

## 11. Resolved questions

Both items below are settled design decisions, retained here because the
reasoning is worth keeping.

### 11.1 The plan's state list is missing READY — APPROVED, six states

PLAN.md 4.3 names five states: `WELCOME, PLAYING, PAUSED, DEAD, WON`. The
original has a distinct sixth (§6.6): the dialog is dismissed, the snake and food
are placed and drawn, but nothing moves until the first direction press.

Folding it into `WELCOME` would draw the overlay over a live board; folding it
into `PLAYING` would start the snake moving on its own. Both are visibly wrong,
so §6.6 specifies six states, amending PLAN.md 4.3.

### 11.2 Food can spawn under the snake's head in the original — APPROVED, fixed

`setupPlayingField` places the food (`snake.js:1259`) *before* the caller marks
the head's cell (`snake.js:1074`, `1120`). With p = 1/1104 the food lands on
(2,2), is immediately overwritten with `1`, and no `-1` remains on the grid. The
food div stays visible, can never be eaten, and no further spawn is triggered —
an unwinnable board.

Deviation 5 fixes this by ordering head-then-food: the head's cell is marked
occupied before the first food is placed, so the food is drawn from the free set
with the head already excluded.
