# v1.2 Task 11 presets and the v1.3 PO-33 library (exec'd by tools/presets.py)
#
# v1.3: every preset plays on its own (a CLK, SEQ or KIT; a KIT preset needs one PLAY), keeps
# KEYS where it makes sense (the clock plays C, the keyboard plays / transposes), fits 4000 cycles
# from the per-type costs, and peaks near -3 dBFS at OUT's line volume (TRIM below, from the bench:
# tools/preset_levels.py).


def lay(name, mods, cables, inamt=None):
    """mods: [(key, type, params)] placed left to right from row 1 (cells 12, 13, ...)."""
    preset(name, {k: (t, 12 + i, p) for i, (k, t, p) in enumerate(mods)}, cables, inamt)


preset("DRUMS", {
    "c": ("clk", 12, {"bpm": 118}), "t": ("trg", 13, {"lane1": 0b000100010001, "lane2": 0b000000010000, "lane3": 0b010101010101, "len": 8}),
    "k": ("kck", 24, {}), "s": ("snr", 25, {}), "h": ("hat", 26, {"decay": 0.2}), "m": ("mix", 27, {"lvl3": 0.5}),
    "x": ("out", 29, {}),
}, [("c", "out", "t", "clk"), ("t", "t1", "k", "trig"), ("t", "t2", "s", "trig"), ("t", "t3", "h", "trig"),
    ("k", "out", "m", "in1"), ("s", "out", "m", "in2"), ("h", "out", "m", "in3"), ("m", "out", "x", "in")])

preset("ACID", {
    "c": ("clk", 12, {"bpm": 128}), "q": ("seq", 13, {"s1": 0, "s2": 12, "s3": 0, "s4": 3, "s5": 0, "s6": 15, "s7": 7, "s8": 10,
                                                     "gates": 0b11011011, "len": 8, "gatelen": 0.4}),
    "o": ("osc", 14, {"wave": 2, "oct": -1}), "f": ("lad", 15, {"cutoff": 300, "res": 0.75, "drive": 0.4}),
    "e": ("ad", 26, {"atk": 1, "dec": 180}), "a": ("vca", 16, {}), "d": ("dly", 17, {"time": 350, "fdbk": 0.4, "mix": 0.25}),
    "x": ("out", 18, {}),
}, [("c", "out", "q", "clk"), ("q", "pitch", "o", "pitch"), ("o", "out", "f", "in"), ("q", "gate", "e", "trig"),
    ("e", "out", "f", "cut", 0.6), ("f", "out", "a", "in"), ("e", "out", "a", "cv"), ("a", "out", "d", "in"),
    ("d", "out", "x", "in")])

preset("PLUCK", {
    "k": ("keys", 12, {}), "s": ("str", 13, {"struct": 0.2, "bright": 0.6, "damp": 0.4}),
    "r": ("rev", 14, {"size": 0.85, "decay": 0.75, "mix": 0.35}), "x": ("out", 15, {}), "c": ("clk", 24, {"bpm": 70}),
}, [("k", "pitch", "s", "pitch"), ("k", "gate", "s", "trig"), ("c", "out", "s", "trig"), ("s", "out", "r", "in"),
    ("r", "out", "x", "in")])

preset("FMBELL", {
    "k": ("keys", 12, {}), "f": ("fmo", 13, {"ratio": 3.5, "index": 0.5}), "a": ("vca", 14, {}),
    "e": ("ad", 25, {"atk": 2, "dec": 1500, "curve": -4}), "r": ("dly", 15, {"time": 420, "fdbk": 0.45, "mix": 0.3}),
    "x": ("out", 16, {}), "c": ("clk", 24, {"bpm": 40}),
}, [("k", "pitch", "f", "pitch"), ("k", "gate", "e", "trig"), ("c", "out", "e", "trig"), ("f", "out", "a", "in"),
    ("e", "out", "a", "cv"), ("e", "out", "f", "index", 0.5), ("a", "out", "r", "in"), ("r", "out", "x", "in")])

