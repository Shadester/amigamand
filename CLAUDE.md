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

bebbo `m68k-amigaos-gcc` (amiga-gcc), target `-m68020 -noixemul`. Needs NDK 3.1 headers.

## Commands

- `make` builds both variants; `make clean` removes them.
- `amigamand`: fixed-point Q8.24, no math library. Runs on any Amiga.
- `amigamand-ffp`: `-DUSE_FFP`, uses `mathffp.library` (Motorola FFP, not IEEE). FFP arg order is reversed (`SPSub(a,b)` = b-a, `SPDiv(a,b)` = b/a); use the `FSUB`/`FDIV` macros.
- Run: copy binary to emulator HD, launch from Shell.

## Layout

- `main.c`: everything so far. Single `mandel()` per variant, selected by `USE_FFP`.
- `Makefile`: two targets.

## Status

Scaffold only: renders 320x256x8, any key/click quits. Not yet compiled or run (no toolchain on dev machine). ARexx and ILBM saving not started.

## Conventions

- C (C89-friendly for the toolchain), AmigaOS types (`LONG`, `UWORD`, `STRPTR`) at API boundaries.
- Keep render loop free of OS calls except periodic input/abort polling.
