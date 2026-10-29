# DaisySP (vendored)

- URL: https://github.com/electro-smith/DaisySP
- Commit: 2c72eaf9eac5fc0dca1919d65d606da907832618
- Scope: MIT core only (`Source/` and `LICENSE`). The `DaisySP-LGPL` submodule is not included.
- Local changes (v1.3, marked `// modsynth:` in the code): speed on the ESP32-S3 (no FPU divide,
  libm sinf/powf cost hundreds of cycles). The unmodified sources are kept, renamed to
  `namespace daisysp_ref`, in `test/test_daisysp_fast/ref/`; `test/test_daisysp_fast` checks the
  modules built on these classes against them.
  - `Utility/modsynth_fast.h` (new): FastRecip, FastExp2, FastSinHalfPi, FastSin2Pi, FastSqrt,
    FastPowQuarter, FastRand.
  - `Filters/svf`: SetFreq/SetRes return early on a repeated value (exact); `SetFreqResFast()`
    (fast math, for a cutoff moving every sample); inline `ProcessBand()` (band output only).
  - `Drums/analogsnaredrum`, `Drums/analogbassdrum`, `Drums/hihat` (+ SquareNoise),
    `PhysicalModeling/resonator` (+ ResonatorSvf `Coefs`/`ProcessCoefs`), `modalvoice`,
    `KarplusString`: coefficients computed only after a setter changes a value (dirty flag),
    per-sample constants once in Init, divides by recip0.s. AnalogBassDrum retunes its resonator
    with `SetFreqResFast()`. The snare's five resonators are unrolled with `ProcessBand()`.
  - `Effects/autowah`: powf/cosf per sample -> FastExp2/FastSinHalfPi.
  - `Synthesis/vosim`, `zoscillator`, `formantosc`: sinf -> FastSin2Pi, `freq / sr` -> `freq * (1/sr)`.
  - Snare, hi-hat, string voice and string: `rand()` -> `FastRand()` (an LCG on the device, still
    `rand()` natively).