preset("DRONE", {
    "s": ("ssw", 12, {"oct": -1, "detune": 0.5}), "f": ("lad", 13, {"cutoff": 500, "res": 0.4}),
    "l": ("lfo", 25, {"wave": 6, "rate": 0.15}), "c": ("cho", 14, {"mix": 0.5}),
    "r": ("dly", 15, {"time": 700, "fdbk": 0.6, "mix": 0.4, "tone": 0.5}), "x": ("out", 16, {}),
}, [("s", "out", "f", "in"), ("l", "out", "f", "cut", 0.4), ("f", "out", "c", "in"), ("c", "out", "r", "in"),
    ("r", "out", "x", "in")])

preset("TURING", {
    "c": ("clk", 12, {"bpm": 100}), "t": ("tur", 13, {"len": 8, "prob": 0.15, "range": 2}),
    "q": ("qnt", 14, {"scale": 10}), "o": ("mdl", 15, {"struct": 0.6, "bright": 0.5}),
    "r": ("dly", 16, {"time": 450, "fdbk": 0.5, "mix": 0.35, "tone": 0.6}), "x": ("out", 17, {}),
}, [("c", "out", "t", "clk"), ("t", "cv", "q", "in"), ("q", "out", "o", "pitch"), ("t", "gate", "o", "trig"),
    ("o", "out", "r", "in"), ("r", "out", "x", "in")])

