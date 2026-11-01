# Third-party notices

The LICENSE file covers this project's own code. These components keep their own licenses.

## Vendored in `lib/`
| Component | License | Copyright | Full text |
|---|---|---|---|
| DaisySP (MIT core only) | MIT | (c) 2020 Electrosmith, Corp. | `lib/DaisySP/LICENSE` |
| Plaits and stmlib (from Mutable Instruments eurorack) | MIT | (c) Emilie Gillet | `lib/plaits/LICENSE`, `lib/plaits/stmlib/LICENSE` |

`src/core/ladder.h` is a rewrite of DaisySP's ladder filter (MIT, (c) 2021 Richard van Hoesel,
(c) 2024 Infrasonic Audio LLC) and keeps its notice. `test/test_plaits_fast/ref_plaits.h` holds
unmodified Plaits/stmlib classes for tests.

## Fetched at build time (not in the repo, linked into the firmware)
| Component | License |
|---|---|
| M5Unified, M5GFX, M5Cardputer (M5Stack) | MIT (M5GFX's bundled fonts carry their own licenses) |
| ArduinoJson (Benoit Blanchon) | MIT |
| Arduino core for ESP32 / ESP-IDF (Espressif) | LGPL-2.1 / Apache-2.0 |

## Reference documents
The datasheets and schematics listed in `docs/reference/README.md` belong to their publishers
and are not in this repository.
