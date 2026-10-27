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

## Next step (as of 2026-10-27)
The owner approved the design on 2026-10-27. The v1 implementation plan is
`docs/superpowers/plans/2026-10-27-v1-firmware.md` (22 tasks). It also decides the spec's §12 open
items as [Claude's call] (see the table at its top).
1. If not done yet: the owner reviews the plan and picks an execution method (subagent-driven or native).
2. Execute it task by task. Tasks 2, 18, 19, 20 and 21 are **owner checkpoints**: the owner flashes and
   reports back (Claude can't hear the output or press keys). Stop and wait at each one.
Probe done 2026-10-27, see docs/probe-results.md; chords are reliable (owner-confirmed).

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL) now. Plaits later.
- Git is on `main` with no remote yet. The owner will push to a private GitHub repo later;
  `gh` is logged in.
