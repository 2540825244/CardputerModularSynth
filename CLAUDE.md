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

## Next step (as of 2026-10-29, session 6)
v1.3 plan `docs/superpowers/plans/2026-10-29-v1.3-perf-and-sampling.md` (perf, SD fix, flash budget,
bypass, KIT demand-driven drum sampler, PO-33 presets, screen sleep). Ledger:
`.superpowers/sdd/2026-10-29-v1.3-perf-and-sampling/progress.md` (trust it and git log after compaction).
Execution: one persistent Opus implementer subagent (impl-v13) does all code; Claude coordinates and
feeds ideas; reviewer rev-v12. The device is on USB: only ever write the Launcher's `modsyn` slot
(0x650000, image <= 1 MB) via `tools/dev/flash_slot.sh`; full flash backup in `dist/`.
Backlog not in the plan: `docs/todo.md`.

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL). Plaits (MIT) is vendored in `lib/plaits`, built with `-DTEST`.
- New presets: edit `tools/presets.py` / `tools/presets_more.py`, then run `python3 tools/presets.py`.
- Git is on `main` with no remote yet. The owner will push to a private GitHub repo later;
  `gh` is logged in.
