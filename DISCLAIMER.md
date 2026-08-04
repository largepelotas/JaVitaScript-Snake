# Disclaimer

<!--
  SCAFFOLD — every line marked "TODO" is a prompt for you, not content.
  Write over it in your own voice and delete the marker. Anything not marked
  TODO is a fact taken from this repository (git history, VERSION, docs/,
  CMakeLists.txt) and should be true as written — but check it before you ship,
  since the numbers move as you keep committing.

  Grep for what's left:  grep -n "TODO" DISCLAIMER.md
-->

This file is written in my own voice, by the person who maintains this project.
It exists so that anyone installing this homebrew knows what it is, where it
came from, and how it was built — before they put it on their Vita.

## Who I am

> **TODO —** Who are you, in two or three sentences? Worth covering: what you
> do (or don't do) professionally, how much C you had written before this, and
> whether you had touched embedded or homebrew development before. Readers are
> mostly trying to calibrate how much to trust the binary, so plain and honest
> beats impressive.

> **TODO —** Optional: why the PS Vita, and why Snake? A sentence on what drew
> you to the platform gives the project a reason to exist beyond the code.

## How this project came about

Verifiable history, from this repository:

- First commit **2026-07-31**; 23 commits on `main` through **2026-08-04**.
- Four releases: **v1.0.0**, **v1.1.0**, **v1.1.1**, and the current
  **v1.1.2** (see [`VERSION`](VERSION)).
- One merged pull request — [#1](../../pull/1), the themes work.
- Roughly **7,400 lines of C** across [`src/`](src/), [`tests/`](tests/) and
  [`tools/`](tools/), plus about **1,800 lines of documentation** in
  [`docs/`](docs/) and this repository's README.

> **TODO —** The part the git log can't tell anyone: what were you actually
> trying to do? Was this a learning exercise, a device you wanted a game on, a
> way to try out a toolchain? Say what the goal was and whether it changed.

> **TODO —** What was hard? The `0x8010113D` install failure documented in
> README's build section — where RGBA LiveArea art aborts the install on real
> hardware while Vita3K accepts the same `.vpk` — is the kind of specific,
> hard-won detail that tells a reader you actually shipped this to a device.
> Mention whatever else cost you real time.

## How this was built, and the part AI played

**This project was built with substantial AI assistance.** I am stating that
plainly and up front rather than leaving it to be inferred. The corresponding
NeoVitaDB catalog entry sets its `ai` flag to `true`, so the store surfaces it
too.

Specifically:

> **TODO — confirm and expand.** You told me two things, and I've written them
> as a skeleton below. Correct anything that's off and add the detail only you
> have: which model or tool, roughly what the working loop looked like, and
> where you overrode it.

- **Most of the code was AI-generated**, written to my direction and under my
  review. I decided what the project was, what went in it, and what shipped;
  I did not type most of the lines in [`src/`](src/).
- **The reverse-specification was produced with AI assistance.**
  [`docs/MECHANICS.md`](docs/MECHANICS.md) — 672 lines extracting the original
  game's mechanics from its JavaScript, with every constant cited to a line
  number — was built this way.

> **TODO —** The question a reader most wants answered: **what did you verify
> yourself?** Did you read the generated code? Run it on hardware before
> tagging each release? Check the MECHANICS citations against the original by
> hand? Be specific about what you checked and, just as usefully, what you
> didn't. An honest "I did not independently re-derive every constant" is worth
> more than a vague claim of full review.

What the repository can attest to on its own, independent of who or what wrote
the code:

- [`docs/MECHANICS.md`](docs/MECHANICS.md) pins the reference implementation at
  commit `c2f26f9b3aab4c0e14fb54a0213089815c0673af` (2026-07-25) and cites line
  numbers in that exact commit, so any claim in it can be checked against a
  fixed target rather than a moving branch.
- `make test` runs unit, input, save-record and theme-table tests plus replay
  scripts; `make parity` re-runs those replays through the SDL shell and
  compares hashes; `make shots` renders headless screenshots with pixel
  assertions. The test plan is in [`docs/TESTPLAN.md`](docs/TESTPLAN.md).
- Deviations from the original are enumerated with rationale in
  [`docs/MECHANICS.md`](docs/MECHANICS.md) §10 and summarised in the README,
  including the cases where this port deliberately fixes an upstream bug.

> **TODO —** Optional but worth considering: your view on AI-assisted homebrew
> generally. You're publishing into a scene that has opinions about it. You
> don't owe anyone an argument, but if you have a position, this is the place.

## What this means for you

> **TODO —** Set expectations honestly. Some things worth being clear about:
> whether you'll be maintaining this or it's finished; whether you can support
> installs that go wrong; and that it's homebrew on a hacked console, which
> carries the usual risks that have nothing to do with this project
> specifically.

This software comes with no warranty of any kind. It writes only to
`ux0:data/VitaSnake/` and installs under title ID `SNEK00001`.

## Provenance and licensing

This is a **derivative work**, and the original is not mine:

- *JavaScript Snake* is by **Patrick Gillespie** ([patorjk.com](https://patorjk.com)),
  MIT licensed. Its license is reproduced verbatim at
  [`third_party/JavaScript-Snake-LICENSE`](third_party/JavaScript-Snake-LICENSE)
  and covers the gameplay design this port derives from.
- Three of the four themes are **copied from the original site and credited to
  the people who contributed them there** — Main (patorjk), Matrix (Geahad
  Haymor), Original (DylanLCrocker). Their colours were read out of the
  upstream stylesheets rather than chosen; the citation table is in
  [`docs/MECHANICS.md`](docs/MECHANICS.md) §8.6.
- The Vita theme is the only one original to this port.
- The bundled font is **DejaVu Sans**, unmodified, under the Bitstream Vera
  Fonts Copyright — see `assets/font-LICENSE.txt`.

- My own contribution — the C implementation, the tests and tools, the
  documentation, and the Vita theme — is **MIT licensed**, matching the
  original. See [`LICENSE`](LICENSE).

All three licenses are permissive and mutually compatible. You are free to use,
modify and redistribute this, including the `.vpk` as built, provided the three
notices travel with it.

## Contact

> **TODO —** Where should people report bugs or ask questions? If it's the
> GitHub issue tracker, say so.

When reporting a bug, please quote the first line of
`ux0:data/VitaSnake/log.txt` — it names the exact build (`VitaSnake v1.1.2`).
The LiveArea bubble cannot: `param.sfo` stores only two fields, so every 1.1.x
build displays as `1.01`.
