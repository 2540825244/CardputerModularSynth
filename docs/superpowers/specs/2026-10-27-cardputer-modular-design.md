# Cardputer Modular — Design

Status: approved by the owner on 2026-10-27. Written in the brainstorm session of the same day.
Implementation plan for v1: [`docs/superpowers/plans/2026-10-27-v1-firmware.md`](../plans/2026-10-27-v1-firmware.md).
Hardware facts: [`docs/hardware-spec.md`](../../hardware-spec.md).
Look: [`docs/design/`](../../design/) has the mockup page (`mockups.html`) and screenshots `01`–`05`.

Each decision says whose it is:
- **[owner]**: the project owner said or chose it.
- **[proposed]**: Claude suggested it and the owner agreed or accepted it without objection.
- **[Claude's call]**: decided by Claude so the design is complete. Change it freely.
- **[open]**: not decided. Left to the builder or for later.

---

## 1. Intent

A modular-synth firmware for the **M5Stack Cardputer-Adv** (ESP32-S3, 240×135 screen,
56-key keyboard). You build a full modular patch on the device itself: put modules on a
grid, patch cables between their inputs and outputs, and tweak parameters. **[owner]**

- **Main use:** feed the audio output into a **Teenage Engineering PO-33 K.O!** line in and
  sample it there. So **capability beats playability**: depth and range of sound come
  before performance ergonomics. **[owner]**
- **Monophonic is fine.** **[owner]**
- **Why it exists:** the owner doesn't like how other Cardputer firmwares work. This one should
  be **intuitive**. The intuition comes from the 12×4 key grid matching the on-screen
  grid one-to-one. **[owner]**
- **Feel:** stylised and fun, in the Teenage Engineering vein. Things are drawn and animated, not
  plain text menus. **[owner]**

## 2. Goals, non-goals, anti-goals

**Goals**
- A full modular patch (sources, modulators, processors, effects, output) fits on the device. **[owner]**
- Grid-based UI where every grid key is a point on screen. **[owner]**
- Patches saved to and loaded from SD. **[owner]**
- Distributed as a `.bin` that runs from **bmorcelli's Launcher**, so the owner can switch firmwares. **[owner]**
- Expandable: the first release is small, but the module system must take new modules easily. **[owner]**
- Later: USB MIDI host for a MIDI keyboard (preferred input), and a Plaits-based macro oscillator.
  The owner has a Behringer Grind (a Plaits clone) and knows those engines. **[owner]**

**Non-goals**
- Polyphony. **[owner]**
- Long sample memory or sampling. The PO-33 does the sampling. **[proposed]**
- Drawing patch cables as lines on screen. Colour-matched dots replace them. **[owner]**
- The Cap LoRa-1262: no planned use. **[proposed]**

**Anti-goals** (it fails if any of these are true, even when it works technically)
- It feels like the other Cardputer firmwares: menu-diving, text lists, unclear key mappings. **[owner]**
- Audio clicks, dropouts or crashes while editing a patch. **[proposed]**
- A firmware update makes saved patches unloadable. **[proposed]**

## 3. Constraints

From `docs/hardware-spec.md`, condensed:

- **CPU:** ESP32-S3FN8, two cores at 240 MHz, hardware float in single precision only. At 48 kHz
  that's ≈5,000 cycles per sample per core.
- **Memory:** 512 KB SRAM, **no PSRAM**. Delay, reverb and grain buffers are the scarce resource.
  8 MB flash.
- **Audio:** ES8311 codec, **mono**, no MCLK (clocked from BCLK). Output goes to the 1 W speaker
  (via the NS4150B amp) and the 3.5 mm jack. **Inserting a plug mutes the speaker in hardware.**
  The amp-enable line doesn't appear among the ESP32 GPIOs on the schematic, so the firmware
  probably cannot choose speaker vs. jack by itself (verify on hardware; see §8, Out module).
- **Input:** the 56 keys are read through a TCA8418 over I2C (G8/G9). Keyboard, IMU, codec
  control and the expansion header all share that bus. How many keys can be held at once
  without ghosting is unverified (modifier + key = 2 keys must work).
- **Launcher:** bmorcelli's Launcher installs the `.bin` into an app slot of ≈4.9 MB and owns
  the partition table. **No custom flash partitions.** Large assets (wavetables, samples) load
  from SD or are compiled into the binary. **[owner: "loaded from SD is fine"]**
- **USB MIDI host (later):** the board cannot supply 5 V to a USB device, so it needs a powered
  USB-C OTG Y-adapter or hub. In host mode the USB serial console is lost.

## 4. Toolchain and repo

- **PlatformIO + Arduino core.** The owner has experience with it. The core sits on ESP-IDF, so
  IDF APIs (I2S, USB host) are still usable. **[owner]**
- Screen and keyboard via M5Unified / M5GFX / M5Cardputer. Check Cardputer-Adv support in the
  pinned versions. **[proposed]**
- DSP: **DaisySP** (MIT core) for v1 building blocks. **Plaits** (MIT) later. **[owner]**
  Do **not** pull in the DaisySP-LGPL modules (e.g. ReverbSc). **[proposed]**
- Git repo initialised on `main`, to be pushed to a private GitHub repo later by the owner. **[owner]**

## 5. Keyboard → screen mapping

The physical keyboard is 4 rows × 14 keys. The **middle 12 columns are the grid**. The
**outer column on each side holds the special keys.** **[owner]**

```
 left side                         12×4 grid                          right side
[esc/`]  1   2   3  4  5  6  7  8  9  0  -  =                       [del/⌫]
[tab  ]  q   w   e  r  t  y  u  i  o  p  [  ]                       [ \   ]
[fn   ]  Aa  a   s  d  f  g  h  j  k  l  ;  '                       [enter]
[ctrl ]  opt alt z  x  c  v  b  n  m  ,  .  /                       [space]
```

- The owner confirmed this layout. Key-cap labels differ slightly: the top-right key only reads
  "del" with fn, otherwise it's backspace. **Map by physical position, not legend.** **[owner]**
- Grid keys are read as raw positions. Shift/opt/alt inside the grid are ordinary cells. **[proposed]**
- Fixed roles **[owner]**:
  - `esc` backs out of the current view.
  - `enter` opens the selected module.
  - `del`/backspace deletes things. It is reserved for deleting.
  - `\` is **undo**.
- Every special key has an **on-screen icon** level with it, and **icons change with the
  current view**, so 8 keys can do more than 8 things. **[owner]**

## 6. Screen layout and look

Real screen: 240×135 px. See `docs/design/01-patch-view-style-b.png`.

**Layout [proposed, shown in mockup, accepted]**
- **Left strip:** 16 px wide, 4 icon slots, each level with `esc`/`tab`/`fn`/`ctrl`.
- **Right strip:** 16 px wide, slots for `del`/`\`/`enter`/`space`.
- **Middle 208 px:** a 12×4 grid with 1 px gaps. Cells are about 16×33 px.
  **Each on-screen row sits on its physical key row.**
- **Status:** the owner wants it small and preferably on the sides. It takes no row of its own.
  **[owner]**
  - CPU usage is a **2 px vertical bar on the far left edge**.
  - RAM usage is a 2 px bar on the far right edge.
  - The page indicator (`1/3`) sits in the `tab` icon slot.
- **Empty cells** show their key letter faintly (`q`, `w`, …), so you always know which key
  hits which cell.

**Look: style B "Pocket Operator LCD" is the default.** It matches the Cardputer's white
casing. **[owner]** The owner liked style A (OP-1 dark) as much ("I can see myself want both"),
so a **switchable dark theme A** is a nice-to-have. Colours must be palette tokens so a theme
is a swap. **[Claude's call]**

| Token | Style B (default) | Style A (dark theme) |
|---|---|---|
| background | `#c4c6b8` | `#0b0b0c` |
| side strips | `#b6b8aa` | `#101113` |
| text / icons | `#1d1f1a` | `#ece9e2` |
| dim text | `#5e6157` | `#7b7f85` |
| faint key letters | `#9a9d90` | `#3a3d42` |
| cell | transparent, 1 px dotted `#8f9286` border | `#16171a` |
| accent / selection / value fill | `#ff4d1a` | `#ff6a2b` (sel. outline white) |
| empty value segment | `#adb0a2` | `#2a2c30` |
| module category bar | all `#1d1f1a` (monochrome) | source `#ff6a2b`, modulator `#4c7dff`, processor/IO `#cfcac0` |

