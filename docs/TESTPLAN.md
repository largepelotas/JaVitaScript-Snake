# Hardware test plan

Phase 6 (PLAN.md 8). Everything that could be verified on the host already has
been — the core has unit tests and blessed replays, the shell has synthesised-
event input tests, and the layout is asserted against pixels in `artifacts/`
(PLAN.md 0.1). What follows is only the part a desktop cannot answer: a real
screen, a real pad, real flash storage, and a process that gets suspended by a
system the host does not have.

It runs in one sitting, about 25 minutes, and it is meant to be executed in
order — a few items depend on the state the previous one leaves behind.

Mark each item **pass** / **fail** and write what you actually saw when it is
not exactly what the item says to expect. "Roughly right" is a fail worth
recording; that is the whole point of doing this on the device.

## Before you start

- Install per [`DEPLOY.md`](DEPLOY.md) §1. If a build is already installed, the
  `.self` push in §2 is enough.
- Have `artifacts/*.png` open on the PC. They are rendered at the Vita's exact
  960x544, so the device screen should match them pixel for pixel, not
  approximately. `welcome.png`, `welcome_hard.png`, `paused.png`, `dead.png`,
  `won.png`, `playing_early.png`, `playing_long_snake.png` and `diagnostic.png`
  are the states below.
- Item 4 needs writing down. Everything else can be reported from memory.

### Starting state

Items 2 and 15 test opposite things, and you only get one first launch, so
decide which you care about before deleting anything:

- **If a previous build's save is on the device**, do items 1, 15, then delete
  `ux0:data/VitaSnake/` in VitaShell and do item 2. That way the upgrade path is
  tested with a genuine old file, which cannot be faked afterwards.
- **If this is a first install**, item 15 does not apply — say so and skip it.

---

## 1. Install and LiveArea (PLAN.md 8.1)

Install the `.vpk` from VitaShell. Expect no error dialog, a bubble on the
LiveArea, and — on opening the bubble — the startup image and background from
`sce_sys/`.

The bubble must read **JaVitaScript Snake**, renamed from "Snake" in v1.1.2.
Check this on a device that already had an earlier build: the title ID is
unchanged, so the install replaces in place and the label updates — there must
be exactly one bubble afterwards, not a second one beside the old. Then do item
2 and confirm the highscore survived, which is the other half of the same
claim: a rename is a label change, and `ux0:data/VitaSnake/` did not move.

Expect: install succeeds; art is not black, not stretched, not the default
homebrew placeholder.

If the install fails with `0x8010113D`, that is the indexed-PNG problem and
DEPLOY.md §1 has the fix; report it rather than working around it.

**Result: pass**

## 2. First launch with no save data (PLAN.md 8.2)

With `ux0:data/VitaSnake/` deleted, launch the game.

Expect: no crash, welcome screen appears, HUD reads `Highscore: 0`, difficulty
line reads `Difficulty: Medium - SQUARE to change`. `log.txt` is created and its
second line is `storage: ux0:data/VitaSnake/ (mkdir rc 0x00000000)` — a non-zero
`rc` here means the directory already existed, which contradicts having deleted
it.

**Result: pass**

## 3. Welcome screen text and layout (PLAN.md 8.3)

Compare against `artifacts/welcome.png`. The strings, in order:

```
JavaScript Snake
Use the D-Pad or left stick to play.
Difficulty: Medium - SQUARE to change
Press X to start.
Based on JavaScript Snake by Patrick Gillespie, MIT licensed.
```

Expect: same wording, same order, same centring, no clipping at the screen edges
and no text running off the dialog. The HUD strip along the top reads
`Length: 1` and `Highscore: N`.

The Vita's screen is the exact size the screenshot was rendered at, so anything
that looks shifted **is** shifted.

**Result: pass**

## 4. Button-index diagnostic (PLAN.md 8.4, 11.1) — write these down

On the welcome screen, hold **L + R**. A panel appears (`artifacts/diagnostic.png`
is the reference) showing `held indices:` and, under it, what this build thinks
those indices are.

Keeping L + R held, press each button in turn and record the index that appears:

| Button | Index this build expects | Index you saw |
|---|---|---|
| Cross | 2 | 2 |
| Circle | 1 | 1 |
| Square | 3 | 3 |
| Triangle | 0 (unbound) | 0 |
| Start | 11 | 11 |
| L | 4 | 4 |
| R | 5 | 5 |
| D-Pad Up | 8 | 8 |
| D-Pad Down | 6 | 6 |
| D-Pad Left | 7 | 7 |
| D-Pad Right | 9 | 9 |

The expected column was read out of the `libSDL2.a` this build links, by
disassembling `VITA_JoystickUpdate` — it is the binary's account of itself, not
an observation of hardware, which is exactly why this item exists (PLAN.md
11.1). A disagreement is a one-line fix to `g_buttons` in
`src/platform/platform_vita.c`, so report the numbers even if the game plays
fine.

**Result: Partial pass, I cannot test cross without the game starting and the diagnostic closing**

