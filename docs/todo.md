# Changes to do

The owner's queue of agreed changes not built yet. Newest at the bottom of each section. Move an item
to the spec (and delete it here) when it ships.

## Agreed, not built
- **Screen sleep** (owner, 2026-10-28): no key for 2 min -> backlight to the lowest level; 5 min ->
  backlight off and no redraws. Audio keeps running (drones, sequencers, latched notes; no deep sleep).
  The first key press only wakes, it does nothing else. SYSTEM page setting: off / 1 / 2 / 5 / 10 min.

## Waiting on the owner's device report (test-5 script)
- Real `cpuCycles` per type from the serial `cost id=measured/estimate` lines (GRD = its worst engine).
- Confirm a forced CPU overload (> 100 % for 15 s) distorts but does not reboot.
- Confirm the boot heap (`[heap init]`, `[heap steady]`) and that GRD fits next to the test-1 patch.

## Deferred (known, not urgent)
- Save without a JSON document: write the patch straight to the SD file. Removes the ~25 KB a full
  patch save needs and the 32 KB create reserve that covers it (review 5b).
- canAddLive charges a second DLY/REV 96 KB even when the first is missing (never created, static
  slot free). Conservative, rare (review 5b M3).
- USB MIDI host.
- Task 14: final whole-branch review, then finish `feat/v1-firmware`.
