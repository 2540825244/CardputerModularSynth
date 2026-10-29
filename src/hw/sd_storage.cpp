#include "sd_storage.h"
#include <SD.h>
#include <SPI.h>

bool SdStorage::mount() {
    if (!spiStarted_) {
        // G5 is the Cap LoRa-1262's SX1262 NSS on the EXT header, on the same SPI bus. It floats
        // low after reset, so the radio answers on MISO and the card's CMD0 fails. A pull-up keeps
        // the radio deselected (harmless with no cap fitted).
        pinMode(5, INPUT_PULLUP);
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

void SdStorage::selfTest() {
    if (!ok_) {
        Serial.println("SD none");
        return;
    }
    static const char* kProbe = "/modsynth/probe.txt";
    const std::string want = "modsynth sd probe";
    std::string got;
    const bool rw = write(kProbe, want) && read(kProbe, got) && got == want;
    SD.remove(kProbe);
    Serial.printf("SD %s type=%d %u MB\n", rw ? "ok" : "FAIL rw", (int)SD.cardType(),
                  (unsigned)(SD.cardSize() >> 20));
}

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
    if (SD.exists(path)) SD.remove(path);  // remove() logs an error for a missing file
    return SD.rename(tmp.c_str(), path);
}

bool SdStorage::exists(const char* path) { return ok_ && SD.exists(path); }
