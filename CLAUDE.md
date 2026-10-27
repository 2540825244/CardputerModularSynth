# Cardputer Modular

Mono modular-synth firmware for the M5Stack Cardputer-Adv (ESP32-S3FN8, no PSRAM, mono ES8311).
Its audio feeds a PO-33 K.O! line in for sampling, so capability beats playability.

## Read first
- `docs/superpowers/specs/2026-10-27-cardputer-modular-design.md`: the design. Tags show who
  decided what: [owner] / [proposed] / [Claude's call] / [open].
- `docs/hardware-spec.md`: hardware facts from the official schematics (pins, audio path, USB, memory).
- `docs/design/`: what it should look like. Style B (grey LCD) is the default.
  Screenshots `01`–`05` plus `mockups.html`.

## Next step (as of 2026-10-27)
The design was written and refined with the owner in the brainstorm session, but **the owner hasn't
formally approved it yet**.
1. Ask the owner whether they've read the design and approve it, or want changes.
   Update the doc if they do.
2. Once approved, write the implementation plan with the `superpowers:writing-plans` skill,
   scoped to the **v1 release** (design §8.1): KEYS, OSC, VCF, VCA, ENV, LFO, DLY, OUT modules,
   carry-the-cable patching, SD save/load, separate speaker/line volume. Keep the module system
   expandable for §8.2.
3. Plan early tasks to **check the hardware unknowns on the real device**. The owner flashes and
   listens; Claude can't hear the output:
   - I2S/ES8311 at 48 kHz with no MCLK
   - whether `HP_DET` is readable from a GPIO
   - keyboard ghosting with modifier + key
   - M5Unified/M5Cardputer support for the Adv
   - free RAM in practice

## Constraints to remember
- PlatformIO + Arduino core. `pio` isn't on PATH: use `~/.platformio/penv/bin/pio`.
- Delivered as a `.bin` for bmorcelli's Launcher. **No custom partitions.** Assets go on SD
  under `/modsynth/`.
- DaisySP (MIT core only, not DaisySP-LGPL) now. Plaits later.
- Git is on `main` with no remote yet. The owner will push to a private GitHub repo later;
  `gh` is logged in.
