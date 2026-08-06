# Deploying to hardware

Everything the maintainer needs to get a build onto the Vita, iterate on it quickly, and
read a crash dump when it goes wrong. Covers PLAN.md 7.4 and 7.5.

Set this once per shell; every command below uses it:

```sh
export VITA_IP=192.168.1.xxx        # shown on VitaShell's title bar with SELECT
```

The title ID is `SNEK00001` and appears in exactly two places in this repo:
`VITA_TITLEID` in `CMakeLists.txt` and the `ux0:app` paths here. It was checked
against all 1022 VitaDB homebrew entries and is unused (PLAN.md 11.5).

The title ID is not the displayed name and does not follow it. v1.1.2 renamed
the bubble from "Snake" to **JaVitaScript Snake** by changing `VITA_APP_NAME`
alone. Three things stayed put, deliberately: the title ID above, the `ux0:app`
paths derived from it, and `ux0:data/VitaSnake/` in `platform_vita.c`. Changing
the first two would make the firmware install a second application beside the
old one; changing the third would strand every existing highscore and theme.

## 0. One-time setup on the Vita

1. Install **VitaShell** (bubble, gives you a file manager and an FTP server).
2. Install **vitacompanion** via the taiHEN config so it starts on boot. It
   listens on **1337 (FTP)** and **1338 (command server)**, which is what makes
   the one-liner in section 2 possible.

Confirm both are up:

```sh
curl -s --list-only ftp://$VITA_IP:1337/ | head        # FTP alive
echo version | nc $VITA_IP 1338                        # command server alive
```

## 1. First install: the VPK

```sh
cmake -B build-vita -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
cmake --build build-vita
```

That produces `build-vita/snake.vpk`. Copy it over and install it with
VitaShell (press X on the .vpk, confirm):

```sh
curl -T build-vita/snake.vpk ftp://$VITA_IP:1337/ux0:/data/snake.vpk
```

This step is only needed once, and again whenever the LiveArea assets or
`param.sfo` change - the fast path below replaces the executable only.

### If the install fails with 0x8010113D

That is the LiveArea PNGs, not the build. The firmware only accepts **8-bit
indexed** (colour type 3) images in `sce_sys/`; truecolour ones install fine in
Vita3K and then fail on hardware at the end of the copy. The checked-in images
are indexed already; the trap is replacing one with an RGBA export. To check
any image before packaging it:

```sh
python3 -c 'import struct,sys; d=open(sys.argv[1],"rb").read(26); \
  print(struct.unpack(">IIBB", d[16:26]))' sce_sys/icon0.png
# -> (128, 128, 8, 3)   width, height, bit depth, colour type. Type must be 3.
```

## 2. Fast iteration: push eboot.bin and relaunch

After the VPK has been installed once, a rebuild can be pushed directly. The
file that becomes `eboot.bin` on device is `build-vita/snake.self`:

```sh
cmake --build build-vita \
  && curl -T build-vita/snake.self ftp://$VITA_IP:1337/ux0:/app/SNEK00001/eboot.bin \
  && printf 'destroy; launch SNEK00001\n' | nc $VITA_IP 1338
```

`destroy` closes whatever is running (including a previous copy of Snake, which
would otherwise hold the file open), and `launch` starts the new one. Roughly a
five second loop.

## 3. Reading a crash dump

A crash writes a gzipped core file to `ux0:data/` named
`psp2core-<time>-<pid>-<name>.psp2dmp`. Fetch the newest one:

```sh
curl -s --list-only ftp://$VITA_IP:1337/ux0:/data/ | grep psp2dmp
curl -o crash.psp2dmp ftp://$VITA_IP:1337/ux0:/data/psp2core-....psp2dmp
```

Parse it against the **unstripped ELF** - `build-vita/snake`, not `snake.velf`
and not `eboot.bin`. The Vita build carries `-g` for exactly this reason
(`CMakeLists.txt`), so addresses resolve to source lines rather than bare
symbol names. Keep the `build-vita/` tree from the build that is on the device;
symbols from a different build will point at the wrong lines.

```sh
cd ~/vita-parse-core
PATH=$VITASDK/bin:$PATH PYTHONPATH=vendor python3 main.py \
    ~/crash.psp2dmp ~/vita-snake/build-vita/snake
```

Output is the crashed thread, its stop reason, registers, disassembly around PC
and LR with the faulting instruction highlighted, and the stack contents with
every address that lands in our text segment resolved to `function at file:line`.

### How that tool got installed

PLAN.md 7.5 said `vitasdk/vita-parse-core`; the actual repo is
**`xyzz/vita-parse-core`** and it is unmaintained Python 2, which will not run
on this machine's Python 3.14. It was installed like this:

```sh
git clone https://github.com/xyzz/vita-parse-core.git ~/vita-parse-core
cd ~/vita-parse-core
git apply ~/vita-snake/tools/vita-parse-core-py3.patch
```

`tools/vita-parse-core-py3.patch` in this repo is the Python 3 port, against
upstream commit `644b5f0`. It is small - `str2bytes`/`bytes2str` moved out of
`elftools.common.py3compat` (deleted in modern pyelftools) into `util.py`, and
`util.py`'s `xrange`, `string.letters` and `ord()`-on-characters were updated
for Python 3 byte semantics.

Its one dependency, pyelftools, is pure Python and was vendored rather than
pip-installed, because this box has neither `pip` nor `python3-venv` and both
need root to add:

```sh
mkdir -p ~/vita-parse-core/vendor
curl -sL -o /tmp/pyelftools.whl \
  https://files.pythonhosted.org/packages/46/2a/f9697576603dae937727827505a6126a066affb227034e77e6f9068910da/pyelftools-0.33-py3-none-any.whl
unzip -qo /tmp/pyelftools.whl -d ~/vita-parse-core/vendor
```

Note the pinned `pyelftools==0.24` in the tool's `requirements.txt` is **not**
usable - it imports `MutableMapping` from `collections`, which Python 3.10
removed. 0.33 works with the patch above.

### Verifying the tool without a crash

The pipeline was tested end to end before any hardware crash existed, by
synthesising a `.psp2dmp` with the note layouts `core.py` expects and pointing
its fake PC at a real address in `build-vita/snake`. The generator is not
checked in - it was scaffolding, not a deliverable - but the result confirmed
symbolisation, disassembly and stack walking all work:

```
PC: 0x810006d8 (snake.elf@1 + 0x6d8 => game_queue_input at src/core/game.c:97)
```

So the first real dump should need no debugging of the debugger. What is still
unverified is the shape of a *real* dump: the note layouts came from the tool's
own source, not from a dump Sony's firmware produced.

## 4. Vita3K (no hardware needed)

The emulator is the Phase 5 exit criterion and runs on the Windows side, not in
WSL. Install `ux0:` from the Vita3K UI, then drag `build-vita/snake.vpk` onto
the window to install it. The WSL path is reachable from Windows Explorer at:

```
\\wsl$\Ubuntu\home\<user>\JaVitaScript-Snake\build-vita\snake.vpk
```

Vita3K is for checking that the app boots, the LiveArea art is right, and the
welcome screen renders. It is not a substitute for hardware on input timing or
performance (PLAN.md 1.3).
