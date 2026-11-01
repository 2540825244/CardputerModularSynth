# Cardputer Modular

Mono modular-synth firmware for the M5Stack Cardputer-Adv (ESP32-S3FN8, no PSRAM, mono ES8311).
Its audio feeds a PO-33 K.O! line in for sampling, so capability beats playability.

## Read first
- `docs/superpowers/specs/2026-10-27-cardputer-modular-design.md`: the design. Tags show who
  decided what: [owner] / [proposed] / [Claude's call] / [open].
- `docs/hardware-spec.md`: hardware facts from the official schematics (pins, audio path, USB, memory).
- `docs/reference/`: offline schematics and datasheets (index in its README). The PDFs are
  git-ignored (third-party, not redistributed); fetch them from the README links.
- `docs/design/`: what it should look like. Style B (grey LCD) is the default.
  Screenshots `01`–`05` plus `mockups.html`.

## Next step (as of 2026-10-31, session 10)
Release v1.5 on M5Burner. The owner has played v1.5 on the device and is satisfied. The repo is on
GitHub (public, `origin`; `main` = `feat/v1-firmware`), MIT, with README + `docs/overview.md`.
`tools/release.sh v1.5` writes `dist/modsynth-v1.5-m5burner.bin` (full image at 0x0) and
`-launcher.bin` (the app). Waiting on the owner to flash the full image themselves (it replaces the
Launcher; restore from `dist/`). Then: the M5Burner listing (description with the AI-made /
proof-of-concept / as-is notes and the third-party notices, a cover image). Full-image test passed
2026-11-01; the repo is public.
v1.5 itself: plan `docs/superpowers/plans/2026-10-31-v1.5-sequencers.md`, spec section 16.
Firmware ~1000 KB of the 1 MB slot (~48 KB left). Backlog: `docs/todo.md`.
Pictures are checked natively: `pio test -e native -f test_pictures` writes `.pio/pictures/*.png`.
Execution: Claude implements inline, one Opus reviewer subagent.
The device is on USB only when the owner says so: only ever write the Launcher's `modsyn` slot
(0x650000, image <= 1 MB) via `tools/dev/flash_slot.sh`; full flash backup in `dist/`.
The third-party PDFs in `docs/reference/` are git-ignored; history was rewritten to drop them
(pre-rewrite bundle in `dist/`).

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- `env:cardputer` = dev build (serial diagnostics via `-DMODSYNTH_SERIAL_LOG`, `hw/serial_log.h`);
  `env:release` = what `tools/release.sh` ships (silent, `__FILE__` paths remapped).
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL). Plaits (MIT) is vendored in `lib/plaits`, built with `-DTEST`.
- New presets: edit `tools/presets.py` / `tools/presets_more.py`, then run `python3 tools/presets.py`;
  `python3 tools/preset_levels.py` sets their OUT gain (-3 dBFS) from the native bench (test_presets).
- GitHub: `origin` = github.com/2540825244/CardputerModularSynth, **public** since 2026-11-01. Keep `main`
  in step with `feat/v1-firmware`; push only when the owner asks. Commits use the GitHub noreply
  email (set in this repo's git config), never the university address.
