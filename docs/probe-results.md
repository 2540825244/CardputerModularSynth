# Probe results

Firmware: `pio run -e probe` (`src/probe/probe_main.cpp`). Flash `.pio/build/probe/firmware.bin`.
Run by the owner on the real Cardputer-Adv via bmorcelli's Launcher, 2026-10-27.

## (a) Tone: clean on the speaker and on the jack?
(440 Hz sine, then `space` for the 100 Hz saw; note aliasing, bass, distortion, silence)

Answer: 48 kHz with no MCLK works. The sine sounds fine on the speaker and on headphones.
The raw 100 Hz saw is "a bit jagged" (expected from a naive saw). Loudness and tone on the
speaker vary with distance and angle (acoustic). Headphones are very loud even off-ear at
-6 dBFS, so the jack is a hot line level. Keep the OUT `line` default conservative and tune it
against the PO-33 later (the PO-33 was out of battery, so not yet tested into it).

## (b) Keys: corner/side key (x,y) and chords
Corner/side keys (x,y), with x=0 left and y=0 top assumed (confirmed):

| Key | (x,y) |
|---|---|
| top-left | esc (0,0) |
| top-right | del (13,0) |
| bottom-left | ctrl (0,3) |
| bottom-right | space (13,3) |

Chords (which register fully): `fn`+`q`, `ctrl`+`z`, `fn`+`ctrl`+`a`, three grid keys at once:

Answer: the owner reports modifier + key chords are "pretty reliable" (2026-10-27). Done.

## (c) HP_DET: did any GPIO change with a plug?
Answer: No. No GPIO changed with the plug in or out, so HP_DET is not readable by firmware.
The hardware speaker mute on plug-in works reliably. v1 keeps manual SPK/LINE only; the
auto-destination step (Task 20) is dropped.

## (d) RAM
| free (8bit) | largest block | min free | (4th number) |
|---|---|---|---|
| 325820 | 294900 | 320692 | before the sprite |
| 252432 | 233460 | 252432 | after a 64.8 KB sprite + 8 KB UI stack |

The Launcher does not change RAM. The real firmware sprite is 16 KB, so the largest block is
about 280 KB, above the 260 KB rule. `kDelaySlots` stays 2.

## (e) Board name and SD
Board id/name: not stated explicitly; the probe ran and the keyboard reader worked.
SD mounted (type, size): OK, type 3 (SDHC), 59818 MB.
