# Flash budget (v1.3 Task 2): the app must fit the Launcher's 1 MB modsyn slot.
# - The audio path (DSP modules, engine, DaisySP, Plaits, the audio task, the bench) is built
#   exactly as before: -O2, flags untouched, so its object code (and the sound) cannot change.
# - Everything else (UI, render, file I/O, M5 libraries, Arduino core and its libraries) builds
#   with -Os and, for C++, -fno-exceptions. Nothing catches, so a throw ends in terminate() either
#   way; the unwind tables were ~50 KB of flash.
import os

Import("env")

ROOT = env.subst("$PROJECT_DIR")
AUDIO_DIRS = [os.path.join(ROOT, d) + os.sep for d in ("src/core", "src/bench", "lib/DaisySP", "lib/plaits")]
AUDIO_FILES = {os.path.join(ROOT, "src/hw/audio_out.cpp")}
# src/core files that only the UI task runs (load/save, presets, undo, editing, compiling).
UI_CORE = {os.path.join(ROOT, "src/core", f) for f in
           ("patch_io.cpp", "presets.cpp", "undo.cpp", "patch.cpp", "param.cpp", "program.cpp", "registry.cpp")}


def on_audio_path(path):
    if path in UI_CORE:
        return False
    return path in AUDIO_FILES or any(path.startswith(d) for d in AUDIO_DIRS)


def size_flags(env, node):
    if on_audio_path(node.srcnode().get_abspath()):
        return node
    cc = [f for f in env["CCFLAGS"] if f not in ("-O2", "-Os")] + ["-Os"]
    cxx = [f for f in env["CXXFLAGS"] if f != "-fexceptions"] + ["-fno-exceptions"]
    return env.Object(node, CCFLAGS=cc, CXXFLAGS=cxx)


env.AddBuildMiddleware(size_flags)