- **Cable colours**, used for dots in both styles, kept distinct from category colours:
  `#ffe14d` `#ff8fc8` `#39d0ff` `#ff3d9a` `#a77bff` `#b6ff3b`. More colours are
  needed if a patch has more cables than this. Reuse rules are **[open]**.
- **Type:** the mockup uses the pixel font *Silkscreen* (OFL), uppercase, ≈6–8 px. The device
  needs a matching bitmap font. Picking or converting one is **[open]**.
- **Icons:** 11–12 px line icons drawn in mockups:
  - modules: osc (sine), lfo (slow sine), vcf (low-pass curve with peak), vca (triangle),
    adsr (envelope), delay (decaying bars), out (speaker), keys (piano)
  - side keys: file, page, plus, move, trash, undo, enter, play, back, next, cable, minus,
    toggle, tilt
  Final icon art is **[open]**.
- **Animation is part of the look [owner: "visualise and stylise stuff to make it more fun"]:**
  - the LFO icon wobbles at its rate
  - the Out tile has a bouncing level meter
  - the VCF view shows the filter response curve moving with modulation
  - the ADSR view draws its envelope with a dot running along it while the gate is open
  - pins waiting for a carried cable pulse

## 7. Views and interaction

### 7.1 Patch view (zoomed out)

