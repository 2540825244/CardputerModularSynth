# v1.2 Task 11 presets (exec'd by tools/presets.py)
preset("DRUMS", {
    "c": ("clk", 12, {"bpm": 118}), "t": ("trg", 13, {"lane1": 0b000100010001, "lane2": 0b000000010000, "lane3": 0b010101010101, "len": 8}),
    "k": ("kck", 24, {}), "s": ("snr", 25, {}), "h": ("hat", 26, {"decay": 0.2}), "m": ("mix", 27, {"lvl3": 0.5}),
    "r": ("rev", 28, {"mix": 0.15, "decay": 0.4}), "x": ("out", 29, {}),
}, [("c", "out", "t", "clk"), ("t", "t1", "k", "trig"), ("t", "t2", "s", "trig"), ("t", "t3", "h", "trig"),
    ("k", "out", "m", "in1"), ("s", "out", "m", "in2"), ("h", "out", "m", "in3"), ("m", "out", "r", "in"),
    ("r", "out", "x", "in")])

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
    "r": ("rev", 14, {"size": 0.85, "decay": 0.75, "mix": 0.35}), "x": ("out", 15, {}),
}, [("k", "pitch", "s", "pitch"), ("k", "gate", "s", "trig"), ("s", "out", "r", "in"), ("r", "out", "x", "in")])

preset("FMBELL", {
    "k": ("keys", 12, {}), "f": ("fmo", 13, {"ratio": 3.5, "index": 0.5}), "a": ("vca", 14, {}),
    "e": ("ad", 25, {"atk": 2, "dec": 1500, "curve": -4}), "r": ("rev", 15, {"mix": 0.3}), "x": ("out", 16, {}),
}, [("k", "pitch", "f", "pitch"), ("k", "gate", "e", "trig"), ("f", "out", "a", "in"), ("e", "out", "a", "cv"),
    ("e", "out", "f", "index", 0.5), ("a", "out", "r", "in"), ("r", "out", "x", "in")])

preset("DRONE", {
    "s": ("ssw", 12, {"oct": -1, "detune": 0.5}), "f": ("lad", 13, {"cutoff": 500, "res": 0.4}),
    "l": ("lfo", 25, {"wave": 6, "rate": 0.15}), "c": ("cho", 14, {"mix": 0.5}),
    "r": ("rev", 15, {"size": 0.95, "decay": 0.85, "mix": 0.5}), "x": ("out", 16, {}),
}, [("s", "out", "f", "in"), ("l", "out", "f", "cut", 0.4), ("f", "out", "c", "in"), ("c", "out", "r", "in"),
    ("r", "out", "x", "in")])

preset("TURING", {
    "c": ("clk", 12, {"bpm": 100}), "t": ("tur", 13, {"len": 8, "prob": 0.15, "range": 2}),
    "q": ("qnt", 14, {"scale": 10}), "o": ("mdl", 15, {"struct": 0.6, "bright": 0.5}),
    "r": ("rev", 16, {"size": 0.9, "decay": 0.7, "mix": 0.4}), "x": ("out", 17, {}),
}, [("c", "out", "t", "clk"), ("t", "cv", "q", "in"), ("q", "out", "o", "pitch"), ("t", "gate", "o", "trig"),
    ("o", "out", "r", "in"), ("r", "out", "x", "in")])

# v1.2 Task 13: GRD (Plaits / Grind)
# GRD is a whole Grind voice: osc -> ladder -> VCA with its own ADS and LFO (LFO -> timbre here).
preset("GRIND", {
    "k": ("keys", 12, {}), "g": ("grd", 13, {"eng": 0, "harm": 0.4, "timb": 0.45, "morph": 0.3, "cut": 1800,
                                             "res": 0.35, "feg": 0.4, "dcy": 500, "sus": 0.5, "lrate": 0.3,
                                             "ltim": 0.3}),
    "r": ("rev", 14, {"mix": 0.25}), "x": ("out", 15, {}),
}, [("k", "pitch", "g", "pitch"), ("k", "gate", "g", "trig"), ("g", "out", "r", "in"), ("r", "out", "x", "in")])
