# Hardware Spec — M5Stack Cardputer-Adv (K132-Adv) + Cap LoRa-1262

Collected 2026-10-27 from the official docs page and the official schematics
(`Sch_M5CardputerAdv_v1.0_2025_06_20`, `Sch_StampS3_v0.3.3`). Items marked
**(verify)** are inferred or come from conflicting sources. Check them on the
real device.

## 1. Compute & memory

| Item | Value | Synth implication |
|---|---|---|
| Module | Stamp-S3A | |
| SoC | ESP32-S3FN8, 2× Xtensa LX7 @ 240 MHz | One core for audio, one for UI/MIDI |
| FPU | Single-precision float only (double runs in software, division is slow) | Use `float`, multiply by reciprocals, no `double` |
| SIMD | PIE 128-bit integer vector ops (esp-dsp uses them) | Useful for int16 mixing/FIR; not for float DSP |
| SRAM | 512 KB total; probe: 325,820 B free, 294,900 B largest block (252,432 / 233,460 with a 65 KB sprite + 8 KB stack) | **Tight**: limits delay/reverb/granular buffers |
| PSRAM | **None** ("N8" = 8 MB flash, no PSRAM) | No large sample RAM; no multi-second buffers |
| Flash | 8 MB | ~3 MB app + ~4 MB for wavetables/samples (memory-mapped, read-only) |
| microSD | SPI: CS G12, MOSI G14, CLK G40, MISO G39 | SD confirmed working (probe: SDHC, 59,818 MB). Hold G5 high first (see §6). Mounted, FATFS takes ~15 KB of heap in one block with `max_files` = 2 (~4.6 KB per allowed open file; the default 5 took ~28 KB). Patch storage, sample streaming (mono 48k/16-bit = 96 KB/s, easy) |

CPU budget at 48 kHz: 240 MHz / 48 000 ≈ **5 000 cycles per sample per core**.

## 2. Audio path (from the schematic)

```
ESP32-S3 I2S ──► ES8311 (mono codec) ── OUTP (DACP) ─┬─► NS4150B amp ─► 1 W speaker
  G41 SCLK/BCLK                                      │       (muted by HP_DET when a plug is inserted; hardware mute confirmed, but HP_DET is not on a readable GPIO)
  G43 LRCK                                           └─► 22 µF ─► PJ-342 3.5 mm jack (L and R carry the same signal)
  G42 DSDIN (S3 → codec, playback)
  G46 ASDOUT (codec → S3, record)
  I2C G8/G9 (control; shared bus)
MEMS mic (MSM381A3729, differential) ──► ES8311 MIC1P/N
```

- **The output is mono.** ES8311 has a single DAC, and both jack channels are tied to it.
- **No MCLK.** The ES8311 MCLK pin is not connected, so the codec must run with
  SCLK/BCLK as its master clock. That limits the sample-rate/BCLK ratios. 48 kHz
  works with no MCLK (probe, 2026-10-27). 96 kHz is **(verify)**.
- **The jack is a hot line level** (probe: headphones very loud off-ear at -6 dBFS).
- **The jack works as a line out.** Into the PO-33's high-impedance line in, the
  22 µF coupling cap is no problem (corner < 1 Hz). Into 32 Ω headphones it cuts
  bass below ~225 Hz.
- **No line in on the jack.** The only audio input is the onboard MEMS mic.
- The headphone-detect circuit mutes the speaker amp in hardware.

## 3. User I/O

| Item | Detail |
|---|---|
| Display | ST7789V2 1.14" 240×135 IPS; on the internal Stamp bus (G33–G37), backlight G38 |
| Keyboard | 56 keys (4×14) via TCA8418 (I2C 0x34, INT G11). Key rollover is a matrix: key coordinates (probe-confirmed): x 0-13 left to right, y 0-3 top to bottom. Modifier + key chords confirmed "pretty reliable" on the device (probe, 2026-10-27) |
| IMU | BMI270 6-axis, I2C 0x69. Tilt/shake can be a modulation source |
| Buttons | G0 (BOOT/user), reset; side power switch |
| IR TX | G44 |
| RGB LED | WS2812 on the Stamp, G21 |
| Battery | 1S LiPo, 1500 or 1750 mAh (sources conflict); ADC on G10 |

## 4. Expansion

**Grove HY2.0-4P (Port A):** G2 / G1 + 5 V + GND. Slide switch SW2 picks the 5 V source:
the board's boost converter (+5VOUT) or USB VBUS (+5VIN). These two pins are the
**only GPIOs left free with the LoRa cap attached.**

