# Plaits + stmlib (vendored)

- Plaits: https://github.com/pichenettes/eurorack, commit f4cfa7697095a28d292bff758beb51deb49b5a86,
  `plaits/dsp/` and `plaits/resources.{cc,h}` only (no drivers, UI or bootloader).
- stmlib: https://github.com/pichenettes/stmlib, commit d18def816c51d1da0c108236928b2bbd25c17481,
  `stmlib.h`, `dsp/` and `utils/` only.
- License: MIT (Emilie Gillet), see `LICENSE` and `stmlib/LICENSE`.
- Local changes: `plaits/user_data.h` is replaced by a stub (no flash user data). Built with
  `-DTEST`, which selects stmlib's portable C++ paths instead of ARM inline assembly.
