# Third-party notices

The LICENSE file covers this project's own code. These components keep their own licenses.

## Vendored in `lib/`
| Component | License | Copyright | Full text |
|---|---|---|---|
| DaisySP (MIT core only) | MIT | (c) 2020 Electrosmith, Corp. | `lib/DaisySP/LICENSE` |
| Plaits (from Mutable Instruments eurorack) | MIT | (c) 2016 Emilie Gillet | `lib/plaits/LICENSE` |

## Fetched at build time (not in the repo, linked into the firmware)
| Component | License |
|---|---|
| M5Unified, M5GFX, M5Cardputer (M5Stack) | MIT (M5GFX's bundled fonts carry their own licenses) |
| Arduino core for ESP32 / ESP-IDF (Espressif) | LGPL-2.1 / Apache-2.0 |

Native tests also use ArduinoJson (MIT), which is not in the firmware.

## Reference documents
The datasheets and schematics listed in `docs/reference/README.md` belong to their publishers
and are not in this repository.