# v1.2 Task 13: GRD (Plaits / Grind)
# GRD is a whole Grind voice: osc -> ladder -> VCA with its own ADS and LFO (LFO -> timbre here).
preset("GRIND", {
    "k": ("keys", 12, {}), "g": ("grd", 13, {"eng": 0, "harm": 0.4, "timb": 0.45, "morph": 0.3, "cut": 1800,
                                             "res": 0.35, "feg": 0.4, "dcy": 500, "sus": 0.5, "lrate": 0.3,
                                             "ltim": 0.3}),
    "r": ("cho", 14, {"mix": 0.35}), "x": ("out", 15, {}), "c": ("clk", 24, {"bpm": 60}),
}, [("k", "pitch", "g", "pitch"), ("k", "gate", "g", "trig"), ("c", "out", "g", "trig"), ("g", "out", "r", "in"),
    ("r", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: drum kits (KIT, PLAY)
# One pass = 8 hits, GAP apart: the PO-33 slices a drum sample into 8, one hit per pad.


def kit8(name, voices, kitp, fx=None, bal=None):
    """voices: 8 (type, params); lane l fires voice l on step l (KIT defaults). Two mixers -> OUT.
    bal: 8 dB cuts (mixer levels) that even out the slices (bench: steps)."""
    mods = [("t", "kit", dict({"len": 8, "gap": 500}, **kitp))]
    mods += [("v%d" % i, t, p) for i, (t, p) in enumerate(voices)]
    lv = [round(10 ** (d / 20), 3) for d in (bal or [0] * 8)]
    mods += [("ma", "mix", {"lvl%d" % (i + 1): lv[i] for i in range(4)}),
             ("mb", "mix", {"lvl%d" % (i + 1): lv[4 + i] for i in range(4)})]
    cables = []
    for i, (t, _) in enumerate(voices):
        cables.append(("t", "t%d" % (i + 1), "v%d" % i, "trig"))
        cables.append(("t", "acc", "v%d" % i, "acc"))
        cables.append(("v%d" % i, "out", "ma" if i < 4 else "mb", "in%d" % (i % 4 + 1)))
    if fx:
        mods.append(("fx", fx[0], fx[1]))
        cables += [("ma", "out", "fx", "in"), ("mb", "out", "fx", "in"), ("fx", "out", "x", "in")]
    else:
        cables += [("ma", "out", "x", "in"), ("mb", "out", "x", "in")]
    mods.append(("x", "out", {}))
    lay(name, mods, cables)


kit8("808KIT", [
    ("kck", {"freq": 48, "tone": 0.3, "decay": 0.85, "atkfm": 0.2, "selffm": 0.1}),
    ("snr", {"freq": 180, "tone": 0.4, "decay": 0.45, "snappy": 0.5}),
    ("hat", {"freq": 3500, "tone": 0.5, "decay": 0.15}),
    ("hat", {"freq": 3500, "tone": 0.5, "decay": 0.65}),
    ("kck", {"freq": 75, "tone": 0.5, "decay": 0.6, "atkfm": 0.1, "selffm": 0.0}),
    ("kck", {"freq": 110, "tone": 0.5, "decay": 0.5, "atkfm": 0.1, "selffm": 0.0}),
    ("snr", {"freq": 300, "tone": 0.8, "decay": 0.3, "snappy": 1.0}),
    ("hat", {"freq": 8000, "tone": 0.9, "decay": 0.08, "noise": 0.2}),
], {"accl": 0b00000001, "acc": 0.6}, bal=[0, -2, 0, -11, -8, -9, -6, -11])

kit8("909KIT", [
    ("kck", {"freq": 58, "tone": 0.65, "decay": 0.55, "atkfm": 0.6, "selffm": 0.45}),
    ("snr", {"freq": 220, "tone": 0.7, "decay": 0.5, "snappy": 0.8}),
    ("hat", {"freq": 7000, "tone": 0.7, "decay": 0.2, "noise": 0.8}),
    ("hat", {"freq": 7000, "tone": 0.7, "decay": 0.7, "noise": 0.8}),
    ("kck", {"freq": 90, "tone": 0.6, "decay": 0.45, "atkfm": 0.4, "selffm": 0.2}),
    ("kck", {"freq": 140, "tone": 0.6, "decay": 0.4, "atkfm": 0.4, "selffm": 0.2}),
    ("snr", {"freq": 350, "tone": 0.9, "decay": 0.35, "snappy": 1.0}),
    ("hat", {"freq": 9500, "tone": 0.3, "decay": 0.9, "noise": 0.4}),
], {"accl": 0b00010001, "acc": 0.5}, ("dly", {"time": 120, "fdbk": 0.15, "mix": 0.12, "tone": 0.6}),
   bal=[-3, -2, -1, -6, -6, -6, -3, 0])

# stacked steps: K, S, CH, OH, K+CH, S+CH, K+S, K+S+CH
STACK = {"l1": 0b11010001, "l2": 0b11100010, "l3": 0b10110100, "l4": 0b00001000}

lay("LOFIKT", [
    ("t", "kit", dict({"len": 8, "gap": 500}, **STACK)),
    ("k", "kck", {"freq": 52, "tone": 0.5, "decay": 0.6, "atkfm": 0.3}),
    ("s", "snr", {"freq": 190, "tone": 0.3, "decay": 0.4, "snappy": 0.4}),
    ("h", "hat", {"freq": 5000, "tone": 0.4, "decay": 0.2}),
    ("o", "hat", {"freq": 5000, "tone": 0.4, "decay": 0.6}),
    ("m", "mix", {"lvl4": 0.56}), ("c", "cru", {"bits": 7, "rate": 11025, "mix": 1}), ("x", "out", {}),
], [("t", "t1", "k", "trig"), ("t", "t2", "s", "trig"), ("t", "t3", "h", "trig"), ("t", "t4", "o", "trig"),
    ("k", "out", "m", "in1"), ("s", "out", "m", "in2"), ("h", "out", "m", "in3"), ("o", "out", "m", "in4"),
    ("m", "out", "c", "in"), ("c", "out", "x", "in")])

lay("METKIT", [
    ("t", "kit", {"len": 8, "gap": 500, "l1": 0b00100001, "l2": 0b10000010, "l3": 0b01000100,
                  "l4": 0b01101000, "l5": 0b10010000}),
    ("b", "mdl", {"struct": 0.75, "bright": 0.6, "damp": 0.4, "accent": 1}),
    ("r", "res", {"struct": 0.6, "bright": 0.8, "damp": 0.5}),
    ("s", "str", {"struct": 0.1, "bright": 0.9, "damp": 0.3, "oct": 1}),
    ("h", "hat", {"freq": 9000, "tone": 0.5, "decay": 0.4, "noise": 0.5, "accent": 1}),
    ("g", "mdl", {"struct": 0.9, "bright": 0.4, "damp": 0.6, "oct": -1, "accent": 1}),
    ("m", "mix", {"lvl2": 0.2, "lvl3": 0.5, "lvl4": 0.35}), ("d", "dly", {"time": 180, "fdbk": 0.3, "mix": 0.2}), ("x", "out", {}),
], [("t", "t1", "b", "trig"), ("t", "t2", "r", "in"), ("t", "t3", "s", "trig"), ("t", "t4", "h", "trig"),
    ("t", "t5", "g", "trig"), ("b", "out", "m", "in1"), ("r", "out", "m", "in2"), ("s", "out", "m", "in3"),
    ("h", "out", "m", "in4"), ("m", "out", "d", "in"), ("g", "out", "d", "in"), ("d", "out", "x", "in")])

# GRD is heavy: no step stacks it with two drums. K, S, CH, OH, K+CH, S+CH, K+S, S+OH
lay("PLTKIT", [
    ("t", "kit", {"len": 8, "gap": 500, "l1": 0b01010001, "l2": 0b11100010, "l3": 0b00110100, "l4": 0b10001000}),
    ("g", "grd", {"eng": 13, "oct": -1, "harm": 0.5, "timb": 0.5, "morph": 0.6, "decay": 0.6, "vca": 1, "flt": 2}),
    ("s", "snr", {"freq": 200, "tone": 0.6, "decay": 0.45, "snappy": 0.7}),
    ("h", "hat", {"freq": 6500, "tone": 0.6, "decay": 0.2}),
    ("o", "hat", {"freq": 6500, "tone": 0.6, "decay": 0.6}),
    ("m", "mix", {"lvl2": 0.63, "lvl3": 0.8, "lvl4": 0.4}), ("x", "out", {}),
], [("t", "t1", "g", "trig"), ("t", "t2", "s", "trig"), ("t", "t3", "h", "trig"), ("t", "t4", "o", "trig"),
    ("g", "out", "m", "in1"), ("s", "out", "m", "in2"), ("h", "out", "m", "in3"), ("o", "out", "m", "in4"),
    ("m", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: basses
lay("SUBBAS", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 60}), ("o", "osc", {"wave": 0, "oct": -2}),
    ("p", "osc", {"wave": 3, "oct": -1}), ("m", "mix", {"lvl2": 0.3}), ("f", "vcf", {"cutoff": 600, "res": 0.2}),
    ("e", "env", {"a": 2, "d": 400, "s": 0.7, "r": 200}), ("a", "vca", {}), ("x", "out", {}),
], [("k", "pitch", "o", "pitch"), ("k", "pitch", "p", "pitch"), ("o", "out", "m", "in1"), ("p", "out", "m", "in2"),
    ("m", "out", "f", "in"), ("f", "out", "a", "in"), ("k", "gate", "e", "gate"), ("c", "out", "e", "gate"),
    ("e", "out", "a", "cv"), ("a", "out", "x", "in")])

lay("RUBBER", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 90}), ("f", "fmo", {"oct": -2, "ratio": 1, "index": 0.5}),
    ("l", "lpg", {"decay": 400, "color": 0.6}), ("o", "ovd", {"drive": 0.3, "tone": 0.5}), ("x", "out", {}),
], [("k", "pitch", "f", "pitch"), ("f", "out", "l", "in"), ("k", "gate", "l", "trig"), ("c", "out", "l", "trig"),
    ("l", "out", "o", "in"), ("o", "out", "x", "in")])

