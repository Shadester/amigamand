# amigamand

Mandelbrot generator for AmigaOS 3.1 (Kickstart 40). Target: classic 68k Amiga, OCS/ECS/AGA.

## Goals

- Render Mandelbrot set to an Intuition screen
- AGA support: up to 256 colors (8 bitplanes), palette cycling/gradients; fall back to OCS/ECS depth when no AGA
- ARexx port: scriptable control (set region/iterations/palette, render, save, quit)
- Image saving: IFF ILBM (planned); other formats only if asked

## Constraints

- AmigaOS 3.1 APIs only (no 3.2+/OS4 calls). Open libraries with explicit version checks, close on exit.
- No FPU assumed. Use fixed-point integer math by default; FPU path optional via separate build.
- Cross-compile on macOS; test in an emulator (FS-UAE / WinUAE) at A1200 config.
- Check every allocation and library open; clean up in reverse order on failure.

## Toolchain

vbcc (`vc +aos68k -cpu=68020`) with vasm and vlink. Not bebbo gcc.
Installed in `toolchain/vbcc` inside the repo (gitignored; nothing outside the repo; override with `VBCC=`); the Makefile sets `VBCC` and `PATH` and calls `$(VBCC)/bin/vc`, so plain `make` works from any shell.
Built from source on macOS arm64: vbcc, vasm (`vasmm68k_mot`), vlink (unofficial GitHub mirror Leffmann/vlink, official URL was dead). Unix config from `vbcc_unix_config.tar.gz` (the target package's own config uses Amiga assigns, do not use it).
Headers: NDK 3.2 `Include_H` (no 3.1/3.9 on Aminet). Do not call 3.2-only APIs.
`MUL()` in the fixed-point build is a vbcc inline-asm `muls.l` (`fxmul`); plain `long long` made vbcc call slow 64-bit helpers.

## Commands

- `make` builds both variants; `make clean` removes them.
- `amigamand`: fixed-point Q8.24, no math library. Runs on any Amiga.
- `amigamand-ffp`: `-DUSE_FFP`, uses `mathffp.library` (Motorola FFP, not IEEE). FFP arg order is reversed (`SPSub(a,b)` = b-a, `SPDiv(a,b)` = b/a); use the `FSUB`/`FDIV` macros (they give a-b and a/b). `SPCmp(a,b)` is +1 if a > b.
- Run: `tools/run.sh` builds, then boots Workbench 3.1 in FS-UAE (A1200, AGA). Uses the user's own ROM and `workbench-311.hdf` from `~/Documents/FS-UAE` (HDF mounted read-only). Binaries appear as volume `AMIGAMAND:`; start them from a Shell (`AMIGAMAND:amigamand`). No icons yet. FS-UAE releases the mouse with Cmd+G (or F12+G); `--automatic_input_grab=0` stops auto-grab.
- AGA screens need `SetChipRev(SETCHIPREV_BEST)` before `OpenScreenTags`, else depth 8 fails with `OSERR_TOODEEP` (7). `main.c` exits with `100 + SA_ErrorCode` on screen failure, 21 to 24 for the other failures; AmigaDOS prints it as `failed returncode N`.

## Layout

- `main.c`: everything so far. Single `mandel()` per variant, selected by `USE_FFP`.
- `Makefile`: two targets.

## Status

Renders 8 planes (256 colors, AGA), default PAL HIRES 640x256. Menu (right mouse button): Project (Redraw, Quit), Mode (lores 320x256, hires 640x256, hires interlace 640x512; each reopens the screen and redraws). Esc quits. View is always x -2.5..1, y -1.4..1.4 (same aspect in every mode). Fixed-point and FFP builds both verified in FS-UAE (minimal boot) to draw the same image; FFP is far slower. NOT yet verified: the menu (never clicked), the interlace mode, a run under full Workbench. Palette is a crude placeholder. ARexx and ILBM saving not started.

## Conventions

- C (C89-friendly for the toolchain), AmigaOS types (`LONG`, `UWORD`, `STRPTR`) at API boundaries.
- Keep render loop free of OS calls except periodic input/abort polling.