Screenshot: `docs/design/01-patch-view-style-b.png`.

- **One cell = one module.** A tile shows the category bar, the icon and a 3-letter name (OSC,
  VCF, ENV…). **[owner: two zoom levels]**
- **The patch is bigger than one screen.** It is made of grid pages, and `tab` moves to the next
  page. **[owner]** How many pages, and whether paging is only sideways, is **[open]**.
- **Select a module** by pressing its key without a modifier. This also shows you which cell
  that key is. **[owner]**
- **Cables as colour-matched dots.** When a module is selected, it and each module at the other
  end of its cables show a small dot in that cable's colour. No lines are drawn. **[owner]**
- **Placing a module:** hold an empty cell, or modifier + click, to open a module selection menu.
  **[owner]**
  - The mockup proposes `fn`+cell as the modifier. **[proposed]**
  - Hold-to-open timing **[open]**.
  - **The selection menu itself has not been designed.** It is **[open]** and should follow §6's look.
- **Side keys in patch view [proposed in mockup, not objected to]:**
  - `esc` file menu (save/load)
  - `tab` next page
  - `fn` place
  - `ctrl` move module (ctrl+module, then target cell)
  - `del` delete module
  - `\` undo
  - `enter` open module
  - `space` audition (hold to play a note)

### 7.2 Module view (zoomed in)

Screenshots: `03-module-view-vcf.png`, `04-module-view-adsr.png`. These were mocked in style
A only and are to be rendered in style B.

- `enter` on a selected module opens its view. `esc` returns to the patch view. **[owner]**
- **Every module has its own menu/view and may do things specific to it.** **[owner]**
  For example, the Keys module's view is where you map keys. MIDI and other special modules
  are configured in their views.
- **Top row (number keys `1`…`=`) = value strip.** **[proposed, owner liked it]**
  - 12 segments, filled up to the selected parameter's current value.
  - Parameter name on the left, value with unit on the right ("CUTOFF … 1.20kHz").
  - Pressing a key jumps the value to that coarse step.
- **Rows 2–4** hold the module's **parameters**, **inputs (in)**, **outputs (out)** and **live
  drawings** (the filter curve, the envelope). Drawings may span several cells.
  - A **parameter cell** shows a short name and its value.
  - A **pin cell** shows an `in`/`out` tag, its name, and a dot in the cable's colour per
    connected cable (a hollow ring if unpatched), plus the name of the module at the other end.
    An output with several cables shows several dots.
- **Editing a parameter [owner]:**
  1. Select the cell. Then either:
  2. Use the number row for **coarse** values.
  3. Use **two keys for fine** −/+. The mockup uses `ctrl` = −, `space` = + **[proposed]**.
  4. Or **hold the parameter's key and tilt the device** to sweep it.
  - Bounded parameters spread their range over the 12 coarse keys. For unbounded or log-scale
    parameters (frequency, time), the owner suggested **coarse = large jumps** instead
    ("maybe like coarse should be buttons with large jump idk"). The exact scheme is **[open]**.
- **Side keys in module view [proposed in mockup, not objected to]:**
  - `esc` back
  - `tab` next parameter
  - `fn` cable (pick up / drop)
  - `ctrl` fine −
  - `del` unpatch pin / reset parameter
  - `\` undo
  - `enter` toggle (e.g. LP/HP)
  - `space` fine +
  Space is no longer "audition" here. Whether audition is needed in module view is **[open]**.

### 7.3 Patching: "carry the cable"

Screenshot: `05-carry-cable.png`. **[owner chose this over split view; it was the owner's original idea]**

1. In a module view, `fn`+pin **picks up** a cable from that pin.
2. Navigate freely (`esc`, select another module, `enter`). The `fn` icon stays highlighted.
3. The top strip shows what you're holding, e.g. `● HOLDING LFO.OUT → FN+INPUT`, with a
   pulsing dot in the cable's colour. Valid target pins pulse.
4. `fn`+pin on the target **drops** it, and the cable is made.
5. Cancel: `esc` twice **[proposed]**, or `del`.

- Whether you can start from an input as well as an output, and whether cables within one
  module are allowed, is **[open]**.
- Rule: outputs **fan out** to many inputs. Several cables into one input **sum**. **[proposed, accepted]**

### 7.4 Output amount (attenuverter)

- Every **output** has an **amount** from −100% to +100%. Negative inverts the signal. The owner
  described it as "attenuation/phase/etc". **[owner: on every output]**
- **The amount merges into the output pin cell.** Selecting an output puts its amount on the value
  strip, edited like any parameter, and `fn` on the same pin still handles cables. **[Claude's
  call; owner: merged or separate are both fine]** The mockups still show a separate `AMT` cell
  next to the output. That is superseded.
- Whether "etc" includes an **offset** as well is **[open]**.

### 7.5 Undo / delete

- `\` = undo, at least for: placing, moving and deleting modules, adding and removing cables,
  and parameter changes. **[owner]** Redo is **[open]**.
- `del` deletes the selected module (with its cables) or unpatches the selected pin. Both can
  be undone. **[owner]**

## 8. Modules

### 8.1 First release (v1) — the owner's first patch **[owner]**

| Module | Inputs | Outputs | Parameters / menu |
|---|---|---|---|
| **KEYS** (virtual keyboard) | — | pitch (1 V/oct), gate, velocity | Its view maps keys to notes (octave etc.). Glide/portamento lives here **[owner suggested]**. MIDI→CV is built in, so no separate MIDI→CV module. **[proposed]** |
| **OSC** | pitch, FM | out | Wave: sine / square / triangle / saw. Tune. |
| **VCF** | in, cutoff CV, (res CV) | out | Cutoff, resonance, LP/HP switch. Drive in the mockup **[Claude's call]**. Live response-curve drawing. |
| **VCA** | in, CV | out | Level |
| **ENV** (ADSR) | gate (+ retrigger in the mockup) | out | A, D, S, R, drawn live in its view. Curve/loop shown in the mockup **[Claude's call]**. |
| **LFO** | (rate CV **[Claude's call]**) | out | Sine / square, rate |
| **DLY** (delay) | in, (time CV) | out | Time, feedback, mix. Max time limited by RAM. |
| **OUT** | in | — | **Separate volume for speaker and line** **[owner]**, limiter + DC blocker, level meter. **[proposed]** |

**OUT, speaker vs line:**
- Per the schematic, a plug in the 3.5 mm jack mutes the speaker in hardware. **No firmware
  switch between the outputs is needed.** **[owner]**
- Instead, OUT keeps **two volume settings, one for the speaker and one for the line out**,
  because the right level for the PO-33's line in differs from a comfortable speaker level. **[owner]**
- **Picking which volume applies:**
  - **Ideal:** automatic, from the headphone-detect signal (`HP_DET`). But on the schematic
    `HP_DET` only drives the amp-enable circuit and doesn't appear to reach an ESP32 GPIO, so
    the firmware may not be able to tell whether a plug is in. **[open — verify on hardware]**
  - **Fallback if it's not readable:** a manual SPK/LINE toggle in OUT's view that only picks
    which volume setting applies. The hardware still routes the audio. **[Claude's call]**

**Keys and playing [owner]:** the owner's model is "a virtual keyboard module where in its menu you
just map keys to virtual midi/oct/gate/etc out".
- **Opening the Keys module's view puts you in play mode.** The grid keys play notes per the
  mapping.
- Its own settings (octave, glide, the key mapping, pins) live on a second inner page, with a
  control to flip between the play page and the settings page. The owner called this "worst
  case" and acceptable.
- **The flip control doesn't have to be a side key.** A grid cell works too. It depends on the
  note layout:
  - If the play page uses **the whole grid** (e.g. a full chromatic layout), a side key is the
    better flip. The owner: it "generally work[s] better".
  - If the note layout leaves spare cells, a flip cell (or settings cells) can sit in the grid.
- The note layout (chromatic vs. other), and therefore where the flip lives, is **[open]**.
- The owner also accepted "audition" on `space` in patch view.

### 8.2 Later releases (keep the system ready for these) **[owner unless noted]**

- **Plaits** macro oscillator (a Grind-like set of engines).
- **USB MIDI** input module: same outputs as Keys. Needs a powered OTG adapter.
  BLE-MIDI / DIN-MIDI on Grove are alternatives. **[proposed]**
- **TILT** module: the BMI270 IMU as a patchable modulation source, separate from hold-to-tilt
  editing. **[owner]**
- **Utilities:** switch, mux/demux **[owner]**. Mixer, attenuverter/offset, sample & hold,
  clock/divider **[proposed]**.
- Sequencer, scope/tuner **[proposed]**. Theme A **[Claude's call]**.
  Rings/Braids-style modules if CPU allows **[proposed]**.

## 9. Signal model **[proposed, owner: "good"]**

- **Every signal is audio rate.** One type, so anything patches into anything.
- Processing in blocks of 16–32 samples at **48 kHz**. Signals are `float`.
- Voltage conventions like Eurorack, so Grind/Plaits instincts carry over:
  - **1 V/oct** pitch
  - audio and bipolar CV about ±5 V
  - gates high above a threshold
- **Feedback loops are allowed.** The cable that closes a loop adds one block of delay.
- **Inputs sum, outputs fan out**, and each output has an amount (§7.4).

## 10. Runtime architecture **[proposed/Claude's call]**

- **Core 1:** audio only. It runs the module graph per block and feeds I2S → ES8311.
- **Core 0:** UI, keyboard, display, SD, MIDI.
- Patch edits **must never click or crash the audio**:
  - the UI builds the changed graph off the audio thread
  - the audio thread swaps it in at a block boundary
  - parameter changes are smoothed
- **Never block on I2C from the audio thread.** The I2C bus is shared with the keyboard.
- **Resource budget:**
  - each module type declares an approximate CPU cost and RAM need
  - CPU and RAM show in the status bars
  - **adding a module that won't fit is refused** with a clear message
- **No allocation on the audio thread.** Delay buffers come from a fixed pool.

## 11. Patch storage **[owner: SD; format details proposed]**

- Folder `/modsynth/` on SD, with patches in `/modsynth/patches/`, kept apart from other
  firmwares' files.
- Text format (JSON or INI, **[open]**) with a **format version** and **stable module-type IDs**,
  so future firmware can still load old patches.
- Stores: modules (type, page, cell, parameters), cables (from module.pin → to module.pin,
  colour), output amounts.
- **The current patch is autosaved.** The file menu sits on `esc` in patch view.
- Assets (wavetables later) live on SD under `/modsynth/`.

## 12. Left to the builder / open items

1. Module selection menu design (look and navigation). Must match §6.
2. Patch size: number of pages, paging direction.
3. Keys note layout (chromatic or other), which decides whether its play/settings flip is a side key or a grid cell (§8.1). Whether module view needs an audition key.
4. Coarse-step scheme for log/unbounded parameters. Fine-step sizes.
5. Cable colour reuse when there are more than 6 cables. Whether patching can start from an input.
6. Output offset in addition to amount.
7. Whether the firmware can read headphone-detect, to switch automatically between the speaker and line volume settings (§8.1).
8. Bitmap font and final icon art.
9. Patch file format (JSON vs INI). Redo.
10. Hold-to-place timing. Ghosting limits of the key matrix.

## 13. Decision log (session of 2026-10-27)

| Topic | Decision | Whose |
|---|---|---|
| DSP libraries | DaisySP now, Plaits later | owner |
| Delivery | `.bin` via bmorcelli's Launcher | owner |
| Grid | middle 12×4 keys = grid; 2 outer columns = special keys with icons | owner |
| Zoom | patch view (1 cell = 1 module) + module view | proposed → owner agreed |
| Canvas | larger than one screen | owner |
| Cables | colour-matched dots, no lines | owner |
| Signal model | all audio rate, ±5 V / 1 V/oct, summing inputs, feedback allowed | proposed → owner agreed |
| Editing | number row coarse, two keys fine, hold+tilt | owner (extending a proposal) |
| Tilt | also its own patchable module | owner |
| Keyboard play | a Keys module mapping keys to pitch/gate/etc.; its open view plays notes, with a flip (side key if the grid is all notes, else a grid cell) to an inner settings page | owner |
| Per-module menus | each module's view can do its own things | owner |
| Undo / delete | `\` undo, `del` delete | owner |
| Status | small, on the sides | owner |
| Output | limiter/meter; no output switch (hardware does it on plug-in); separate speaker and line volume | proposed + owner |
| Look | style B default; A as optional theme | owner (theme: Claude's call) |
| Value strip on top row | yes | proposed → owner liked it |
| Output amount | on every output, merged into the pin cell | owner + Claude's call |
| Cross-module patching | carry the cable | owner |
| Toolchain | PlatformIO + Arduino core | owner |
| v1 scope | starter-patch modules + Keys + patching + SD + speaker/line volume settings; rest later, keep expandable | proposed → owner agreed |