> Fixed 2026-08-01: while L+R are held on the welcome screen, buttons now only
> report their index — Cross does not start the game and Square does not cycle
> the difficulty (`src/shell/input.c`, `test_diagnostic_swallows_input`). The
> panel says so on a new bottom line. **Re-run this item on the next build and
> fill in Cross.** Every other index in the table matched, and the log confirms
> them independently: masks `0x31`, `0x32`, `0x38`, `0x70`, `0xB0`, `0x130`,
> `0x230`, `0x830` over the L+R base of `0x30` are exactly triangle 0, circle 1,
> square 3, down 6, left 7, up 8, right 9 and start 11. Only bit 2 is
> unwitnessed.
>
> Re-run 2026-08-01 on the fixed build: **pass**. Cross reports index 2 and does
> not start the game while the panel is up. The whole table is now observed on
> hardware and agrees with the disassembly, so PLAN.md 11.1 is closed and
> `g_buttons` in `src/platform/platform_vita.c` needs no change.

## 5. D-Pad in all four directions (PLAN.md 8.5)

Start a game with Cross. Steer with the D-Pad only.

Expect: all four directions respond, and the snake keeps moving in the last
direction pressed without holding anything.

**Result: pass**

## 6. Left stick and deadzone (PLAN.md 8.6)

Same, with the left stick only.

Expect: all four directions respond. Resting the stick does not steer, and a
slight lean does not either — the deadzone is 8000 of 32767, about a quarter of
the travel. A stick that turns the snake when you let go is a fail; so is one
that needs to be slammed to the edge.

**Result: pass**

## 7. Reversal does not kill you (PLAN.md 8.7)

While moving right with a snake at least 3 long, press Left as fast as you can.

Expect: nothing happens — the snake keeps going right. It must never turn into
its own neck. Try it in all four axes, and try it fast enough to land two
presses inside one step.

**Result: pass**

## 8. Premove queue, depth 1 (PLAN.md 8.8, MECHANICS.md 4.4)

Moving right, press **Up then Right** (or Up then Left) as fast as you can —
both inside a single step, which at Medium is 75ms.

Expect: the snake goes up on the next step and turns on the step after. Both
presses are honoured, on consecutive steps, not collapsed into one.

Then press three directions quickly. Expect **the first press to be lost**: the
queue is depth 1, so the third press overwrites the second. Whether you get one
turn or two out of the remaining pair depends on the last press:

- If the last press is a legal turn from your current heading, it wins outright
  and you get a single turn. Moving right, `Down, Left, Up` gives you Up only.
- If the last press reverses the direction you are currently *moving*, it cannot
  be taken immediately, so it waits: `Down, Up, Left` while moving right gives
  Up then Left on consecutive steps.

Both shapes are pinned by `test_premove_third_input` in `tests/test_core.c`, and
both are the original's behavior rather than a choice made here (MECHANICS.md
4.4). What would be a bug is three presses producing three turns.

This one is subtle and is the difference between "a snake game" and *that* snake
game (PLAN.md 11.4), so spend a minute on it.

**Result: Partial pass, three directions quickly does not seem to be "only the last two matter"**

> Resolved 2026-08-01, no code change: the checklist was wrong, not the game.
> "Only the last two" holds for one of the two sequences above and not the
> other, and which one you get depends on the last press. The item now says so,
> and `test_premove_third_input` pins both on the host. Nothing to re-test.

## 9. Pause (PLAN.md 8.9)

Press START mid-game. Compare against `artifacts/paused.png`.

Expect: the board freezes, the overlay reads `[Paused]` then
`Press START to unpause.`, and START resumes. Direction presses made while
paused are ignored — the snake resumes in the direction it was already going.

**Result: pass**

## 10. The three speeds are visibly different (PLAN.md 8.10)

Play a few seconds of each of Easy, Medium and Hard (item 13 is how you switch).

Expect: 100ms, 75ms and 50ms per step respectively — Hard is twice Easy's pace,
and the difference between Easy and Medium should be obvious side by side. There
is no speed ramp within a game in any of the three modes; a game that speeds up
as you eat is a fail.

**Result: pass**

## 11. Both deaths (PLAN.md 8.11)

Die once by driving into a wall, and once by driving into your own body.
Compare against `artifacts/dead.png`.

Expect: both produce the death overlay — `JavaScript Snake`, `You died :(`,
`Play Again?`, `Press X` — and Cross starts a new game. Circle returns to the
welcome screen.

**Result: pass**

## 12. Highscore updates and survives a relaunch (PLAN.md 8.12)

Beat the current highscore, then die. Expect the HUD's `Highscore:` to have
risen to the length you reached.

Now exit to the LiveArea with the PS button, close the game, and relaunch.

Expect: the highscore is still there. `log.txt` shows a line like
`mode=Medium (saved) seed=... highscore=N` with the same N.

**Result: pass**

## 13. Square cycles the difficulty (MECHANICS.md §10 row 12)

On the welcome screen, press Square repeatedly.

Expect: the difficulty line cycles `Easy` → `Medium` → `Hard` → `Easy`, wrapping
forever, and the wording stays `Difficulty: X - SQUARE to change`.
`artifacts/welcome_hard.png` is what the Hard case should look like.

