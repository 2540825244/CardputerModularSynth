#pragma once
#include "core/engine.h"

// Installs the I2S driver (no MCLK) and starts the audio task on core 1. Core 0 must then call
// codecBegin() (I2C is core-0 only). Returns false if the driver could not be installed.
bool audioBegin(Engine&);
void codecBegin();  // ES8311 register sequence; call from the core-0 side after audioBegin
int audioI2sError();  // first esp_err_t seen by the audio path, 0 if none
int codecFailures();  // I2C register writes that failed
