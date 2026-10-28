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

## Next step (as of 2026-10-28, session 5)
v1.2 "feature complete" (plan `docs/superpowers/plans/2026-10-28-v1.2-feature-complete.md`, spec §14 + §14.1) is
on branch `feat/v1-firmware`. Review rounds 1-5b are fixed; the RAM rework freed ~120 KB (static 153.9 KB).
Ledger with every ruling: `.superpowers/sdd/2026-10-28-v1.2-feature-complete/progress.md`.
**Agreed-but-unbuilt changes live in `docs/todo.md`**: add to it when the owner OKs something for later.
1. Owner flashes `dist/modsynth-v1.2-test3.bin` and runs `dist/test-5-script.md` with the serial monitor on.
   The `cost id=measured/estimate` lines give the real cpuCycles for the type tables.
2. Fix what the report finds, build the `docs/todo.md` items, then Task 14: final whole-branch review.
3. Execution mode: native, plus one persistent Opus reviewer agent (rev-v12). No other subagents.
   Automatic speaker/line switching stays manual (owner, 2026-10-28). USB MIDI host is deferred.

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL). Plaits (MIT) is vendored in `lib/plaits`, built with `-DTEST`.
- New presets: edit `tools/presets.py` / `tools/presets_more.py`, then run `python3 tools/presets.py`.
- Git is on `main` with no remote yet. The owner will push to a private GitHub repo later;
  `gh` is logged in.
