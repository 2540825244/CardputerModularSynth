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

## Next step (as of 2026-10-28, session 4)
v1.2 "feature complete" (plan `docs/superpowers/plans/2026-10-28-v1.2-feature-complete.md`, spec §14 + §14.1) is
built on branch `feat/v1-firmware`: Tasks 1–13 done, review rounds 1–2 fixed, GRD is now a whole Grind voice,
and adding modules checks measured CPU. Ledger with every ruling: `.superpowers/sdd/2026-10-28-v1.2-feature-complete/progress.md`.
1. Owner flashes `dist/modsynth-v1.2-test2.bin` and runs `dist/test-4-script.md` with the serial monitor on.
   The `cost id=measured/estimate` lines give the real cpuCycles: put them into the type tables
   (GRD = its worst engine). Also watch `stackHw ... audio=` (8 KB stack) and "NO RAM" toasts.
2. Fix what the report finds, then Task 14: final whole-branch review, finish the branch.
3. Execution mode: native, plus one persistent Opus reviewer agent. No other subagents.
   Automatic speaker/line switching stays manual (owner, 2026-10-28). USB MIDI host is deferred.

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL). Plaits (MIT) is vendored in `lib/plaits`, built with `-DTEST`.
- New presets: edit `tools/presets.py` / `tools/presets_more.py`, then run `python3 tools/presets.py`.
- Git is on `main` with no remote yet. The owner will push to a private GitHub repo later;
  `gh` is logged in.
