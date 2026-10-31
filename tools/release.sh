#!/bin/sh
# Builds a release into dist/:
#   modsynth-<ver>-m5burner.bin  full image for M5Burner / esptool at 0x0 (bootloader, partitions,
#                                boot_app0, app), as `pio run -t upload` would write it
#   modsynth-<ver>-launcher.bin  the app alone, for bmorcelli's Launcher (SD install)
#   tools/release.sh v1.5
set -e
VER=${1:?usage: tools/release.sh <version, e.g. v1.5>}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PIO=~/.platformio/penv/bin
BUILD=$ROOT/.pio/build/cardputer
BOOT_APP0=~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin
FULL=$ROOT/dist/modsynth-$VER-m5burner.bin
APP=$ROOT/dist/modsynth-$VER-launcher.bin

cd "$ROOT"
"$PIO/pio" run -e cardputer
mkdir -p dist

# Offsets and flash settings are PlatformIO's for this board (FLASH_EXTRA_IMAGES in
# `pio run -t envdump`; qio boards are written as dio). Partitions: default_8MB.csv, app0 at 0x10000.
"$PIO/python" ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 merge_bin -o "$FULL" \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 "$BUILD/bootloader.bin" 0x8000 "$BUILD/partitions.bin" 0xe000 "$BOOT_APP0" \
  0x10000 "$BUILD/firmware.bin"
cp "$BUILD/firmware.bin" "$APP"

# The app inside the full image must be the app byte for byte.
APP_SIZE=$(stat -f %z "$APP")
tail -c +65537 "$FULL" | head -c "$APP_SIZE" | cmp -s - "$APP" ||
  { echo "app at 0x10000 differs from firmware.bin"; exit 1; }

echo
shasum -a 256 "$FULL" "$APP"
