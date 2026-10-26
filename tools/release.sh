#!/bin/sh
# Makes the release archives:  tools/release.sh [version]    -> build/release/amigamand-<version>.zip and .lha
#
# Both hold a drawer amigamand/ with the two programs, their icons and ReadMe.txt.
#   .zip   made with the system zip.
#   .lha   made with an LHA that can create archives (-lh5-, header level 1, generic format: what Amiga unpackers read most
#          safely). Lhasa (brew install lhasa) only extracts, so name one with LHA=/path/to/lha, for example a build of
#          LHa for UNIX (https://github.com/jca02266/lha). Without one only the .zip is made.
# The archives are checked by unpacking them again.
VERSION=${1:-1.0}

cd "$(dirname "$0")/.." || exit 1
die() { printf 'release: %s\n' "$*" >&2; exit 1; }
make >/dev/null || die "make failed"
grep -q "amigamand $VERSION " main.c || die "the \$VER: string in main.c is not version $VERSION"

OUT=build/release
rm -rf "$OUT" && mkdir -p "$OUT/amigamand" || exit 1
cp amigamand amigamand-fpu amigamand.info amigamand-fpu.info "$OUT/amigamand/" || exit 1
cp docs/ReadMe.txt "$OUT/amigamand/ReadMe.txt" || exit 1

ZIP=$PWD/$OUT/amigamand-$VERSION.zip
(cd "$OUT" && zip -qr -X "$ZIP" amigamand) || die "zip failed"
unzip -tq "$ZIP" >/dev/null || die "the zip does not pass its own test"
echo "$ZIP"

can_create() { d=$(mktemp -d) && echo x >"$d/f" && (cd "$d" && "$1" ag1 t.lha f >/dev/null 2>&1) && [ -s "$d/t.lha" ]; r=$?; rm -rf "$d"; return $r; }
LHA=${LHA:-lha}
if can_create "$LHA"; then
    ARCHIVE=$PWD/$OUT/amigamand-$VERSION.lha
    (cd "$OUT" && "$LHA" ag1 "$ARCHIVE" amigamand) >/dev/null || die "creating the lha failed"
    "$LHA" t "$ARCHIVE" >/dev/null || die "the lha does not pass its own CRC test"
    echo "$ARCHIVE"
else
    echo "no LHA that can create archives (set LHA=/path/to/lha): only the .zip was made" >&2
fi
