# Changes to do

The owner's queue of agreed changes not built yet. Newest at the bottom of each section. Move an item
to the spec (and delete it here) when it ships.

## Agreed, not built
- **Screen sleep** (owner, 2026-10-28): no key for 2 min -> backlight to the lowest level; 5 min ->
  backlight off and no redraws. Audio keeps running (drones, sequencers, latched notes; no deep sleep).
  The first key press only wakes, it does nothing else. SYSTEM page setting: off / 1 / 2 / 5 / 10 min.

## CPU (bench on the device, 2026-10-29: `pio run -e bench`, flashed into the modsyn slot)
Done: per-sample helpers inline, fast Ladder (960 -> 585 cyc), all 61 cpuCycles measured (UI 30 fps).
Real costs put 5 presets over budget: DRUMS 23750, TURING 12950, GRIND 5500, PLUCK 5060, DRONE 5050.
- DaisySP drums / physical models recompute coefficients every sample (snare: 2 powf + 6 SVF
  SetFreq/SetRes with sinf/powf per sample). Cache them, recompute only when a param changes or per
  block: SNR 10738, MDL 9694, HAT 6870, RES 6301, KCK 3297, STR 2829, AWH 2664, VOS 2139, ZOS 1580.
- Engine: sanitize + peak per output sample with integer compares (float compares ~40 cyc/sample).
- Ladder at 2x oversampling as an option (~290 cycles less in GRD/LAD; slightly more aliasing).
- UI redraw costs the audio ~16-19 % (shared 16 KB instruction cache): IRAM for the engine and
  hot modules, or a lower frame rate when nothing changes.
- Plaits divisions are ROM `__divsf3` calls (~60 cyc): a fast exact FPU division would cut them.

## Device
- The firmware is 995 KB of the 1 MB modsyn slot (53 KB left).
- SD card fails to mount at boot (`f_mount failed (3)`) on 2026-10-29: card missing, or the Launcher
  leaves the bus in a bad state?

## Waiting on the owner
- Confirm a forced CPU overload (> 100 % for 15 s) distorts but does not reboot.

## Deferred (known, not urgent)
- Save without a JSON document: write the patch straight to the SD file. Removes the ~25 KB a full
  patch save needs and the 32 KB create reserve that covers it (review 5b).
- canAddLive charges a second DLY/REV 96 KB even when the first is missing (never created, static
  slot free). Conservative, rare (review 5b M3).
- CPU FULL on edit (KIT patches) is not said after undo/redo or a load that lands over the
  estimate; only edits that raise it warn (review of the GAP fix, 2026-10-29). Old behaviour too.
- USB MIDI host.
- Task 14: final whole-branch review, then finish `feat/v1-firmware`.
