# amigamand

A Mandelbrot and Julia set explorer for the Amiga, for AmigaOS 3.1 (68020 and up). It draws in 256 colours on an AGA machine
(an Amiga 1200 or 4000) and in fewer on other chip sets. You zoom with the mouse, script it with ARexx, and save the
picture as an IFF ILBM.

Two programs are built from one source:

| | needs | notes |
|---|---|---|
| `amigamand` | any 68020 or better | fixed point (Q8.24), the pixel loop is in assembly. The one to use. |
| `amigamand-fpu` | 68881/68882, 68040 or 68060 | `double` maths. Deeper zoom, but not faster than the fixed point one in the tests. It stops at once with return code 26 if there is no FPU. |

## Using it

Start it from Workbench (double click; the tool types set the options) or from a Shell:

    amigamand [MODE n] [CX x] [CY y] [SPAN s] [ITER n] [PALETTE name] [JULIA] [JR x] [JI y]
              [SAVE file] [QUIT] [EXACT] [DEPTH n]

| option | meaning |
|---|---|
| `MODE` | 0 lores, 1 hires (the default), 2 hires interlace. The sizes are 320x256, 640x256, 640x512 (PAL) or 200 / 400 lines (NTSC). |
| `CX CY SPAN` | the centre of the view and its width, for example `CX -0.7436 CY 0.1318 SPAN 0.02` |
| `ITER` | up to how many iterations, 1 to 255 (default 128) |
| `PALETTE` | `Rainbow`, `Fire`, `Ocean`, `Gray`, `Electric`, or 0 to 4 |
| `JULIA`, `JR`, `JI` | the Julia set for the constant JR + JI i (default -0.8 + 0.156i) |
| `SAVE`, `QUIT` | save an ILBM when the first picture is finished, and quit |
| `EXACT` | no fast fill (see below) |
| `DEPTH` | bitplanes, 1 to 8. Fewer planes give fewer colours and less video DMA, which makes a stock A1200 faster. |

Mouse: a left click centres the view there; a left drag draws a box and zooms into it; the right button opens the menu.
Keys: `+` and `-` zoom, the cursor keys pan, `0` resets, `j` shows the Julia set of the point you clicked last, `m` goes back to
the Mandelbrot set, `r` redraws, `s` saves, `c` cycles the colours, space stops a render, Esc quits.

The menu has Project (redraw, abort, save, quit), View (zoom, reset, Mandelbrot, Julia), Mode (the three modes, or "Other..." for
any mode the machine has), Palette (five palettes, cycle colours) and Detail (64, 128, 255 iterations, fast fill). The
Right-Amiga shortcuts work.

A picture is drawn coarse first, then refined. When the view is mirrored about the real axis (the Mandelbrot set centred on 0)
or turned half way round (a Julia set centred on 0) only half of it is computed.

### ARexx

The port is `AMIGAMAND`:

    address AMIGAMAND
    'ZOOM -0.7436 0.1318 0.02'
    'ITER 255'
    'PALETTE fire'
    'WAIT'                      /* returns when the picture is finished */
    'SAVE RAM:spiral.iff'
    'QUIT'

Commands: `RENDER ABORT WAIT RESET`, `ZOOM cx cy span`, `CENTER x y`, `SPAN s`, `MODE n`, `PALETTE name|n`, `CYCLE ON|OFF`,
`ITER n`, `JULIA [re im]`, `MANDELBROT`, `FAST ON|OFF`, `SAVE file`, `QUERY` (returns the view as a result string), `QUIT`.
A bad argument or an unknown command returns 10.

## Building

You need vbcc, vasm, vlink and the NDK headers. They are not in the repository (`toolchain/` is ignored); `CLAUDE.md` says how
they were built on a Mac. Then:

    make                  # amigamand and amigamand-fpu
    python3 tools/make_icon.py     # the .info files, if you change the tool types

`./run` builds and starts the program at once in FS-UAE (an A1200 with your own Kickstart 3.1 ROM and Workbench 3.1 disk in
`~/Documents/FS-UAE`); start options go after it: `./run MODE 0 ITER 255`, and `./run fpu JULIA` runs the FPU build. The emulated machine is a 68040, the fastest
setup that makes a difference (`CPU=68020 FPU= ./run` is a stock A1200, 30 times slower). Esc quits the program. FS-UAE gives the mouse back with Cmd+G (or F12+G).

`tools/run.sh` boots the whole Workbench instead and starts the build (an A1200 with your own Kickstart 3.1 ROM and Workbench 3.1 disk in
`~/Documents/FS-UAE`). `CPU=68030 FPU=68882 tools/run.sh` emulates a faster machine.

## Tests

    tools/test/test.sh

boots the programs in FS-UAE and checks the pictures pixel for pixel, the keys, the menu shortcuts, colour cycling, the
requesters, the real ARexx port, the icon and the tool types. See `tools/test/README.md`.

## How fast

Ticks of 1/50 s for one picture on an emulated stock A1200 (68020, chip RAM only):

| picture | ticks | seconds |
|---|---|---|
| the whole set, hires | 680 | 14 |
| the spiral at span 0.02, lores, 255 iterations | 3953 | 79 |
| a Julia set, hires | 1160 | 23 |

With any fast RAM (an accelerator) it is several times faster, because on a stock A1200 the CPU shares chip RAM with the
screen: fewer bitplanes (`DEPTH`) or lores help there. `CLAUDE.md` has what was tried and measured.

## Limits

- Fixed point has 24 bits of fraction: below a span of about 1e-4 the picture turns blocky. Use `amigamand-fpu`.
- "Fast fill" (Mariani-Silver) and the periodicity check treat a region as inside the set when its border is, or when an
  orbit comes back close to itself. They are right in every test here, but they are heuristics: `EXACT` switches the fill off.
- The mouse and the real Workbench icon were checked only through test builds that call the same handlers and read the same
  icon file; nobody has used them on real hardware.
