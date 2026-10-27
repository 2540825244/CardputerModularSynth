#pragma once
#include "core/storage.h"

// SD on SPI G40/G39/G14, CS G12.
class SdStorage : public Storage {
   public:
    bool begin();  // mounts and creates /modsynth/patches
    bool available() override { return ok_; }
    bool read(const char* path, std::string& out) override;
    bool write(const char* path, const std::string& data) override;  // tmp + rename
    bool exists(const char* path) override;

   private:
    bool ok_ = false;
};
