#pragma once
#include <cstdint>

// Small, pure animation helpers for the renderer (natively tested).
float ease(float from, float to, float dtMs, float tauMs);  // one-pole step towards `to`
float zoomT(uint32_t sinceMs, uint32_t nowMs);              // 0 -> 1 over kZoomMs, smoothstep
float flashT(uint32_t sinceMs, uint32_t nowMs, uint32_t durMs);  // 1 -> 0 over durMs, 0 after
constexpr uint32_t kZoomMs = 140;
