#!/bin/sh
# Sets up what the build needs, inside the repository (nothing is installed anywhere else).
#
#   tools/setup.sh          build the Amiga cross toolchain into toolchain/vbcc (about two minutes)
#   tools/setup.sh check    only report what is there and what is missing
#
# The toolchain is vbcc (C compiler), vasm (assembler), vlink (linker) and the Amiga target and headers:
#   vbcc, vasm       from Volker Barthelmann's server (compilers.de)
#   vlink            the Leffmann/vlink mirror on GitHub, pinned to a commit (the official download address was dead)
#   target, config   vbcc's AmigaOS target and its Unix config files
#   headers          the NDK 3.2 include files from Aminet (no 3.1 or 3.9 set is there; do not call 3.2-only functions)
# Every download is checked against the SHA-256 in this file: the servers speak plain HTTP, and a changed file could be
# a changed compiler. If upstream has legitimately changed a file the check fails; look at it, then update the sum here
# (or run with SKIP_VERIFY=1 once).
#
# Needs: cc, make, curl, tar, lha (unpacks the .lha files; the NDK one uses a compression bsdtar cannot read:
# macOS `brew install lhasa`, Debian/Ubuntu `apt install lhasa`), shasum or sha256sum.
# Written for macOS; the sources are portable C and should build on Linux too (not tried).
#
# Not installed here, and not redistributable: your own Kickstart 3.1 ROM and Workbench 3.1 disk (see `check`).
#   TOOLCHAIN=dir   install somewhere else (then run make with VBCC=dir)
#   KEEP_WORK=1     keep the build directory

VBCC_URL=http://www.ibaug.de/vbcc/vbcc.tar.gz
VBCC_SHA=67e221062b50fb590613487ec2948a00db80552e061bbe348e6eaed5b33f0af8
VASM_URL=http://www.ibaug.de/vasm/vasm.tar.gz
VASM_SHA=fe1b1aa3dd42c55ba047ec8347ec251b72f8f4791ea921a52578ae5d5eccaa6a
VLINK_COMMIT=d78459de313df5fd850f4c240aad8b00b65526ac
VLINK_URL=https://github.com/Leffmann/vlink/archive/$VLINK_COMMIT.tar.gz
VLINK_SHA=13e167bcfee7de80c496c3671068a3052ce17dc5ba4914d60bbc4109393dff82
CONFIG_URL=http://phoenix.owl.de/vbcc/current/vbcc_unix_config.tar.gz
CONFIG_SHA=c643a095439aaf0cbcf7521920c0ff5d1cb07a4fc6fe8cc59e69ae1dbb329497
TARGET_URL=http://phoenix.owl.de/vbcc/current/vbcc_target_m68k-amigaos.lha
TARGET_SHA=ec734d7115359cdb5d1c70349284ecbd5712ef47e8bc28d4150117c6f8c73289
NDK_URL=http://aminet.net/dev/misc/NDK3.2.lha
NDK_SHA=96cabd4ad683dced632e147bf86dee0f50dcb1254386216c25c362916a6409bb

cd "$(dirname "$0")/.." || exit 1
ROOT=$PWD
DEST=${TOOLCHAIN:-$ROOT/toolchain/vbcc}
CACHE=$ROOT/toolchain/cache
FSUAE=$HOME/Documents/FS-UAE

say() { printf '%s\n' "$*"; }
die() { printf 'setup: %s\n' "$*" >&2; exit 1; }

check() {
    ok=0
    report() { # name, found (0/1), hint
        if [ "$2" = 1 ]; then say "  ok       $1"; else say "  MISSING  $1  ($3)"; ok=1; fi
    }
    say "Toolchain ($DEST):"
    for f in vc vbccm68k vasmm68k_mot vlink; do
        [ -x "$DEST/bin/$f" ] && r=1 || r=0
        report "bin/$f" $r "run tools/setup.sh"
    done
    [ -f "$DEST/targets/m68k-amigaos/include/proto/exec.h" ] && r=1 || r=0
    report "AmigaOS target and proto headers" $r "run tools/setup.sh"
    [ -f "$DEST/targets/m68k-amigaos/include/intuition/intuition.h" ] && r=1 || r=0
    report "NDK headers" $r "run tools/setup.sh"
    [ -f "$DEST/config/aos68k" ] && r=1 || r=0
    report "config/aos68k" $r "run tools/setup.sh"
    command -v lha >/dev/null && r=1 || r=0
    report "lha, to unpack the Amiga archives while setting up" $r "brew install lhasa"
    say "Running and testing (optional):"
    [ -x /Applications/FS-UAE.app/Contents/MacOS/fs-uae ] && r=1 || r=0
    report "FS-UAE" $r "brew install --cask fs-uae-emulator"
    [ -f "$FSUAE/Kickstarts/Kickstart-v3.1-rev-40.68-1993-Commodore-A1200.rom" ] && r=1 || r=0
    report "Kickstart 3.1 A1200 ROM in $FSUAE/Kickstarts" $r "your own copy; or set KICKSTART=file"
    [ -f "$FSUAE/Hard Drives/workbench-311.hdf" ] && r=1 || r=0
    report "Workbench 3.1 disk in $FSUAE/Hard Drives/workbench-311.hdf" $r "your own copy; or set WB=file"
    command -v python3 >/dev/null && r=1 || r=0
    report "python3 (tests; their venv with Pillow and pyobjc is made by tools/test/test.sh)" $r "install Python 3"
    return $ok
}

