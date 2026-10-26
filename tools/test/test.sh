#!/bin/sh
# Runs the test suite (see tools/test/README.md). Needs FS-UAE, your Kickstart and Workbench HDF in ~/Documents/FS-UAE.
#   tools/test/test.sh [--update] [test ...]
cd "$(dirname "$0")/../.." || exit 1
make >/dev/null || exit 1
VENV=tools/test/.venv
if [ ! -x "$VENV/bin/python" ]; then
    python3 -m venv "$VENV" && "$VENV/bin/pip" -q install pillow pyobjc-framework-Quartz || exit 1
fi
exec "$VENV/bin/python" tools/test/suite.py "$@"