lay("REESE", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 60}), ("s", "ssw", {"oct": -2, "detune": 0.55, "mix": 0.8}),
    ("f", "lad", {"cutoff": 500, "res": 0.3, "drive": 0.3}), ("l", "lfo", {"rate": 0.3}),
    ("e", "env", {"a": 5, "d": 300, "s": 0.8, "r": 300}), ("a", "vca", {}), ("x", "out", {}),
], [("k", "pitch", "s", "pitch"), ("s", "out", "f", "in"), ("l", "out", "f", "cut", 0.3), ("f", "out", "a", "in"),
    ("k", "gate", "e", "gate"), ("c", "out", "e", "gate"), ("e", "out", "a", "cv"), ("a", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: leads
lay("SYNCLD", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 90}), ("v", "vso", {"sync": 1, "ratio": 3, "shape": 1}),
    ("f", "vcf", {"cutoff": 3000, "res": 0.2}), ("e", "env", {"a": 5, "d": 300, "s": 0.7, "r": 300}),
    ("a", "vca", {}), ("d", "dly", {"time": 333, "fdbk": 0.35, "mix": 0.25}), ("x", "out", {}),
], [("k", "pitch", "v", "pitch"), ("v", "out", "f", "in"), ("f", "out", "a", "in"), ("k", "gate", "e", "gate"),
    ("c", "out", "e", "gate"), ("e", "out", "a", "cv"), ("a", "out", "d", "in"), ("d", "out", "x", "in")])

