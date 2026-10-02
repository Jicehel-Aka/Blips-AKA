#!/usr/bin/env bash
# Runs every test that needs no hardware:
#   1. the engine tests (rules, editor, view port, all shipped levels, a solver replayed on a real level);
#   2. a scripted run of the real SDL build (headless) that solves a level through the UI;
#   3. a scripted run that draws, tests and saves a level in the editor.
# Usage: tests/run_tests.sh [path-to-blips-pc-binary]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "== engine tests"
g++ -std=c++17 -O2 -Wall -Wextra -I"$ROOT/main/engine" -o "$TMP/engine_test" \
    "$ROOT"/tests/engine_test.cpp "$ROOT"/main/engine/blips_*.cpp
"$TMP/engine_test" "$ROOT/SD_files/BLIPS/levelpacks" | tail -n 4

BIN="${1:-}"
if [ -n "$BIN" ]; then
  # The runs below happen in a temporary directory (cd): a relative path would no longer resolve.
  BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
  [ -f "$BIN" ] || [ -f "$BIN.exe" ] || { echo "binary not found: $BIN" >&2; exit 1; }
  PY=python3; command -v python3 >/dev/null 2>&1 || PY=python
  export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy

  echo "== scripted UI run"
  mkdir -p "$TMP/shots"
  "$PY" "$ROOT/tests/make_scripts.py" play "$TMP/play.script" "$TMP/save"
  (cd "$TMP/shots" && BLIPS_DATA="$ROOT/SD_files/BLIPS" BLIPS_SAVE="$TMP/save" AKA_PC_SCRIPT="$TMP/play.script" timeout 120 "$BIN")
  test -f "$TMP/shots/solved.bmp" && test -f "$TMP/shots/level3.bmp"
  "$PY" - "$TMP/save/PROGRESS.DAT" <<'PY'
import struct, sys
d = open(sys.argv[1], "rb").read()
magic, count = struct.unpack_from("<II", d, 0)
assert magic == 0x42505231 and count >= 1, "bad progress file"
name = d[8:48].split(b"\0")[0].decode()
unlocked = struct.unpack_from("<H", d, 48)[0]
print("progress:", name, "unlocked", unlocked)
assert name == "bips" and unlocked == 3, "level 2 was not solved through the UI"
PY
  echo "scripted UI run OK"

  echo "== scripted level editor run"
  mkdir -p "$TMP/edata" "$TMP/eshots"
  cp -r "$ROOT/SD_files/BLIPS/." "$TMP/edata/"          # the editor writes into mylevels/: work on a copy
  rm -rf "$TMP/edata/music"
  "$PY" "$ROOT/tests/make_scripts.py" editor "$TMP/editor.script"
  (cd "$TMP/eshots" && BLIPS_DATA="$TMP/edata" BLIPS_SAVE="$TMP/esave" AKA_PC_SCRIPT="$TMP/editor.script" timeout 120 "$BIN")
  test -f "$TMP/eshots/ed_saved.bmp"
  "$PY" - "$TMP/edata/mylevels/MYPACK1/level1.lev" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
assert len(d) % 3 == 0, "bad level file"
parts = sorted((d[i], d[i + 1], d[i + 2]) for i in range(0, len(d), 3))
print(parts)
want = sorted([(4, x, 25) for x in range(25, 30)] + [(2, 25, 25), (7, 29, 25)])
assert parts == want, "level not saved as drawn"
PY
  echo "scripted editor run OK"
fi
echo "ALL TESTS PASSED"
