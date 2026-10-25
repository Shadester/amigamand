#!/bin/sh
# Build, then boot Workbench 3.1 in FS-UAE as an Amiga 1200 (AGA) with the binaries on a second drive (volume AMIGAMAND:).
# Open a Shell on Workbench and run: AMIGAMAND:amigamand  or  AMIGAMAND:amigamand-fpu
#   CPU=68030 FPU=68882   emulate a faster CPU and an FPU (amigamand-fpu needs one; default: plain A1200, 68020, no FPU)
#   KICKSTART=file   another ROM        WB=file   another Workbench HDF (mounted read-only)        EMULATOR=file   another fs-uae
# The Kickstart ROM and the Workbench HDF are your own: by default the ones in ~/Documents/FS-UAE.
cd "$(dirname "$0")/.." || exit 1
make >/dev/null || exit 1
FSUAE=$HOME/Documents/FS-UAE
ROM=${KICKSTART:-$FSUAE/Kickstarts/Kickstart-v3.1-rev-40.68-1993-Commodore-A1200.rom}
WB=${WB:-$FSUAE/Hard Drives/workbench-311.hdf}
EMU=${EMULATOR:-/Applications/FS-UAE.app/Contents/MacOS/fs-uae}
[ -f "$ROM" ] || { echo "No Kickstart ROM at $ROM (set KICKSTART=)" >&2; exit 1; }
[ -f "$WB" ] || { echo "No Workbench HDF at $WB (set WB=)" >&2; exit 1; }
[ -x "$EMU" ] || { echo "$EMU not found (brew install --cask fs-uae-emulator)" >&2; exit 1; }

rm -rf build/hd && mkdir -p build/hd || exit 1
cp amigamand amigamand-fpu build/hd/ || exit 1
set -- ${CPU:+--cpu=$CPU} ${FPU:+--uae_fpu_model=$FPU}
exec "$EMU" --amiga_model=A1200 --kickstart_file="$ROM" "$@" \
    --hard_drive_0="$WB" --hard_drive_0_read_only=1 \
    --hard_drive_1=build/hd --hard_drive_1_label=AMIGAMAND \
    --automatic_input_grab=0 --fullscreen=0 --window_width=960 --window_height=720 --floppy_drive_volume=0