Now start a game and press Square during play, while paused, and on the death
screen.

Expect: nothing happens in any of them. The difficulty can only change on the
welcome screen — the original disables its dropdown once play starts, and the
step interval must not change underneath a running snake.

**Result: pass**

## 14. The difficulty persists across launches

Cycle to **Hard**, play one game, die, exit to the LiveArea, close the game and
relaunch.

Expect: the welcome screen comes up on Hard, not Medium. `log.txt` shows
`difficulty: Hard` when you cycled to it, and `mode=Hard (saved) ...` on the
next launch.

**Result: pass**

## 15. A save from the previous build survives the upgrade

Only meaningful if the device already had a build from before Square existed —
see "Starting state" above.

Expect: after installing this build over it, the old highscore is still shown,
and the difficulty starts at Medium. `log.txt` must **not** contain
`score: unknown version`, `score: bad magic` or `score: checksum mismatch`; any
of those means the old record was thrown away, which is a bug in the version 1
compatibility path, not a normal upgrade.

**Result: pass**

## 16. Extended play (PLAN.md 8.13)

Play continuously for at least five minutes, dying and restarting freely, and
get the snake long enough to fill a good part of the board — `playing_long_snake.png`
is the sort of length worth reaching.

Expect: no slowdown as the snake grows, no stutter, no crash, and no drift in
how the game feels between the first minute and the fifth.

If it crashes, DEPLOY.md §3 covers pulling and reading the `psp2dmp`; send it.

**Result: pass**

## 17. The log is sane (PLAN.md 8.14)

Pull `ux0:data/VitaSnake/log.txt`:

```sh
curl -o log.txt ftp://$VITA_IP:1337/ux0:/data/VitaSnake/log.txt
```

Expect, per launch: `--- vita-snake start ---`, the `storage:` line, an
`input: opened pad ...` line naming the pad and its button and axis counts, a
`mode=... seed=... highscore=...` line, and `--- vita-snake exit ---` on a clean
exit. Nothing about missing assets, and no repeated error every frame.

The log is flushed after every line, so even a lock-up leaves one worth reading.

**Result: pass**

---

## 18. Triangle cycles the theme, on any screen (MECHANICS.md §10 row 15)

New in the themes build. Press **Triangle** on the welcome screen: the line
`Theme: Main - TRIANGLE to change` should advance Main → Matrix → Original →
Vita → Main, the whole screen should recolour with it, and the credit line under
the MIT one should change to name that theme's author.

Then start a game and press Triangle **while the snake is moving**. Unlike
Square, this is meant to work mid-game — the original swaps its stylesheet live
— and the snake must not stutter, change speed or die when it happens.

Check each theme at least once in all four places, because three colours appear
in only one of them each:

| Where | What to look at |
|---|---|
| Welcome | Theme name and author line |
| Playing | Snake against the playfield, and the food |
| **Paused** | Matrix's pause text is white, not green. Original's pause box is dark green, not black. |
| **Death or win** | Matrix's dialog text is **red**, where its welcome text is green. |

The two bold rows are the ones worth being fussy about: those fields exist
because Main could not show them, so a screenshot of Main proves nothing about
either.

Legibility is the real question here, and the device screen is the only place it
can be answered. Matrix measures 9.36:1 for snake against playfield and Main
12.26:1, but **Original is 3.29:1, and its food is 1.50:1 against its
playfield** — the lowest contrast anything in this build has. If the red food on
green is hard to spot in normal play, say so; that is a judgement the numbers
cannot make.

**Result:**

## 19. The theme persists across launches

Cycle to **Matrix**, exit to the LiveArea, close the game and relaunch.

Expect: the welcome screen comes up in Matrix. `log.txt` shows
`theme: Matrix` when you cycled to it, and `theme=Matrix (saved)` on the next
launch.

Then check it survives alongside everything else: set Hard **and** Original,
beat the highscore, relaunch, and confirm all three came back. They share one
16-byte record, so a bug in one loses the others.

`log.txt` must not contain `score: theme ... is not in this build` — that would
mean the record holds an index this build does not have.

**Result:**

## What to send back

1. This file with the results filled in.
2. The button-index table from item 4, whether or not it matched.
3. `log.txt`.
4. Any `psp2core-*.psp2dmp` from `ux0:data/` (DEPLOY.md §3), plus what you were
   doing when it appeared.
5. A photo of anything that looked wrong on screen — the screenshots in
   `artifacts/` are the reference, so a photo next to one of those settles it
   faster than a description.

## If something fails

| Symptom | Where to look |
|---|---|
| Install rejected, `0x8010113D` | DEPLOY.md §1 — indexed PNGs in `sce_sys` |
| A button does the wrong thing | Item 4's table, then `g_buttons` in `src/platform/platform_vita.c` — one line |
| Crash | DEPLOY.md §3, crash-dump analysis |
| Highscore or difficulty lost | The `score:` lines in `log.txt` say which check rejected the record |
| Text clipped or misplaced | Compare against `artifacts/`; the screenshot is the same resolution as the screen |
| Feels wrong but nothing is visibly broken | Say so anyway, in whatever words fit — items 8 and 10 are the usual causes |
