#!/usr/bin/env python3
"""Sets each preset's OUT gain (GAIN in tools/presets_more.py) for about -3 dBFS peak.

Runs the native preset bench (test/test_presets), reads each preset's peak, adjusts GAIN by the
difference, regenerates src/core/presets.cpp, and repeats (OUT soft-clips, so a few rounds).
Run: python3 tools/preset_levels.py
"""
import os, re, subprocess

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MORE = os.path.join(ROOT, "tools", "presets_more.py")
PIO = os.path.expanduser("~/.platformio/penv/bin/pio")
TARGET, TOL = -3.0, 0.5


def gains():
    s = open(MORE).read()
    m = re.search(r"GAIN\.update\((\{.*?\})\)", s, re.S)
    return eval(m.group(1)) if m else {}


def write(g):
    s = open(MORE).read()
    body = "{\n" + "".join('    "%s": %.1f,\n' % (k, v) for k, v in sorted(g.items()) if abs(v) >= 0.05) + "}"
    s = re.sub(r"GAIN\.update\(\{.*?\}\)", "GAIN.update(" + body + ")", s, flags=re.S)
    open(MORE, "w").write(s)


def bench():
    subprocess.run(["python3", os.path.join(ROOT, "tools", "presets.py")], check=True, cwd=ROOT)
    out = subprocess.run([PIO, "test", "-e", "native", "-f", "test_presets", "-v"], cwd=ROOT,
                         capture_output=True, text=True).stdout
    peaks = {}
    for line in out.splitlines():
        m = re.match(r"^(\S+)\s+[\d.]+\s+(-?[\d.]+)\s+(-?[\d.]+)", line)
        if m and m.group(1) != "PRESET": peaks[m.group(1)] = float(m.group(2))
    return peaks


def main():
    for rnd in range(5):
        g, peaks = gains(), bench()
        off = {k: TARGET - v for k, v in peaks.items() if abs(TARGET - v) > TOL}
        print("round %d: %d presets off" % (rnd, len(off)))
        if not off: break
        for k, d in off.items():  # soft clip: the last dB above -3 take more than 1 dB of gain
            g[k] = max(-24.0, min(24.0, g.get(k, 0.0) + (d * 1.3 if d < 0 else d)))
        write(g)
    for k, v in sorted(bench().items()): print("%-7s %6.1f dB  gain %+.1f" % (k, v, gains().get(k, 0.0)))


main()
