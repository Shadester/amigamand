amigamand 1.0 - Mandelbrot and Julia sets for the Amiga
=======================================================

amigamand draws the Mandelbrot set and Julia sets. You zoom with the mouse,
control it from ARexx, and save the pictures as IFF ILBM.

Needs:  AmigaOS 3.1, a 68020 or better. 256 colours on an AGA machine (A1200,
        A4000); fewer colours, chosen by itself, on other chip sets. PAL or
        NTSC.

Files
-----
  amigamand        for any 68020 or better. Use this one.
  amigamand-fpu    for 68881/68882, 68040, 68060. Uses the FPU, so it can
                   zoom deeper. Stops at once, with return code 26, when
                   there is no FPU.
  *.info           Workbench icons. The tool types are the start options;
                   those in brackets are switched off (remove the brackets).

Use
---
Double click the icon, or from a Shell:

  amigamand [MODE n] [CX x] [CY y] [SPAN s] [ITER n] [PALETTE name]
            [JULIA] [JR x] [JI y] [SAVE file] [QUIT] [EXACT] [DEPTH n]

  MODE     0 lores, 1 hires (default), 2 hires interlace
  CX CY    the middle of the view      SPAN  its width
  ITER     iterations, 1 to 255 (default 128)
  PALETTE  Rainbow, Fire, Ocean, Gray, Electric
  JULIA    the Julia set for JR + JI i (default -0.8 + 0.156i)
  SAVE     save an ILBM when the picture is finished   QUIT  and quit
  EXACT    no fast fill      DEPTH  bitplanes, 1 to 8 (fewer: faster on a
                             stock A1200, fewer colours)

  amigamand MODE 1 CX -0.7436 CY 0.1318 SPAN 0.02 ITER 255 PALETTE Fire

Mouse
-----
  left click        centre the view there
  left drag         draw a box and zoom into it
  right button      the menu (Right-Amiga shortcuts work)

Keys
----
  + -        zoom in, out          cursor keys   pan
  0          reset the view        r   redraw
  j          Julia set of the point you clicked last     m   Mandelbrot set
  s          save as ILBM          c   cycle the colours
  space      stop drawing          Esc quit

ARexx
-----
The port is AMIGAMAND.

  options results
  address AMIGAMAND
  'ITER 255'
  'PALETTE fire'
  'ZOOM -0.7436 0.1318 0.02'      /* centre x, centre y, width */
  'WAIT'                          /* until the picture is finished */
  'SAVE RAM:spiral.iff'
  'QUERY'                         /* the view, as a result string */
  say result
  'QUIT'

Commands: RENDER ABORT WAIT RESET, ZOOM cx cy span, CENTER x y, SPAN s,
MODE n, ITER n, PALETTE name|n, CYCLE ON|OFF, FAST ON|OFF, JULIA [re im],
MANDELBROT, SAVE file, QUERY, QUIT. A wrong command or argument returns 10.

Limits
------
The fixed point version has 24 bits of fraction: below a span of about 1e-4 the
picture gets blocky. Use amigamand-fpu there.
Made and tested in an emulator (FS-UAE) only. Not yet tried on a real Amiga.

Source, tests, notes: https://github.com/Shadester/amigamand