lay("SQRLD", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 120}), ("o", "osc", {"wave": 3}), ("p", "osc", {"wave": 3, "fine": 8}),
    ("m", "mix", {}), ("f", "vcf", {"cutoff": 2200, "res": 0.35}), ("e", "env", {"a": 3, "d": 200, "s": 0.6, "r": 250}),
    ("a", "vca", {}), ("h", "cho", {"rate": 0.4, "mix": 0.4}), ("x", "out", {}),
], [("k", "pitch", "o", "pitch"), ("k", "pitch", "p", "pitch"), ("o", "out", "m", "in1"), ("p", "out", "m", "in2"),
    ("m", "out", "f", "in"), ("f", "out", "a", "in"), ("k", "gate", "e", "gate"), ("c", "out", "e", "gate"),
    ("e", "out", "a", "cv"), ("a", "out", "h", "in"), ("h", "out", "x", "in")])

lay("VOXLD", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 60}), ("v", "vos", {"form1": 700, "form2": 1200, "shape": 0.5}),
    ("l", "lfo", {"rate": 5}), ("e", "env", {"a": 40, "d": 300, "s": 0.8, "r": 400}), ("a", "vca", {}),
    ("h", "cho", {"rate": 0.5, "mix": 0.35}), ("x", "out", {}),
], [("k", "pitch", "v", "pitch"), ("l", "out", "v", "pitch", 0.004), ("v", "out", "a", "in"),
    ("k", "gate", "e", "gate"), ("c", "out", "e", "gate"), ("e", "out", "a", "cv"), ("a", "out", "h", "in"),
    ("h", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: plucks
lay("KALIMB", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 100}), ("m", "mdl", {"struct": 0.15, "bright": 0.45, "damp": 0.55}),
    ("d", "dly", {"time": 300, "fdbk": 0.3, "mix": 0.25}), ("x", "out", {}),
], [("k", "pitch", "m", "pitch"), ("k", "gate", "m", "trig"), ("c", "out", "m", "trig"), ("m", "out", "d", "in"),
    ("d", "out", "x", "in")])

lay("HARP", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 80}), ("s", "str", {"struct": 0.55, "bright": 0.7, "damp": 0.25}),
    ("h", "cho", {"rate": 0.3, "mix": 0.3}), ("x", "out", {}),
], [("k", "pitch", "s", "pitch"), ("k", "gate", "s", "trig"), ("c", "out", "s", "trig"), ("s", "out", "h", "in"),
    ("h", "out", "x", "in")])

lay("LPGPLK", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 120}), ("o", "osc", {"wave": 1}), ("l", "lpg", {"decay": 350, "color": 0.8}),
    ("d", "dly", {"time": 250, "fdbk": 0.35, "mix": 0.3}), ("x", "out", {}),
], [("k", "pitch", "o", "pitch"), ("o", "out", "l", "in"), ("k", "gate", "l", "trig"), ("c", "out", "l", "trig"),
    ("l", "out", "d", "in"), ("d", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: pads / drones
lay("WARM", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 20}), ("s", "ssw", {"oct": -1, "detune": 0.45}),
    ("f", "lad", {"cutoff": 900, "res": 0.2}), ("e", "env", {"a": 800, "d": 1000, "s": 0.8, "r": 1500}),
    ("a", "vca", {}), ("h", "cho", {"rate": 0.3, "mix": 0.5}), ("x", "out", {}),
], [("k", "pitch", "s", "pitch"), ("s", "out", "f", "in"), ("f", "out", "a", "in"), ("k", "gate", "e", "gate"),
    ("c", "out", "e", "gate"), ("e", "out", "a", "cv"), ("a", "out", "h", "in"), ("h", "out", "x", "in")])

