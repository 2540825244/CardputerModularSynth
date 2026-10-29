# Plaits + stmlib (vendored)

- Plaits: https://github.com/pichenettes/eurorack, commit f4cfa7697095a28d292bff758beb51deb49b5a86,
  `plaits/dsp/` and `plaits/resources.{cc,h}` only (no drivers, UI or bootloader).
- stmlib: https://github.com/pichenettes/stmlib, commit d18def816c51d1da0c108236928b2bbd25c17481,
  `stmlib.h`, `dsp/` and `utils/` only.
- License: MIT (Emilie Gillet), see `LICENSE` and `stmlib/LICENSE`.
- Local changes: `plaits/user_data.h` is replaced by a stub (no flash user data). Built with
  `-DTEST`, which selects stmlib's portable C++ paths instead of ARM inline assembly.
- v1.3 speed edits, marked `// modsynth:` (the ESP32-S3 has no FPU divide: a ROM call, ~66 cycles):
  - `plaits/modsynth_iram.h` (new): `MODSYNTH_IRAM` = `IRAM_ATTR` on the device. `Voice::Render`
    and `VirtualAnalogEngine::Render` (the default engine) run from IRAM.
  - `stmlib/dsp/dsp.h`: `FastRecip()` (recip0.s + 2 Newton steps); `SoftLimit` uses it.
  - `stmlib/dsp/filter.h`: `Svf::set_f_q` computes r and h with `FastRecip`.
  - `plaits/dsp/oscillator/variable_shape_oscillator.h`: the per-sample slopes with `FastRecip`.
  - `plaits/dsp/oscillator/grainlet_oscillator.h`: the per-sample divides with `FastRecip`.
  - `plaits/dsp/physical_modelling/resonator.h`: ResonatorSvf coefficients with `FastRecip`, the
    four-mode loop unrolled.
  - `plaits/dsp/oscillator/harmonic_oscillator.h`: the harmonic loop unrolled (same arithmetic).
  - The unmodified classes are kept for `test/test_plaits_fast` in `test/test_plaits_fast/ref_plaits.h`.
