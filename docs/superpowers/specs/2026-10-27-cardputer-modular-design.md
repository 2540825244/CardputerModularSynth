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
  - **One "do it" gesture [owner, 2026-10-28]:** on every cell, `fn`+tap, holding the key, and
    tapping it then pressing `enter` all do the same thing (open, place, carry a cable, step an
    option, load/save). A plain tap only selects. Holding `enter` is a sweep (hold-and-tilt).
    Exceptions: the Keys play page (keys play notes) and the module view's number row (a tap sets
    the coarse value). Gestures are mapped in one place (`src/ui/gesture.h`) so a later UI
    redesign can remap them without touching the views. **[Claude's call: the mechanism]**
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
- **Placing a module:** do-it on an empty cell (hold it, `fn`+tap it, or select it and `enter`) opens a module selection menu.
  **[owner]**
  - The mockup proposes `fn`+cell as the modifier. **[proposed]**
  - Hold-to-open timing **[open]**.
  - **Selection menu [owner, plan review]:** pressing a module type in the menu doesn't place it yet. It opens
    that type's details (what it does, its pins, its cost), and `enter` places it. Do-it on a type
    (`fn`+tap or hold) places it directly. **[owner, 2026-10-28]**
- **Side keys in patch view [proposed in mockup, not objected to]:**
  - `esc` file menu (save/load)
  - `tab` next page, `fn`+`tab` previous page **[owner, plan review]**
  - `fn` place
  - `ctrl` move module (ctrl+module, then target cell)
  - `del` delete module; `fn`+`del` bypass / enable it (v1.3 **[owner]**: the soft delete; the
    module keeps its instance and cables, is not processed and not counted by the CPU guard;
    effects and processors with audio in 0 / out 0 pass it through, others output 0; the tile
    draws dim with a slash, the module view says BYPASS; undoable, saved as `"bypass":true`;
    enabling is not CPU-gated, CPU FULL warns)
  - `\` undo
  - `enter` do-it on the selection (open module / place menu)
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
  2. Use the number row for **coarse** values: `1`…`9` = 10–90% of the knob, `0` = 100%, `-` = min,
     `=` = default. For frequency and time the % is along the knob's log curve. **[owner, plan review]**
  3. Use **two keys for fine** −/+. The mockup uses `ctrl` = −, `space` = + **[proposed]**.
  4. Or **select the parameter, hold `enter` and tilt the device** to sweep it; release `enter`
     to stop. **[owner, 2026-10-28; was "hold the parameter's key"]**
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
  - `enter` do-it on the selection: carry a cable on a pin, next option on a stepped param
    (e.g. LP/HP); hold `enter` on a knob to tilt-sweep it
  - `space` fine +
  Space is no longer "audition" here. Whether audition is needed in module view is **[open]**.

### 7.3 Patching: "carry the cable"

Screenshot: `05-carry-cable.png`. **[owner chose this over split view; it was the owner's original idea]**

1. In a module view, do-it on a pin (`fn`+pin, hold the pin, or select it and `enter`) **picks up** a cable from that pin.
2. Navigate freely (`esc`, select another module, `enter`). The `fn` icon stays highlighted.
3. The top strip shows what you're holding, e.g. `● HOLDING LFO.OUT → FN+INPUT`, with a
   pulsing dot in the cable's colour. Valid target pins pulse.
4. Do-it on the target pin **drops** it, and the cable is made.
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
  v1: the play page's top-row cell `=` flips to settings; `esc` on the settings page flips back
  (2026-10-28; it was `enter`, which is now do-it). **[Claude's call]**
- The owner also accepted "audition" on `space` in patch view.

### 8.2 Later releases (keep the system ready for these) **[owner unless noted]**

- **Plaits** macro oscillator (a Grind-like set of engines).
- **USB MIDI** input module: same outputs as Keys. Needs a powered OTG adapter.
  BLE-MIDI / DIN-MIDI on Grove are alternatives. **[proposed]**
- **TILT** module: the BMI270 IMU as a patchable modulation source, separate from hold-to-tilt
  editing. **[owner]**
- **Utilities:** switch, mux/demux **[owner]**. Mixer, attenuverter/offset, sample & hold,
  clock/divider **[proposed]**.
- More note layouts for the Keys module (or other layout modules), beyond the v1 three-octave
  chromatic grid. **[owner, plan review]**
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
- **No allocation on the audio thread.** Delay buffers come from a pool (one static slot, one heap
  slot taken on the UI thread when a second DLY/REV is placed).
- **KIT, demand-driven activation** (v1.3 **[owner: "demand driven module activation"]**, details
  **[Claude's call]**): the KIT module (MOD page, max 1) plays a drum kit into the PO-33 in one take:
  8 trigger lanes (T1..T8) plus an ACC lane over up to 12 steps (one per key column; stacked hits =
  several lanes on a step), a step every GAP (150-1500 ms) or each CLK rise, one pass or a loop.
  ACC out = ACC x 5 V during accented steps. A pass starts on a RUN rise, enter on a non-editable
  cell of the KIT view, or PLAY in the patch view when there is no KEYS (KEYS keeps PLAY).
  - A module whose every connected input traces back to one lane (the KIT's T-out, or modules
    already in that lane) is in that lane's group; ACC, self-cables and sources without an instance
    don't count. A source must be struck by the lane (its trig/gate input). A module whose output
    leaves the group must be audio (source, processor, effect: a held CV would drop). KIT, OUT and
    DLY/REV (echoes outlive the quiet window) never join; cycles stay always-on.
  - A group switched off forgets its ringing (drum and physical voices re-init), so every hit of
    a take starts the same. An edit that pushes the worst step past the budget toasts CPU FULL.
  - A group runs from its trigger until its outputs leaving the group stay under 5 mV (-60 dB of
    5 V) for 50 ms, or the next step starts (then it fades out over 2.7 ms). Idle groups output 0
    and cost 0.
  - CPU guard: always-on modules + the costliest set of groups one step fires; the live check adds
    that worst step to the measured load.
- **Screen sleep and redraws** (v1.3 **[owner]**, details **[Claude's call]**): no key for the SYSTEM
  sleep time (OFF / 1 / 2 / 5 / 10 min, default 2) dims the backlight to its lowest; 3 min later it
  goes off and nothing is drawn. Audio keeps running. The first key only wakes (swallowed down to
  its Up). Frames are drawn only when what they show changes: a key, the patch, the CPU/RAM bars,
  the meters of modules in view, a toast, a blink. After a UI change they run at 30 fps for 300 ms;
  meters and bars alone redraw at most every 100 ms (not while dim); at least once a second.

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
| Gestures (2026-10-28) | `fn`+tap = hold = tap then `enter` = the cell's one do-it action, in every view; tap selects; exceptions: Keys play page, number row | owner (exceptions: Claude's guess, owner agreed) |
| Tilt sweep (2026-10-28) | select a param, hold `enter`, tilt; was hold the param key | owner |
| File menu (2026-10-28) | tap selects a slot, do-it loads/saves it | Claude's guess, owner agreed |
| Keys settings → play (2026-10-28) | `esc` (enter is do-it now) | Claude's call |
| Open items (§12) | decided in the v1 plan's table; owner changes: place menu shows details then `enter` places; `fn`+`tab` = previous page; coarse = "*k*0% of the knob", `-` min, `=` default; more Keys layouts later | owner (plan review) |

## 14. v1.2 changes (session of 2026-10-28, plan `docs/superpowers/plans/2026-10-28-v1.2-feature-complete.md`)

The owner asked for "feature complete": more modules everywhere, unified paged menus, pages inside
modules, a readable place menu, factory patches, a Grind-like macro module, a responsive animated UI,
context icons, more ways to move, a visible carried cable that `fn` drops, an attenuverter on every
input, and more keyboard layouts that switch without rewiring. **[owner]** How each is met is
**[Claude's call]** unless marked:

- **Pages everywhere:** `tab` = next page, `fn`+`tab` = previous, in the patch view (3 pages), place
  menu (5 category pages), module view (a type's pages) and file menu (LOAD, SAVE, PRESETS, SYSTEM).
  The tab slot shows `n/N`. In the module view `tab` used to be "next parameter". **[owner: tab flips module pages]**
- **Module pages:** automatic when params/ins/outs overflow a row, or declared per type (`PageDef`).
  A page is a cell page or a Play page (KEYS). Sequencer steps are `Bit` cells, toggled by a plain tap.
- **KEYS:** pages PLAY and SET (`tab` flips; `esc` leaves from either). Six layouts using all 48 keys
  (chromatic, scale, piano, bass fret, fourths, drum pads) as a LAYOUT param, so switching never
  rewires. **[owner: bass fret, scale, keyboard mimic; full grid; easy switching]** Play page: `ctrl`/`space`
  octave, `enter`/`fn`+`enter` layout, `del` panic, `\` latch.
- **Moving:** `ctrl`+cell (v1), hold a module and press another cell **[owner]**, or select then tap
  `ctrl` alone **[owner]**. Onto a module = swap; `fn`+target during a move = copy.
- **Cables:** in the patch view a "HOLDING" strip shows the carry and modules that can take it pulse
  **[owner: indication]**; a lone `fn` tap drops it **[owner]**, as does one `esc` in the patch view.
  Hold a pin and press a pin of the other kind = patch directly.
- **Input attenuverters:** every input has an amount −100…+100 % (default +100 %), applied after its
  cables sum, saved as `inamounts`. **[owner]**
- **Side strip:** one function (`src/ui/side_icons.*`) decides every icon from the state (esc = ✕
  during a move or carry **[owner]**); held keys draw inverted and flash on a tap **[owner: respond to
  presses]**; holding `fn` alone shows a help panel. `fn`+`\` = redo.
- **Look:** zoom transitions, key flashes, eased value strip, value bars, activity meters, sliding
  toasts; theme A selectable on the SYSTEM page with brightness and hold time.
- **Files:** 36 slots, NEW patch, 12 factory presets; first boot loads BASIC. **[owner: default patches]**
- **Modules:** about 45 new types (effects, processors, sources, drums, modulators, sequencers,
  utilities, scope/tuner) and **GRD**, Plaits' 24 engines (original 16 first, as on the Grind).
  **[owner: Grind macro]** DLY and REV share the two 96 KB buffers.
- **Deferred:** USB MIDI host (needs the owner's powered OTG adapter to verify, and host mode drops
  the USB serial console).

### 14.1 After review round 2 and the owner's first v1.2 run (2026-10-28)
- **GRD is a whole Grind voice** **[owner: "grind is supposed to be an all in one with filters and
  stuff"]**: Plaits -> 24 dB ladder (LP / HP / OFF, CUT, RES, FEG = ADS -> cutoff) -> VCA (ENV = the
  ADS, LPG = Plaits' own low-pass gate, DRN = open), an ADS whose release is the decay (as on the
  Grind), and a triangle LFO (rate, -> cutoff, -> timbre). New CUT input; ENV and LFO outputs; pages
  MAIN / VOICE / MOD. With TRG unpatched the VCA is open, so a fresh GRD sounds. AUX stays raw Plaits.
  The Grind's sequencer/arp is not in GRD: SEQ, TUR and KEYS cover it. **[Claude's call on the details]**
- Patched CV on GRD's FM/TIM/MOR goes in at amount 1 (the input's attenuverter scales it); FMA/TMA/MMA
  are the internal-envelope amounts. Plaits renders in its own 12-sample steps, so DEC times match.
- Every module now has room for 22 params and 9 inputs (Patch ~5.3 KB, static RAM 275 of 327 KB).
- **CPU guard** **[owner report: the bar topped off and audio distorted on adding GRD]**: the audio
  thread measures each module's cycles; the cost of a type is its measured value + 1/8 once it has
  run (GRD: never below its estimate, the engine changes live), else its estimate. Adding to the
  running patch also needs measured load + cost <= budget, and a free heap block for the biggest
  allocation. The serial log prints `cost id=measured/estimate` when a measurement rises.
- A module the patch holds but could not create toasts "NO RAM" (retried every ~130 ms).
- Toasts: "CPU FULL" when the measured load stays above 95 % for 0.5 s; "NO RAM" when a save cannot get
  the heap (nothing is written: a truncated JSON would replace a good file); "SKIPPED n TYP" names the
  type a load left out for budget. GRD takes two heap blocks (16 KB Plaits buffer + ~11 KB object):
  two fit a fragmented heap more often than one of ~28 KB.
- **RAM (owner report, v1.2-test2: GRD "NO RAM" on an empty patch, no room after the test-1 patch,
  a load dropped DLY)**: static RAM was 275.5 KB, leaving ~10-20 KB of heap under the 24 KB reserve.
  Owner: "smaller undo / remove undo / quick save onto SD"; "if memory is limited, the main goal
  should be making fun generation possible". Now 153.8 KB static:
  - Undo keeps compact snapshots (only the modules in use, each with its own type's params and
    pins) in one 16 KB arena, up to 16 steps (~3 on a full 32-module patch); the oldest go first.
    Was 8 full 5.3 KB copies (42 KB). **[owner: smaller undo; size Claude's call]**
  - One delay slot (1 s, 96 KB) is static; the second DLY/REV takes its 96 KB from the heap while
    placed (canAdd counts it, canAddLive needs the 96 KB block). REV's plate needs 45,945 samples,
    so slots stay 1 s. **[Claude's call, from the owner's "fun generation" priority]**
  - A load never leaves a module out for RAM or CPU (a patch built against the live load must
    reload whole; a reduced patch would be autosaved over the good one): one without heap stays in
    the patch, greyed out and silent ("NO RAM"), and is created once there is room; an overloaded
    patch says "CPU FULL". Only pool slots and MAX 1 skip.
    Every create keeps kRamReserve free; a DLY/REV replacing a retired one waits a collect() for
    the static slot instead of taking the heap slot.
  - Adding to the running patch judges CPU by the measured load + the new type's cost only (the
    per-type sum over-counts: each type's worst moment). **[owner: "quite bounded by CPU"]**
- **Force: fn+enter** **[owner: "modifier enter to bypass", "even when loading"]**: in the place menu
  it places past NO CPU / NO RAM (toast "FORCED"); the enter icon reads FORCE (lit) while fn is held.
  MAX 1 and NO BUFFER still hold. Creates always keep kRamReserve (32 KB: a full patch's save), so a
  forced RAM-short module waits greyed out. Loads need no force (they never skip for budget).
- Autosave out of heap toasts "NOT SAVED: NO RAM" again every 30 s.
- fn shows an icon in every view: PLACE in the place menu, LOAD/SAVE/PRESET in the file menu,
  a shift arrow (PREV) where fn only reverses tab/enter (KEYS play page, SYSTEM page). **[owner report]**
- KEYS: a latched note keeps sounding when you leave the Play page (space audition is off while latched). CHROMATIC starts at C3 again
  (v1.1's bottom row), so old patches keep their octave.

## 15. v1.4: pictures for every module, 5 new modules, 14 presets (session 8, 2026-10-29)
Plan `docs/superpowers/plans/2026-10-29-v1.4-pictures-modules-presets.md` (its "calls made" table
lists Claude's calls). Owner: "revamp all the modules to be more graphical if the hardware budget
allows ... something fresh while keeping the current well established interaction logic"; mockups
approved ("good to go"); "more modules and presets as you see fit".
- **Pictures** (`ui/pictures*.{h,cpp}`, `ui/pic_*.cpp`): every module with automatic pages, plus
  GRD (TONE / VOICE / MOD), KEYS SET and SEQ SET, draws a picture over cols 5..11 of its page.
  Painters draw through `ui/gfx.h` (the M5Canvas on the device, a framebuffer in test_pictures that
  catches out-of-box draws and writes `.pio/pictures/*.png` and a contact sheet `_sheet.png`).
  Static pictures cost nothing until a param changes; live ones (LFO phase, ENV/AD/LPG level, EUC
  / ARP step, TUR register, MUX/DMX choice, TLT, OUT meter, DYN gain reduction, VCA gain, CHS
  point, OSC Hz, KEYS held note, CLK beat) redraw through the RedrawGate (<= 10 fps, on change).
  The selected param's part of a picture is the accent; ENV / VCF / AD draw a dashed ghost of the
  defaults. VSO FMO FOR VOS ZOS are the real DaisySP oscillators run for two cycles; FLD / OVD the
  real shapers; EQ3 its own biquads; drums and physical models are sketches of their knobs (too
  heavy to run). **[owner: the direction; every picture's content Claude's call]**
- **Layout**: on a picture page, params past row 1 continue on row 3 after the outs, so most
  modules keep every knob beside the picture. GRD's pages are TONE (ENG, its 3 knobs, OCT; TUN and
  the mod amounts; VCA DEC COL), PATCH (pins), VOICE, MOD. KEYS SET: LAY SCL ROT OCT GLD / VEL LEG
  LAT / outs.
- **GRD's knobs are named per engine** in cells, strip and picture (FM: RAT IDX FBK ...), from the
  Plaits manual and checked against `lib/plaits/.../engine*/` (the owner knows the Grind: a wrong
  name is one table line in `ui/pictures.cpp`).
- **Whole UI**: the value strip fills a bipolar knob from its zero (zero marked), ticks the default;
  the place menu shows the type's picture at its defaults; patch tiles of OSC VSO LFO VCF LAD ENV AD
  EUC draw a glyph that follows their params; SEQ TRG EUC KIT tiles show the step on their category
  bar, CLK lights it on the beat; SEQ step cells draw their pitch as a bar; steps past LEN hatch.
- **New modules** (`core/modules/v14.cpp`): ARP (7 chords, UP / DN / U/D / RND, 1-3 octaves, CLK or
  its RATE, GATE holds, a new gate starts at the root), CLP (handclap), PRC (TOM CNG CB CLV RIM),
  VOW (A E I O U formants, SHF, RES, MIX), EQ3 (low shelf 200 Hz, mid peak, high shelf 4 kHz, +-15 dB).
  cpuCycles are estimates until the device bench.
- **Presets**: a second PRESETS page (file menu: LOAD SAVE PRESETS PRESETS-2 SYSTEM); 14 presets
  on it for the new modules, at -3 dBFS from the bench.
- Found, not changed: DaisySP's Fm2 adds the modulator into the carrier's phase for good (an FM,
  not a PM), so FMO's index is very strong (~40 cycles of deviation at 30 %); FMO's picture shows it.

## 16. v1.5: piano roll, evolving line, transport (session 9, 2026-10-31)
Plan `docs/superpowers/plans/2026-10-31-v1.5-sequencers.md` (its "calls made" table lists Claude's
calls). Owner: "a more intuitive piano roll one that also has a built in clock and a button to switch
on/off as well as hooked to the global play button and can do notes of different length (and 16th
notes etc)? And maybe one that can organically evolve the line. Like the current sequencer you can't
even change the root note."
- **Transport** (`core/transport.*`): a global play / stop. Space in the patch view plays a stopped
  transport, and stops it when the patch has a ROL or EVO (else Space keeps auditioning KEYS / running
  KIT). fn+enter on the ROLL page does the same. Stopped: CLK, ROL and EVO send no clock or gates and
  SEQ TRG EUC TUR ignore clocks; a start puts them all back on step 1 together. Loads and NEW play.
  The side icon reads PLAY / STOP.
- **SEQ** gets ROOT and OCT on its SET page: the steps are semitones above ROOT, snapped to SCL.
- **ROL** (piano roll, MOD page): 32 steps, one mono note per step plus ties, so a note lasts 1..32
  steps. RES 1/4 1/8 1/16 1/32 8T 16T; its own clock (BPM, SWG) or a step per CLK rise; RUN mutes the
  gates but keeps time; ROOT / SCL / OCT transpose the stored notes (stored as semitones above ROOT,
  snapped to SCL as they play); GLN (of a note's last step), GLD glide. Ins CLK RST TRS (1 V/oct
  added), outs PIT GAT CLK (the step clock, for TRG / EUC / EVO) EOC. The notes are 11 params of three
  7-bit step codes, so save, load, undo and presets need nothing new. Pages ROLL (play), SET, PINS.
- **ROLL page** (raw keys, like KEYS' PLAY): the number row picks the note length (1/16 ... 1 bar at
  1/16, shown as fractions of the RES and as bars), REST, TIE (the note before lasts longer), OCT-/+
  (the keyboard). Rows 2-4 are the scale from ROOT (the middle row starts on ROOT, the bottom an
  octave lower): a key writes a note of the chosen length at the cursor, plays it while held, and
  moves the cursor past it. fn+key only plays; fn+length resizes the note under the cursor.
  ctrl / space move the cursor a step (fn: a beat); del erases the note under it (fn: clears the
  pattern); enter = RUN; `\` undo. The page draws the roll (beats, bars, ROOT rows named), the cursor
  as wide as the chosen length, the playing note in the accent, and a status line.
- **EVO** (evolving line, MOD page): a 2..16 step line grown from SEED as a random walk in the scale
  (ROOT, SCL, RNG degrees, DNS notes vs rests, ties, accents). Every pass end each step mutates with
  MUT (+ the MUT input, 5 V = 100 %): the pitch walks a degree or two (GRV pulls it home), a note comes
  or goes, a tie flips, the step copies the one half a pass away (motifs echo), or the accent flips.
  A transport start replays the seed (each take the same); RST only rewinds. MUT 0 freezes it. Outs
  PIT GAT ACC CLK. Its picture draws the line now, the playing step and the generation.
- **Presets** (PRESETS 2): ROLL (a 2-bar ROL bass with kick / clap from its CLK out), EVOLVE (EVO
  into GRD's modal engine), DUET (ROL bass + an EVO lead on the ROL's clock).
