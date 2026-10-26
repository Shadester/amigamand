# amigamand

Mandelbrot and Julia sets for AmigaOS 3.1 (Kickstart 40). Target: classic 68k Amiga; AGA (256 colours) first, fewer colours on OCS/ECS.

## Constraints

- AmigaOS 3.1 APIs only (no 3.2+/OS4 calls). Open libraries with explicit version checks, close on exit.
- Two builds from one source: `amigamand` (fixed point, no FPU needed) and `amigamand-fpu` (68881/68882/68040+).
- Cross-compile on macOS; test in FS-UAE (A1200 config).
- Check every allocation and library open; clean up in reverse order on failure.

## Toolchain

vbcc (`vc +aos68k -cpu=68020`) with vasm and vlink. Not bebbo gcc.
Installed in `toolchain/vbcc` inside the repo (gitignored; nothing outside the repo; override with `VBCC=`); the Makefile sets `VBCC` and `PATH` and calls `$(VBCC)/bin/vc`, so plain `make` works from any shell.
Built from source on macOS arm64: vbcc, vasm (`vasmm68k_mot`), vlink (unofficial GitHub mirror Leffmann/vlink, official URL was dead). Unix config from `vbcc_unix_config.tar.gz` (the target package's own config uses Amiga assigns, do not use it).
Headers: NDK 3.2 `Include_H` (no 3.1/3.9 on Aminet). Do not call 3.2-only APIs. Names that clash with NDK macros: `CMD_*` (exec/io.h), `CUSTOM`.
vbcc calls slow 64-bit helpers for `long long` multiplies: use `muls.l` (inline asm or `iter020.s`).

## Layout

- `main.c`: UI (screen, window, menus, mouse, keys), rendering control (row passes, Mariani-Silver tiles), ASL requesters, ARexx commands, startup options.
- `view.c/h`: the view (centre, span, Julia constant), number type `num` (Q8.24 `LONG`, or `double` with `USE_FPU`), `view_run()` (pens of a run of pixels), number parse/format. FPU build: the loop is C (`iter()`).
- `iter020.s`: fixed-point pixel loop in assembly, one call per row. Same results as the old C loop, pixel for pixel.
- `c2p.c/h`: chunky-to-planar straight into the screen bitmap (8 pixels per step). `WriteChunkyPixels` costs hundreds of cycles per pixel on AGA.
- `palette.c/h`: five palettes (Rainbow, Fire, Ocean, Gray, Electric). Pen 0 is black (inside the set).
- `ilbm.c/h`: IFF ILBM writer (BMHD, CAMG with the mode ID, CMAP, ByteRun1 BODY).
- `rexx.c/h`: ARexx port `AMIGAMAND` (hand-rolled, no rexxsyslib needed unless a command returns a result string).
- `tools/run.sh`: boots Workbench 3.1 in FS-UAE with the binaries (and icons) on volume `AMIGAMAND:`.
- `tools/make_icon.py`: writes `amigamand.info` and `amigamand-fpu.info` (committed; rerun if the tool types change).
- `tools/test/`: the test suite (`test.sh`, `suite.py`, `amiga.py`, `icontest.c`, `expected.json`); see its README.

## Commands

- `make` builds both variants into `amigamand` and `amigamand-fpu`; `make clean`. Test-only builds (not in `all`): `amigamand-tm1`/`-tm2` (end the first render with a scripted mouse drag/click through the real handlers), `amigamand-tw` (reads the tool types of SYS:prog.info as if started from Workbench), `build/icontest`.
- `tools/test/test.sh [--update] [test...]`: about ten minutes, 14 tests, all passing (see tools/test/README.md). `--update` rewrites `expected.json` (the pens of five views); do that only for a change meant to alter pixels. The test boot has `LIBS:` from the Workbench disk added, `ENV:` in RAM, and `FailAt`/`If`/`Echo` are Shell built-ins (not in C:).
- `tools/run.sh` (A1200, the user's own Kickstart and `workbench-311.hdf` from `~/Documents/FS-UAE`, HDF read-only). `CPU=68030 FPU=68882 tools/run.sh` for an accelerated machine (the FPU build needs one; without an FPU it exits with return code 26). `--joystick_port_1=none` is set, otherwise FS-UAE turns the cursor keys into a joystick. Cmd+G (or F12+G) releases the mouse.
- Start options (CLI `ReadArgs`, or Workbench tooltypes of the same names): `MODE/N` (0 lores, 1 hires, 2 hires interlace), `CX CY SPAN` (view), `ITER/N` (1..255), `PALETTE` (name or 0..4), `JULIA`, `JR JI` (constant), `SAVE` (file, after the first render), `QUIT`, `EXACT` (no fast fill), `DEPTH/N` (bitplanes).
- Colours: pens 0..3 belong to Intuition (black, white, grey, blue) so requesters are readable; the fractal gradient starts at pen 4 (from pen 1 with fewer than 16 colours) and only the gradient cycles.
- Keys: Esc quit, `+`/`-` zoom, `0` reset, cursor keys pan, `j` Julia at last click, `m` Mandelbrot, `r` redraw, `s` save, `c` cycle colours, space abort. Mouse: left click centres, left drag zooms into the box, right button the menu (Right-Amiga shortcuts work).
- Menu: Project (Redraw, Abort, Save ILBM, Quit), View (zoom, reset, Mandelbrot, Julia), Mode (lores, hires, interlace, Other... = ASL screen mode requester), Palette (five, Cycle colours), Detail (64/128/255 iterations, Fast fill).
- ARexx (port `AMIGAMAND`): `RENDER ABORT WAIT RESET ZOOM cx cy span CENTER x y SPAN s MODE n PALETTE name|n CYCLE ON|OFF ITER n JULIA [re im] MANDELBROT FAST ON|OFF SAVE file QUERY QUIT`. `WAIT` replies when the render is done. Unknown command or bad argument returns 10.
- Exit codes: 20 start-up, 21..27 library/window/menu/memory failures, 26 no FPU, `100 + SA_ErrorCode` if the screen cannot be opened (7 = too deep: the chip set has no AGA; it falls back to fewer planes by itself).
- AGA screens need `SetChipRev(SETCHIPREV_BEST)` before `OpenScreenTags`.

## Rendering and speed (what was measured, FS-UAE)

Ticks are Amiga `DateStamp` ticks (1/50 s) for one render. Stock A1200 = 68020, chip RAM only. Accelerated CPUs run near host speed in FS-UAE, so read those numbers as ratios only; the emulated FPU is slow on the host, so do not read the FPU numbers as real hardware.

| render | first version | now (020 / 030 / 030+FPU) |
|---|---|---|
| default hires Mandelbrot, 128 it | 1309 / 33 / 40 | 680 / 10 / 21 |
| spiral zoom 320x256, 255 it | 6964 / 140 / 221 | 4074 / 70 / 179 |
| Julia, hires, 128 it | 3496 / 71 / 102 | 1160 / 19 / 45 |

What helped (all pixel-exact against the earlier output): own chunky-to-planar (drawing was 30% of a frame on a 020, most of it on a 030); fixed-point loop with Q4.28 operands and the high long of `muls.l` (no shift fix-ups); the whole pixel loop in assembly with one call per row (a call per pixel cost more than the average pixel's iterations in an overview); Julia half-turn symmetry (z and -z, 2x); real-axis mirror and cardioid/bulb skip for Mandelbrot.
What helped only sometimes: Mariani-Silver fast fill (iterations 7-24% fewer; 6x on a view full of one colour, a loss in an overview) so it is used only when zoomed in (span < 0.5) and for Mandelbrot; the tolerance-based periodicity check (5x on a period-3 view, 20% slower on a deep spiral) so it switches itself off when what it saves is under a quarter of what it spends. An exact periodicity check never fires in fixed point (truncation noise), and in vbcc's compiled loop even the check itself cost 20-35%.
Hardware limit: on a stock A1200 the CPU shares chip RAM with the video DMA. Hires 8 bitplanes costs about 40% more than 2 bitplanes for the same work (`DEPTH 2` vs 8). Any fast RAM removes that.
Rejected: a 2-multiply loop (zr*zi and (zr+zi)(zr-zi)) with a box bailout |re|>2 or |im|>2. Brute force over 9.7M points: the box lags the circle by 1 iteration for 29% of pixels and by 2..4 for 0.4%, so the dwell cannot be recovered without a history of z, which costs what the multiply saves; and an inexact dwell changes pixels.
Mariani-Silver border pixels are computed in runs along the sides (`view_line`, rows and columns), not a call per pixel: period-3 view 357 -> 283 ticks, spiral 4078 -> 3953 on a 020.

## Status

All 14 tests of `tools/test/test.sh` pass. They cover: pens of five views (stored hashes), fast fill and periodicity against plain iteration, interlace and 5-plane modes, NTSC presets (FS-UAE `--ntsc_mode=1`: 200/400 lines), the FPU build (and its no-FPU exit without a guru), keys (zoom, pan, reset, Julia), Right-Amiga menu shortcuts, colour cycling, abort, the ASL file requester and saving (the saved picture equals the batch render), the ASL screen mode requester opening and readable, mouse drag-zoom and click-centre through the real handlers, the icon loaded by the real icon.library, and the Workbench tool-type path.
The ARexx command script (all commands, error codes, WAIT) was run by hand against a hand-built RexxMsg client, not by a test; real ARexx (`rx`, RexxMast on the Workbench disk) has not been tried.
NOT tested, and cannot be from here: real mouse events (FS-UAE takes the mouse only when focused), the buttons of the ASL screen mode requester (mouse only; Return and Esc do nothing in it), the icon drawn on a real Workbench, any real Amiga.
Known limit: fixed point has 24 bits of fraction, so below a span of about 1e-4 the picture turns blocky (use `amigamand-fpu`).

## Conventions

- C (C89-friendly for the toolchain), AmigaOS types (`LONG`, `UWORD`, `STRPTR`) at API boundaries.
- Any change to the numeric loops must give the same pens as before: compare the saved ILBM of a few views pixel for pixel (an ILBM-to-PNG/diff script lived in the session scratchpad, not in the repo).