lay("GLASS", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 20}), ("o", "hrm", {"center": 4, "rolloff": 0.4}),
    ("e", "env", {"a": 600, "d": 1000, "s": 0.9, "r": 2000}), ("a", "vca", {}),
    ("p", "pha", {"rate": 0.2, "mix": 0.5}), ("d", "dly", {"time": 600, "fdbk": 0.5, "mix": 0.3, "tone": 0.6}),
    ("x", "out", {}),
], [("k", "pitch", "o", "pitch"), ("o", "out", "a", "in"), ("k", "gate", "e", "gate"), ("c", "out", "e", "gate"),
    ("e", "out", "a", "cv"), ("a", "out", "p", "in"), ("p", "out", "d", "in"), ("d", "out", "x", "in")])

lay("CHOIR", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 20}), ("v", "vos", {"form1": 500, "form2": 1000, "shape": 0.3}),
    ("e", "env", {"a": 700, "d": 1000, "s": 0.8, "r": 1800}), ("a", "vca", {}),
    ("h", "cho", {"rate": 0.25, "depth": 0.7, "mix": 0.6}), ("d", "dly", {"time": 500, "fdbk": 0.45, "mix": 0.3, "tone": 0.5}),
    ("x", "out", {}),
], [("k", "pitch", "v", "pitch"), ("v", "out", "a", "in"), ("k", "gate", "e", "gate"), ("c", "out", "e", "gate"),
    ("e", "out", "a", "cv"), ("a", "out", "h", "in"), ("h", "out", "d", "in"), ("d", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: chords
lay("CHORDS", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 30}),
    ("g", "grd", {"eng": 6, "harm": 0.3, "timb": 0.5, "morph": 0.4, "cut": 3000, "res": 0.2, "atk": 30, "dcy": 800,
                  "sus": 0.7}),
    ("h", "cho", {"rate": 0.3, "mix": 0.4}), ("x", "out", {}),
], [("k", "pitch", "g", "pitch"), ("k", "gate", "g", "trig"), ("c", "out", "g", "trig"), ("g", "out", "h", "in"),
    ("h", "out", "x", "in")])

