# Cardputer Modular

Mono modular-synth firmware for the M5Stack Cardputer-Adv (ESP32-S3FN8, no PSRAM, mono ES8311).
Its audio feeds a PO-33 K.O! line in for sampling, so capability beats playability.

## Read first
- `docs/superpowers/specs/2026-10-27-cardputer-modular-design.md`: the design. Tags show who
  decided what: [owner] / [proposed] / [Claude's call] / [open].
- `docs/hardware-spec.md`: hardware facts from the official schematics (pins, audio path, USB, memory).
- `docs/reference/`: offline schematics and datasheets (index in its README).
- `docs/design/`: what it should look like. Style B (grey LCD) is the default.
  Screenshots `01`–`05` plus `mockups.html`.

## Next step (as of 2026-10-29, session 8)
v1.4 plan `docs/superpowers/plans/2026-10-29-v1.4-pictures-modules-presets.md` is built and reviewed
(ledger `.superpowers/sdd/2026-10-29-v1.4-pictures-modules-presets/progress.md`; trust it and git log).
v1.4 = a picture for every module (`ui/pictures*`, `ui/pic_*.cpp`, drawn through `ui/gfx.h`), ARP CLP
PRC VOW EQ3, PRESETS 2 page; also the keyboard-freeze and false CPU FULL fixes. Handed to the owner:
`dist/modsynth-v1.4-test1.bin` (406235c, 981 KB of the 1 MB slot: ~67 KB left) + `dist/test-7-script.md`,
made without the device; v1.3's test 6 results never came back. Next: the owner's device results,
then the deferred items in both ledgers (v1.3 Task 9 perf needs the device bench; v1.4 costs are
estimates). Pictures are checked natively: `pio test -e native -f test_pictures` writes
`.pio/pictures/*.png` and `_sheet.png`, look at them after a painter change.
Execution: Claude implements inline, one Opus reviewer subagent.
The device is on USB only when the owner says so: only ever write the Launcher's `modsyn` slot
(0x650000, image <= 1 MB) via `tools/dev/flash_slot.sh`; full flash backup in `dist/`.
Backlog not in the plan: `docs/todo.md`.

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL). Plaits (MIT) is vendored in `lib/plaits`, built with `-DTEST`.
- New presets: edit `tools/presets.py` / `tools/presets_more.py`, then run `python3 tools/presets.py`;
  `python3 tools/preset_levels.py` sets their OUT gain (-3 dBFS) from the native bench (test_presets).
- Git is on `main` with no remote yet. The owner will push to a private GitHub repo later;
  `gh` is logged in.
