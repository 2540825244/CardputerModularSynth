# Reference documents

Offline copies of the official documents behind `docs/hardware-spec.md`. Downloaded 2026-10-27.

The PDFs are third-party documents, so they are git-ignored and not in the repo. Download them
from the Source links below into this folder.

## Cardputer-Adv
| File | What | Source |
|---|---|---|
| `Sch_M5CardputerAdv_v1.0.pdf` | Cardputer-Adv schematic (4 pages): power/charger/boost, keyboard (TCA8418), IMU + ES8311 audio + amp + jack, EXT header | [M5Stack](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1178/Sch_M5CardputerAdv_v1.0_2025_06_20_17_19_58.pdf) |
| `Sch_StampS3_v0.3.3.pdf` | Stamp-S3 module schematic, linked from the Cardputer-Adv page as the "Stamp-S3A" schematic. It shows the USB-C VBUS → fuse → 5 V path (no host power). | [M5Stack](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1150/Sch_StampS3_v0.3.3.pdf) |
| `K132-Adv_dimensions.pdf` | Mechanical size drawing | [M5Stack](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1178/K132-Adv-cardputer-ADV.pdf) |
| `ESP32-S3_datasheet.pdf` | ESP32-S3 chip datasheet | [Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) |
| `TCA8418_datasheet.pdf` | Keyboard matrix scanner (key event FIFO, ghosting behaviour) | [TI](https://www.ti.com/lit/ds/symlink/tca8418.pdf) |

## Cap LoRa-1262
| File | What | Source |
|---|---|---|
| `Sch_Cap-LoRa1262_v1.1.pdf` | Cap schematic | [M5Stack](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1208/U214-sche-Cap-LoRa1262_SCH_V1.1_20251029_2025_11_07_22_53_19.pdf) |
| `Cap-LoRa1262_dimensions.pdf` | Size drawing | [M5Stack](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1208/U214-00-cap1262-model-size.pdf) |
| `SX1261_2_datasheet.pdf` | SX1262 LoRa transceiver | [M5Stack mirror](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1177/DS_SX1261_2_V2-2.pdf) |
| `ATGM336H-6N_datasheet.pdf` | GNSS module | [M5Stack mirror](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/unit/Unit-GPS%20v1.1/ATGM336H-6N.pdf) |

## Not stored here (link only)
- **ES8311 codec datasheet.** The official Everest link (`everest-semi.com/pdf/ES8311 PB.pdf`)
  returned 404, and the copies elsewhere are on unofficial mirrors. For register setup, use
  Espressif's maintained driver in [esp_codec_dev](https://components.espressif.com/components/espressif/esp_codec_dev)
  (the standalone `es8311` component is deprecated). Also check how M5Unified initialises the
  ES8311 on the Cardputer-Adv.
- **Cardputer-Adv product page PDF** (13 MB, a printout of the docs page):
  [link](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/pdf/static/en/core/Cardputer-Adv.pdf).
  The pin tables from it are already in `docs/hardware-spec.md`.
- **ESP32-S3 Technical Reference Manual** (very large):
  [Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf).
- Docs pages: [Cardputer-Adv](https://docs.m5stack.com/en/core/Cardputer-Adv) ·
  [Cap LoRa-1262](https://docs.m5stack.com/en/cap/Cap_LoRa-1262).