**EXT 2.54-14P (rear):**

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | G3 (RESET) | 14 | +5VIN (= USB VBUS) |
| 2 | G4 (INT) | 13 | GND |
| 3 | G6 (BUSY) | 12 | +5VOUT (boost) |
| 4 | G40 SCK (shared with SD) | 11 | G8 SDA (shared I2C) |
| 5 | G14 MOSI (shared with SD) | 10 | G9 SCL (shared I2C) |
| 6 | G39 MISO (shared with SD) | 9 | G13 (UART) |
| 7 | G5 CS | 8 | G15 (UART) |

**Shared I2C bus G8/G9:** keyboard, IMU, codec, the cap's IO expander, EXT.
Audio-thread code must never block on I2C.

## 5. USB — can it host a MIDI keyboard?

- The ESP32-S3 has native USB-OTG full-speed (G19/G20) on the USB-C port. **Host
  mode is supported by the silicon** (ESP-IDF `usb_host`, TinyUSB host).
- **The board cannot power the device.** On the Stamp schematic, USB-C VBUS goes
  through a polyfuse straight to the `+5VIN` rail. CC has 5.1 kΩ pull-downs, so
  the board is a sink only, and nothing on the board can drive 5 V onto VBUS. A
  bus-powered MIDI keyboard on a plain OTG adapter gets no power. The Stamp-S3A
  topology is assumed to match StampS3 v0.3.3 **(verify)**.
- **Workable:** a powered USB-C OTG Y-adapter or powered hub. An external 5 V
  supply feeds VBUS, which powers both the keyboard and the Cardputer (it also
  charges, and the power-path MOSFET Q1 disconnects the battery while 5 V is present).
- **Do not** jumper EXT 5VOUT → 5VIN. The boost output would feed back through D1
  into its own input and through the charger, forming a loop.
- **Debugging cost:** in host mode the USB-Serial/JTAG console is gone, and UART0's
  pins are taken (G43 = I2S LRCK, G44 = IR). Log to the screen, to UART1 on Grove
  G1/G2 through a USB-UART dongle, or over WiFi. Flash by holding G0 at reset.
- **Alternatives:** BLE-MIDI (native on the S3), or DIN/TRS MIDI via an optocoupler
  on a Grove UART (G1/G2). Both avoid the VBUS problem.

## 6. Cap LoRa-1262

| Item | Value |
|---|---|
| Radio | SX1262, 868–923 MHz, +22 dBm, SPI (shares SD bus G14/G39/G40), NSS G5, RST G3, IRQ G4, BUSY G6 |
| GNSS | ATGM336H-6N (AT6668), UART 115200 on G13/G15, up to 10 Hz |
| IO expander | PI4IOE5V6408 on I2C; P0 = antenna switch (set high before LoRa init) |
| Extra port | HY2.0-4P I2C (G8/G9) on the cap |
| Power | LoRa ~163 mA @ 5 V TX; GNSS ~33 mA |
| Warning | Never transmit without the antenna attached |
| SD bus | G5 (NSS) floats low after reset. With the cap fitted, the SX1262 is then selected and drives MISO, so SD init fails (`Card Failed! cmd: 0x00`, `f_mount failed: (3)`). Driving G5 high before `SD.begin` fixes it (device, 2026-10-29) |

**Relevance to the synth: low.** LoRa latency (tens to hundreds of ms) and duty
cycle rule out note/clock sync. ESP-NOW over the built-in WiFi is far better
(~1–3 ms) for linking units. The cap uses every EXT GPIO, so leave it off for
synth use unless a specific feature needs it.

## 7. PO-33 K.O! integration notes

- The PO-33 line in samples whatever is plugged into its input jack. The Cardputer jack → PO line in works directly.
- PO sync modes SY4/SY5 expect sync + audio on one stereo plug. The Cardputer
  output is mono, so sending its own clock would need a GPIO pulse (e.g. Grove G1
  through a divider) wired into the other channel of a custom TRS cable **(verify
  which channel the PO expects)**.
- Since the PO-33 does the sampling and sequencing, the synth should aim for
  sound quality and range, not long sample memory.

## Sources

Offline copies of the schematics and datasheets are in [`docs/reference/`](reference/README.md).


- https://docs.m5stack.com/en/core/Cardputer-Adv
- Schematic: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1178/Sch_M5CardputerAdv_v1.0_2025_06_20_17_19_58.pdf
- Stamp schematic: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1150/Sch_StampS3_v0.3.3.pdf
- https://docs.m5stack.com/en/cap/Cap_LoRa-1262
- https://www.cnx-software.com/2025/10/23/m5stack-cardputer-adv-esp32-s3-computer-gains-improved-antenna-larger-1750-mah-battery-es8311-audio-codec/
- https://fubsypoly-hacking.pages.dev/m5stack-cardputer-adv/vol-2/
- https://www.soundtech.co.uk/musicians-blog/pocket-operator-sync-modes.html
