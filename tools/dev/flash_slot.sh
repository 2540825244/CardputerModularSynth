#!/bin/sh
# Writes an app image into the Launcher's modsyn slot ONLY (0x650000, 1 MB), resets, captures
# serial. Never write anything else on the owner's device.
#   tools/dev/flash_slot.sh <bin> <listen-secs> <out-file>
set -e
BIN=$1; SECS=${2:-30}; OUT=${3:-/dev/stdout}
PORT=${PORT:-/dev/cu.usbmodem1101}
SIZE=$(stat -f %z "$BIN")
[ "$SIZE" -le 1048576 ] || { echo "image too big for the modsyn slot: $SIZE > 1048576"; exit 1; }
DIR=$(cd "$(dirname "$0")" && pwd)
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port "$PORT" \
  -b 921600 --after hard_reset write_flash -z 0x650000 "$BIN" 2>&1 | tr '\r' '\n' | grep -E "Wrote|Hash|rror"
~/.platformio/penv/bin/python "$DIR/listen.py" "$PORT" "$SECS" > "$OUT"
