#pragma once
#include <cstdint>

constexpr float kSampleRate = 48000.f;
constexpr int kBlock = 32;
constexpr float kC4Hz = 261.6256f;
constexpr float kAudioVolts = 5.f;
constexpr float kGateHighVolts = 5.f, kGateOn = 1.0f, kGateOff = 0.5f;
constexpr int kPages = 3, kGridCols = 12, kGridRows = 4, kCellsPerPage = 48, kMaxModules = 32,
              kMaxCables = 64, kMaxParams = 8, kMaxIns = 4, kMaxOuts = 4, kNumCableColors = 6,
              kUndoDepth = 10;
constexpr uint32_t kHoldMs = 400;
