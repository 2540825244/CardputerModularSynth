#include "sd_storage.h"
#include <SD.h>
#include <SPI.h>

bool SdStorage::mount() {
    if (!spiStarted_) {
        SPI.begin(40, 39, 14, 12);
        spiStarted_ = true;
    } else {
        SD.end();
    }
    lastTryMs_ = millis();
    ok_ = SD.begin(12, SPI, 25000000);
    if (ok_) {
        SD.mkdir("/modsynth");
        SD.mkdir("/modsynth/patches");
    }
    return ok_;
}

bool SdStorage::begin() { return mount(); }

bool SdStorage::available() {
    if (!ok_ && millis() - lastTryMs_ >= 5000) mount();
    return ok_;
}

bool SdStorage::read(const char* path, std::string& out) {
    if (!ok_) return false;
    if (!SD.exists(path)) return false;  // a missing file is normal, not a card fault
    File f = SD.open(path, FILE_READ);
    if (!f) {
        ok_ = false;
        return false;
    }
    out.clear();
    out.reserve(f.size());
    char buf[256];
    int n;
    while ((n = f.read((uint8_t*)buf, sizeof buf)) > 0) out.append(buf, n);
    f.close();
    return true;
}

bool SdStorage::write(const char* path, const std::string& data) {
    if (!ok_) return false;
    const String tmp = String(path) + ".tmp";
    File f = SD.open(tmp.c_str(), FILE_WRITE);
    if (!f) {
        ok_ = false;
        return false;
    }
    const size_t w = f.write((const uint8_t*)data.data(), data.size());
    f.close();
    if (w != data.size()) {
        SD.remove(tmp.c_str());
        return false;
    }
    SD.remove(path);
    return SD.rename(tmp.c_str(), path);
}

bool SdStorage::exists(const char* path) { return ok_ && SD.exists(path); }