lay("STACK", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 40}), ("o", "osc", {"wave": 2}), ("p", "osc", {"wave": 2, "tune": 4}),
    ("q", "osc", {"wave": 2, "tune": 7}), ("m", "mix", {}), ("f", "vcf", {"cutoff": 1500, "res": 0.25}),
    ("e", "env", {"a": 20, "d": 500, "s": 0.6, "r": 600}), ("a", "vca", {}), ("x", "out", {}),
], [("k", "pitch", "o", "pitch"), ("k", "pitch", "p", "pitch"), ("k", "pitch", "q", "pitch"), ("o", "out", "m", "in1"),
    ("p", "out", "m", "in2"), ("q", "out", "m", "in3"), ("m", "out", "f", "in"), ("f", "out", "a", "in"),
    ("k", "gate", "e", "gate"), ("c", "out", "e", "gate"), ("e", "out", "a", "cv"), ("a", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: FX, risers, noise
lay("RISER", [
    ("c", "clk", {"bpm": 20, "div": 1}), ("r", "ad", {"atk": 3000, "dec": 400, "curve": 2}),
    ("n", "noi", {"kind": 0}), ("o", "osc", {"wave": 2, "oct": -1}), ("f", "vcf", {"cutoff": 300, "res": 0.6, "mode": 2}),
    ("a", "vca", {}), ("d", "dly", {"time": 300, "fdbk": 0.5, "mix": 0.35}), ("x", "out", {}),
], [("c", "out", "r", "trig"), ("r", "out", "f", "cut"), ("r", "out", "o", "pitch", 0.4), ("n", "out", "f", "in"),
    ("o", "out", "f", "in"), ("f", "out", "a", "in"), ("r", "out", "a", "cv"), ("a", "out", "d", "in"),
    ("d", "out", "x", "in")])

lay("LASER", [
    ("c", "clk", {"bpm": 120}), ("r", "ad", {"atk": 0.5, "dec": 120, "curve": -6}), ("o", "osc", {"wave": 2, "oct": 1}),
    ("a", "vca", {}), ("d", "dly", {"time": 180, "fdbk": 0.6, "mix": 0.4}), ("x", "out", {}),
], [("c", "out", "r", "trig"), ("r", "out", "o", "pitch", 0.6), ("o", "out", "a", "in"), ("r", "out", "a", "cv"),
    ("a", "out", "d", "in"), ("d", "out", "x", "in")])

lay("WIND", [
    ("n", "noi", {"kind": 1}), ("f", "vcf", {"cutoff": 800, "res": 0.7, "mode": 2}), ("l", "lfo", {"wave": 6, "rate": 0.2}),
    ("r", "rev", {"size": 0.9, "decay": 0.8, "mix": 0.5}), ("x", "out", {}),
], [("n", "out", "f", "in"), ("l", "out", "f", "cut", 0.6), ("f", "out", "r", "in"), ("r", "out", "x", "in")])

# ---------------------------------------------------------------- v1.3: GRD voices
lay("GRDBAS", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 120}),
    ("q", "seq", {"s1": 0, "s2": 0, "s3": 12, "s4": 0, "s5": 3, "s6": 0, "s7": 7, "s8": 10, "len": 8, "gatelen": 0.5}),
    ("g", "grd", {"eng": 0, "oct": -2, "harm": 0.3, "timb": 0.6, "morph": 0.4, "cut": 700, "res": 0.5, "feg": 0.6,
                  "dcy": 250, "sus": 0.3}),
    ("x", "out", {}),
], [("c", "out", "q", "clk"), ("q", "pitch", "g", "pitch"), ("k", "pitch", "g", "pitch"), ("q", "gate", "g", "trig"),
    ("g", "out", "x", "in")])

lay("GRDSWM", [
    ("k", "keys", {}), ("c", "clk", {"bpm": 20}),
    ("g", "grd", {"eng": 8, "harm": 0.6, "timb": 0.4, "morph": 0.5, "cut": 2500, "atk": 400, "dcy": 1000, "sus": 0.8}),
    ("h", "cho", {"rate": 0.3, "mix": 0.4}), ("x", "out", {}),
], [("k", "pitch", "g", "pitch"), ("k", "gate", "g", "trig"), ("c", "out", "g", "trig"), ("g", "out", "h", "in"),
    ("h", "out", "x", "in")])

# OUT gain (dB) per preset for about -3 dBFS peak at the line volume: written by tools/preset_levels.py
GAIN.update({
    "808KIT": 2.1,
    "909KIT": -3.0,
    "ACID": 7.0,
    "BASIC": -5.1,
    "CHOIR": 5.0,
    "CHORDS": 12.3,
    "DRONE": 3.1,
    "DRUMS": -6.9,
    "ECHO": 1.9,
    "FMBELL": 7.2,
    "GLASS": 1.3,
    "GRDBAS": 11.4,
    "GRDSWM": 10.4,
    "GRIND": 13.7,
    "HARP": 2.8,
    "KALIMB": 2.3,
    "LASER": 10.4,
    "LOFIKT": -1.7,
    "LPGPLK": 2.6,
    "METKIT": 2.2,
    "PLTKIT": -2.2,
    "PLUCK": 1.0,
    "REESE": 1.3,
    "RISER": -1.6,
    "RUBBER": -1.7,
    "SQRLD": -5.6,
    "STACK": -14.1,
    "SUBBAS": -3.5,
    "SYNCLD": -2.2,
    "TILT": -7.4,
    "TURING": 10.6,
    "VOXLD": 0.6,
    "WARM": 1.9,
    "WIND": -4.5,
    "WOBBLE": -10.6,
})