if [ "$1" = check ]; then
    check
    exit $?
fi
[ -n "$1" ] && die "unknown argument: $1 (use: tools/setup.sh [check])"

for c in cc make curl tar lha; do
    command -v $c >/dev/null || die "$c is needed but not found (lha: brew install lhasa)"
done
if command -v sha256sum >/dev/null; then SUM="sha256sum"; else SUM="shasum -a 256"; fi

fetch() { # url sha -> the file name in the cache
    f=$CACHE/$(basename "$1")
    case $1 in *github.com*) f=$CACHE/vlink-$VLINK_COMMIT.tar.gz ;; esac
    if [ ! -f "$f" ]; then
        say "  downloading $1" >&2
        curl -fL --retry 2 --max-time 600 -sS -o "$f.part" "$1" || die "download failed: $1"
        mv "$f.part" "$f"
    fi
    if [ "$SKIP_VERIFY" != 1 ]; then
        got=$($SUM "$f" | cut -d' ' -f1)
        [ "$got" = "$2" ] || die "$(basename "$f") has SHA-256 $got, expected $2 (see the comment at the top of this script)"
    fi
    printf '%s' "$f"
}

mkdir -p "$CACHE" "$DEST/bin" "$DEST/targets" || die "cannot make directories"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/amigamand-setup.XXXXXX") || die "no temp directory"
[ "$KEEP_WORK" = 1 ] || trap 'rm -rf "$WORK"' EXIT
LOG=$WORK/build.log
build() { # what, dir, command...  (the log is shown if it fails)
    what=$1 dir=$2
    shift 2
    say "  building $what"
    (cd "$dir" && "$@") >>"$LOG" 2>&1 || { tail -30 "$LOG" >&2; die "building $what failed (log: $LOG)"; }
}

say "Fetching (cached in toolchain/cache):"
vbcc_tgz=$(fetch "$VBCC_URL" "$VBCC_SHA")
vasm_tgz=$(fetch "$VASM_URL" "$VASM_SHA")
vlink_tgz=$(fetch "$VLINK_URL" "$VLINK_SHA")
config_tgz=$(fetch "$CONFIG_URL" "$CONFIG_SHA")
target_lha=$(fetch "$TARGET_URL" "$TARGET_SHA")
ndk_lha=$(fetch "$NDK_URL" "$NDK_SHA")

say "Building:"
mkdir -p "$WORK/vbcc" "$WORK/vasm" "$WORK/vlink" "$WORK/target" "$WORK/ndk"
# the vbcc archive holds hard links to itself in unused back ends: tar complains, the files we need are fine
tar xzf "$vbcc_tgz" -C "$WORK/vbcc" --strip-components=1 2>/dev/null
[ -f "$WORK/vbcc/Makefile" ] || die "the vbcc archive did not unpack"
tar xzf "$vasm_tgz" -C "$WORK/vasm" --strip-components=1 || die "the vasm archive did not unpack"
tar xzf "$vlink_tgz" -C "$WORK/vlink" --strip-components=1 || die "the vlink archive did not unpack"
mkdir -p "$WORK/vbcc/bin" "$WORK/vasm/obj" "$WORK/vlink/objects"
# vbcc's make asks the host's type sizes: dtgen reads its answers (all the defaults) from standard input
build vbcc "$WORK/vbcc" sh -c "yes '' | head -50 | make TARGET=m68k"
build vasm "$WORK/vasm" make CPU=m68k SYNTAX=mot
build vlink "$WORK/vlink" make

say "Installing into $DEST:"
cp "$WORK/vbcc/bin/vc" "$WORK/vbcc/bin/vbccm68k" "$WORK/vbcc/bin/vprof" "$DEST/bin/" || die "vbcc did not build"
cp "$WORK/vasm/vasmm68k_mot" "$WORK/vasm/vobjdump" "$WORK/vlink/vlink" "$DEST/bin/" || die "vasm or vlink did not build"
# the Unix config files; the target package has its own config for an Amiga (it names assigns like vincludeos3:), not used
tar xzf "$config_tgz" -C "$DEST" || die "the config archive did not unpack"
(cd "$WORK/target" && lha xq "$target_lha") || die "the target archive did not unpack"
cp -R "$WORK/target/vbcc_target_m68k-amigaos/targets/." "$DEST/targets/" || die "no targets in the target package"
(cd "$WORK/ndk" && lha xq "$ndk_lha") || die "the NDK archive did not unpack"
# the NDK headers fill in what vbcc's target does not have; its own files win (cp -n)
cp -Rn "$WORK/ndk/Include_H/." "$DEST/targets/m68k-amigaos/include/" 2>/dev/null   # exits 1 for each file it skips
[ -f "$DEST/targets/m68k-amigaos/include/intuition/intuition.h" ] || die "no NDK headers"

say "Trying it:"
printf '#include <exec/types.h>\n#include <proto/exec.h>\n#include <intuition/intuition.h>\nint main(void) { return 0; }\n' >"$WORK/t.c"
VBCC=$DEST PATH=$DEST/bin:$PATH "$DEST/bin/vc" +aos68k -cpu=68020 -c -o "$WORK/t.o" "$WORK/t.c" || die "the new compiler cannot compile an Amiga program"
say "  ok: an Amiga program compiles"
say ""
check
say ""
say "Now: make"
