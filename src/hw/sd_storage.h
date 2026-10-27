#pragma once
#include <cstdint>
#include "core/storage.h"

// SD on SPI G40/G39/G14, CS G12.
class SdStorage : public Storage {
   public:
    bool begin();  // mounts and creates /modsynth/patches
    bool available() override;  // while unmounted, re-attempts begin() at most every 5 s
    bool read(const char* path, std::string& out) override;
    bool write(const char* path, const std::string& data) override;  // tmp + rename
    bool exists(const char* path) override;

   private:
    bool mount();
    bool ok_ = false;
    bool spiStarted_ = false;
    uint32_t lastTryMs_ = 0;
};
