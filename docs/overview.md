# Cardputer Modular: overview

The short version is in the [README](../README.md). This page has the details.

> A proof of concept, provided as is with no guarantee. Nearly all code and docs were written by
> Claude (Anthropic's AI model) under the owner's direction; see [How it was made](#how-it-was-made).

## What it is

Firmware that turns the M5Stack Cardputer-Adv (ESP32-S3, 240×135 screen, 56-key keyboard) into a
**monophonic modular synth**. Everything happens on the device: you place modules, patch cables,
set knobs, and save patches to the SD card.

**What it's for:** the owner samples its output into a Teenage Engineering PO-33 K.O! through the
line in. So the design puts **depth and range of sound before playability**: one voice is enough,
but that voice can be a full modular patch.

**Why another Cardputer firmware:** the 12×4 middle of the keyboard maps one to one onto a 12×4 grid
on screen. Every key is a cell, so there are no text menus to dig through. The look follows the
Teenage Engineering style: drawn and animated, on a grey "pocket LCD" theme.

## Status

- v1.5 is the current build (sequencers, piano roll, global transport). Earlier builds ran on the
  owner's Cardputer-Adv; v1.5 has not been tried on the device yet.
- The firmware is about 1000 KB, close to the 1 MB app slot it is tested in.
- Known gaps and ideas: [todo.md](todo.md). USB MIDI host is wanted but not built.

## Install

1. Install [bmorcelli's Launcher](https://github.com/bmorcelli/Launcher) on the Cardputer-Adv.
   This firmware does not bring its own partition table and relies on the Launcher's.
2. Build the `.bin` (below), copy it to the SD card, and install it from the Launcher.
3. Patches are saved on the SD card under `/modsynth/patches/`. The current patch autosaves.

Audio comes out of the speaker and the 3.5 mm jack. A plug in the jack mutes the speaker in
hardware. The OUT module keeps separate levels for speaker and line.

## Build and test

Needs PlatformIO (the Arduino core on ESP-IDF, pinned to `espressif32@6.11.0`).

```sh
pio run -e cardputer          # firmware: .pio/build/cardputer/firmware.bin
pio test -e native            # host-side tests (DSP, patching, UI, presets)
pio test -e native -f test_pictures   # renders every module's screen to .pio/pictures/*.png
tools/release.sh v1.5         # dist/: a full image for M5Burner (0x0) and the app alone for the Launcher
```

Presets are generated: edit `tools/presets.py` / `tools/presets_more.py`, then run
`python3 tools/presets.py` and `python3 tools/preset_levels.py` (which sets each preset's output to
-3 dBFS from the native bench).

## Using it

The keyboard is 4 rows × 14 keys. The middle 12 columns are the grid; the outer column on each side
holds the special keys, and their on-screen icons change with the view.

```
[esc]  1 2 3 4 5 6 7 8 9 0 - =  [del]
[tab]  q w e r t y u i o p [ ]  [ \ ]
[fn ]  Aa a s d f g h j k l ; ' [enter]
[ctrl] opt alt z x c v b n m , . / [space]
```

**Two views:**
- **Patch view:** one cell is one module. Tap a key to select, `fn`+tap (or hold, or `enter`)
  to open a module or place one on an empty cell. `tab` flips pages, `ctrl` moves, `del` deletes,
  `fn`+`del` bypasses, `\` undoes, `space` plays (or auditions the keyboard), `esc` opens the file
  menu (load, save, presets).
- **Module view:** the top row is a value strip for the selected knob: number keys jump to coarse
  values, `ctrl` / `space` nudge it, holding `enter` and tilting the device sweeps it. The rows below
  hold the knobs, the pins and a live picture of what the module does.

**Patching ("carry the cable"):** `fn` on a pin picks up a cable; walk to another module and `fn` on
the target pin to drop it. Outputs fan out; cables into one input add up. Every output has an amount
from -100 % to +100 %. Cables show as colour-matched dots, not lines. Holding `fn`+`space` on an input
sends it a manual +5 V gate.

**Signals:** everything runs at audio rate (48 kHz, float), so anything patches into anything.
Eurorack-style voltages: 1 V/oct pitch, about ±5 V audio and CV.

## Modules

| Kind | Modules |
|---|---|
| Sources | OSC (basic waves), VSO (saw-square morph, sync), SSW (supersaw), FMO (2-op FM), FOR (formant), VOS (VOSIM), ZOS, HRM (16 harmonics), NOI (noise), STR (plucked string), MDL (modal bells), PRT (particles), **GRD** (Plaits engines + ladder + envelope, after the Behringer Grind), KEY (keyboard, 6 layouts) |
| Drums | KCK, SNR, HAT, CLP, PRC (tom, conga, cowbell, clave, rim); KIT (8-lane drum pattern) |
| Processors | VCF (12 dB multimode), LAD (ladder 24/12 dB), VCA, LPG (low-pass gate), VOW (vowel filter), RES (modal resonator), FLD (wavefolder), OVD (overdrive), CRU (bit crusher), RNG (ring mod), DYN (compressor), EQ3 |
| Effects | DLY (1 s, clock sync), REV (plate), CHO, FLG, PHA, TRM, AWH (auto-wah), PSH (pitch shift) |
| Modulators | ENV (ADSR), AD, LFO, RND, CHS (Lorenz chaos), ENF (envelope follower), TLT (device tilt), CLK |
| Sequencing | SEQ (12 steps), ROL (piano roll, 32 steps, notes of any length), EVO (a line that mutates each pass), TRG (3 trigger lanes), EUC (Euclidean), TUR (Turing machine), ARP |
| Utilities | MIX, ATN (attenuverter + offset), XFD, MUX, DMX, QNT (quantizer), SLW (slew), LGC (logic), BRN (Bernoulli gate), KNB (four knobs), SCO (scope and tuner), OUT |

The place menu shows each module's details and CPU cost before you place it. A CPU guard keeps
a patch from overloading the audio core.

**Presets** (file menu → PRESETS, PRESETS 2): 53 patches, including BASIC, ACID, PLUCK, FMBELL,
DRONE, TURING, GRIND, four drum kits, basses, leads, pads, arpeggios, and ROLL / EVOLVE / DUET for
the sequencers.

## Hardware notes

- ESP32-S3FN8: two cores at 240 MHz, 512 KB SRAM, **no PSRAM**, 8 MB flash. RAM is the limit
  for delays and reverbs.
- ES8311 codec, mono, clocked from BCLK. Keyboard via a TCA8418 on I2C.
- Pin-level facts from the official schematics: [hardware-spec.md](hardware-spec.md). The
  schematics and datasheets themselves are not in this repo; [reference/README.md](reference/README.md)
  links to them.

## Repo layout

| Path | What |
|---|---|
| `src/core/` | Engine, patch model, undo, presets, transport; `modules/` has every module |
| `src/ui/` | Views, gestures, module pictures |
| `src/hw/`, `src/render/` | Audio, keyboard and display drivers; drawing |
| `lib/DaisySP`, `lib/plaits` | Vendored DSP libraries (MIT) |
| `test/` | Native unit tests (Unity) |
| `tools/` | Preset generator, level tool, build helpers |
| `docs/superpowers/specs/` | The design document, with who decided what |
| `docs/superpowers/plans/` | One implementation plan per release (v1 to v1.5) |
| `docs/design/` | UI mockups |

## How it was made

The project was built over a few days in October 2026 in sessions with Claude Code. The owner
supplied the idea, the goals and the design decisions, reviewed mockups, and ran the builds on the
hardware. Claude wrote nearly all the code, tests, tools and docs, from a design spec it drafted
with the owner, and a second Claude instance reviewed each release.

The design spec tags every decision as **[owner]**, **[proposed]** (Claude suggested, owner agreed),
**[Claude's call]** or **[open]**, so you can see which choices were the owner's:
[the design](superpowers/specs/2026-10-27-cardputer-modular-design.md).

## License

MIT, see [LICENSE](../LICENSE). DaisySP, Plaits and the build-time libraries keep their own
licenses: [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Provided as is, without warranty.
