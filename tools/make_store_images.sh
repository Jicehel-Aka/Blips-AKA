#!/usr/bin/env bash
# Regenerates SD_files/BLIPS/Picture.png (title screen) and screen.bmp (a game screen) by driving the
# real PC build with a script (headless). Usage: tools/make_store_images.sh path/to/blips
set -euo pipefail
BIN="${1:?usage: make_store_images.sh path/to/blips}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
python3 - "$TMP/shots.script" <<'PY'
import sys
l = ["10 shot title.bmp", "12 tap a", "20 tap a"]    # title screenshot, "Jouer", then the pack "bips"
f = 28
l.append("%d tap a" % f); f += 30                      # start level 1
l.append("%d press up" % f); f += 60                   # walk up the corridor a bit
l.append("%d release up" % f); f += 20
l.append("%d shot game.bmp" % f); f += 2
l.append("%d quit" % f)
open(sys.argv[1], "w").write("\n".join(l) + "\n")
PY
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy BLIPS_DATA="$ROOT/SD_files/BLIPS" BLIPS_SAVE="$TMP/save"
(cd "$TMP" && AKA_PC_SCRIPT="$TMP/shots.script" "$BIN" >/dev/null)
python3 - "$TMP" "$ROOT/SD_files/BLIPS" <<'PY'
import sys
from PIL import Image
tmp, out = sys.argv[1:3]
Image.open(tmp + "/title.bmp").convert("RGB").save(out + "/Picture.png", optimize=True)
Image.open(tmp + "/game.bmp").convert("RGB").save(out + "/screen.bmp")
print("wrote Picture.png and screen.bmp")
PY
