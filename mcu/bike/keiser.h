// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef KEISER_H
#define KEISER_H

#include <NimBLEDevice.h>
#include "sim.h"
#include "../bridge.h"

// Scans for Keiser M3i BLE advertisements. The bike broadcasts cadence and
// power as instantaneous rates; this class feeds the crank/wheel bridges
// with those rates so the downstream tx layers receive fully synthesized
// events (same model as the Python KeiserBike source).
class KeiserScanner : public NimBLEScanCallbacks {
public:
    void begin(BikeData& data);
    void update();

    // NimBLEScanCallbacks
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;

private:
    BikeData* _pData = nullptr;
    NimBLEScan* _pScan = nullptr;

    // Source-owned bridges: scanner publishes rates (cadence, power→speed),
    // bridges synthesize the events that CSC/CP/ANT payloads require.
    RateEventBridge _crank_bridge;
    RateEventBridge _wheel_bridge;
};

#endif
