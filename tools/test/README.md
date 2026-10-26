# Tests

    tools/test/test.sh [--update] [test ...]

Needs FS-UAE, and your own Kickstart 3.1 A1200 ROM and Workbench 3.1 HDF in `~/Documents/FS-UAE` (override with the
`KICKSTART` and `WB` environment variables). The first run makes a Python venv here (Pillow, pyobjc Quartz). On macOS the
tests that take screenshots may ask once for the Screen Recording permission. Nothing needs the keyboard focus: keys are posted
to the emulator process. Takes about ten minutes; each test boots a fresh emulator.

| test | what it checks |
|---|---|
| `pixels` | the pens of five views equal `expected.json` (`--update` rewrites it after a change you meant to make) |
| `fast_fill_exact` | the fast fill and the periodicity check draw what plain iteration draws |
| `modes` | interlace is 640x512, 5 planes work |
| `ntsc` | on an NTSC machine the modes have 200 and 400 lines |
| `fpu` | the FPU build draws nearly the same picture; without an FPU it stops with return code 26 and no guru |
| `keys` | zoom, pan, reset and Julia by key give the views a batch run gives |
| `menu_shortcuts` | Right-Amiga+J, M, R |
| `cycling` | colour cycling moves, and stops |
| `abort` | space stops a render |
| `save_requester` | the ASL file requester opens, Return saves, the file is the picture |
| `other_mode_requester` | the ASL screen mode requester opens and is readable |
| `mouse_handlers` | a drag and a click (test builds `amigamand-tm1`, `-tm2` make the gesture through the real handlers) |
| `icon` | `icon.library` loads `amigamand.info` |
| `workbench_tooltypes` | a start from Workbench (test build `amigamand-tw`) takes its options from the tool types |

What cannot be tested here: the mouse itself and the buttons of the ASL requesters (the emulator takes the mouse only when it
has the focus, and these requesters answer only the mouse).

`amiga.py` is the helper: a `Session` boots a binary from a directory hard drive and gives you its screen, keys and the files
it wrote; `decode` reads an ILBM.
