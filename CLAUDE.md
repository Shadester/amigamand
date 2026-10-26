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
- `tools/run.sh`: boots Workbench 3.1 in FS-UAE with the binaries on volume `AMIGAMAND:`.

## Commands

- `make` builds both variants into `amigamand` and `amigamand-fpu`; `make clean`.
- `tools/run.sh` (A1200, the user's own Kickstart and `workbench-311.hdf` from `~/Documents/FS-UAE`, HDF read-only). `CPU=68030 FPU=68882 tools/run.sh` for an accelerated machine (the FPU build needs one; without an FPU it exits with return code 26). `--joystick_port_1=none` is set, otherwise FS-UAE turns the cursor keys into a joystick. Cmd+G (or F12+G) releases the mouse.
- Start options (CLI `ReadArgs`, or Workbench tooltypes of the same names): `MODE/N` (0 lores, 1 hires, 2 hires interlace), `CX CY SPAN` (view), `ITER/N` (1..255), `PALETTE` (name or 0..4), `JULIA`, `JR JI` (constant), `SAVE` (file, after the first render), `QUIT`, `EXACT` (no fast fill), `DEPTH/N` (bitplanes).
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
Not done: a 2-multiply loop with a box bailout (about 15%, changes nothing visible but needs exact recovery of the dwell), batching the Mariani-Silver border pixels into runs.

## Status

Verified in FS-UAE (minimal boot, scripted): both builds, all start options, ILBM save (decoded and viewed), the ARexx script (all commands and error codes), keys (zoom, pan, Julia, Mandelbrot, reset), Right-Amiga menu shortcuts, progressive passes, Julia, palettes, FPU build with a 68030+68882 and its no-FPU exit.
NOT verified: mouse click and drag-zoom (FS-UAE takes mouse input only when focused; cannot be injected without taking the user's focus), the interlace mode, the ASL requesters (asl.library is not in the minimal boot), palette cycling, saving from the menu, NTSC presets, a run under full Workbench, the `.info` icon (not written yet).

## Conventions

- C (C89-friendly for the toolchain), AmigaOS types (`LONG`, `UWORD`, `STRPTR`) at API boundaries.
- Any change to the numeric loops must give the same pens as before: compare the saved ILBM of a few views pixel for pixel (an ILBM-to-PNG/diff script lived in the session scratchpad, not in the repo).
