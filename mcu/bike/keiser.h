// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef KEISER_H
#define KEISER_H

#include <NimBLEDevice.h>
#include "bike_data.h"
#include "../source_config.h"

// M3 strategy: parses Keiser M3i manufacturer-data advertisements into the
// shared BikeData. Receives advertisements from UnifiedScanner — no longer
// owns the NimBLE scan singleton. Active only when SourceConfig.mode ==
// SOURCE_BLE && targetType == TARGET_M3 && both address and bike ID match
// the saved target.
class KeiserScanner {
public:
    void begin(BikeData& data, const SourceConfig& cfg);
    void stop();
    void onAdvertisement(const NimBLEAdvertisedDevice* advertisedDevice,
                         uint8_t m3BikeId, const uint8_t* payload);

private:
    BikeData* _pData = nullptr;
    const SourceConfig* _pCfg = nullptr;
};

#endif
