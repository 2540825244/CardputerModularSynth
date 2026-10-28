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

## Next step (as of 2026-10-28)
v1 (plan `docs/superpowers/plans/2026-10-27-v1-firmware.md`) is implemented. The current plan is
`docs/superpowers/plans/2026-10-28-v1.1-controls-and-utilities.md`: unified do-it gesture, hold-enter
tilt, 7 new modules. Tasks 1–8 are done and reviewed (branch `feat/v1-firmware`). It is paused at the
**Task 9 owner checkpoint**. Progress and every ruling: `.superpowers/sdd/2026-10-28-v1.1-controls-and-utilities/progress.md`
(read its RESUME NOTE first).
1. Owner flashes `dist/modsynth-v1.1-test.bin` and runs `dist/test-2-script.md`. So far reported: tilt
   ran opposite on both axes, now negated in `src/hw/tilt.cpp` (d397671, not yet re-checked). Still to
   report: tilt feel after the flip, gestures, file menu, new modules, first-boot audio.
2. Fix what the report finds, then Task 10 (release notes, final whole-branch review, finish the branch).
3. Execution mode the owner chose: native, plus one persistent Opus reviewer agent. No other subagents.

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL) now. Plaits later.
- Git is on `main` with no remote yet. The owner will push to a private GitHub repo later;
  `gh` is logged in.
