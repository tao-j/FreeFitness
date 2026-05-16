// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef UNIFIED_SCANNER_H
#define UNIFIED_SCANNER_H

#include <NimBLEDevice.h>
#include "../source_config.h"

// Forward declarations: subscribers receive parsed advertisements from the
// scanner. Kept as forward decls to avoid include cycles.
class KeiserScanner;
class CyclePowerScanner;

struct ScanEntry {
    BikeType type = TARGET_NONE;
    char     address[18] = "";
    char     name[22] = "";
    uint8_t  m3BikeId = 0;        // valid when type == TARGET_M3
    int8_t   rssi = -127;
    uint32_t lastSeen = 0;
};

// One NimBLE scanner, two filters (M3 + CP). Always-on while mode == BLE.
// Maintains the merged scan list the Sensor Picker reads from, and dispatches
// every recognized advertisement to the M3 and CP subscribers (each gates
// internally on whether it should act for the active target).
class UnifiedScanner : public NimBLEScanCallbacks {
public:
    static constexpr uint8_t MAX_ENTRIES = 6;

    void begin();
    void stop();
    bool isRunning() const { return _pScan && _pScan->isScanning(); }

    void attach(KeiserScanner* keiser, CyclePowerScanner* cp);

    uint8_t deviceCount() const { return _entryCount; }
    const ScanEntry& entry(uint8_t i) const { return _entries[i]; }

    // Picker actions — modify cfg directly. UI persists to Preferences.
    void saveBrowsed(uint8_t index, SourceConfig& cfg);
    void clearTarget(SourceConfig& cfg);

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;

private:
    NimBLEScan* _pScan = nullptr;
    KeiserScanner* _keiser = nullptr;
    CyclePowerScanner* _cp = nullptr;
    ScanEntry _entries[MAX_ENTRIES];
    uint8_t _entryCount = 0;
    uint32_t _lastAnyAdvertLog = 0;

    uint8_t upsert(BikeType type, const char* address, const char* name, uint8_t m3BikeId, int8_t rssi);
    bool tryParseM3(const NimBLEAdvertisedDevice* dev, uint8_t& outBikeId, const uint8_t** outPayload) const;
};

#endif
