#!/bin/sh
# Runs the plug-in in a headless GIMP with a temporary profile (the user's
# GIMP configuration is not touched) and checks loading/exporting.
#
# Set GIMP_TEST_DIR to reuse a profile.
set -e
cd "$(dirname "$0")/.."

cargo build

TMP=${GIMP_TEST_DIR:-$(mktemp -d)}
mkdir -p "$TMP/profile/plug-ins/arma-gimp-plugin"
cp target/debug/arma-gimp-plugin "$TMP/profile/plug-ins/arma-gimp-plugin/"

GIMP_CONSOLE=${GIMP_CONSOLE:-$(command -v gimp-console-3 || command -v gimp-console)}
GIMP3_DIRECTORY="$TMP/profile" GIMP_TEST_DIR="$TMP" "$GIMP_CONSOLE" \
    -i -n --quit --batch-interpreter=python-fu-eval \
    -b "exec(open('tests/gimp_test.py').read())" \
    2>&1 | tee "$TMP/log.txt" | grep -E '^(PASS|FAIL)'

! grep -q '^FAIL' "$TMP/log.txt" && grep -q '^PASS all' "$TMP/log.txt"
