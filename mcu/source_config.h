// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef SOURCE_CONFIG_H
#define SOURCE_CONFIG_H

#include <Arduino.h>

// User-facing source mode. Internal bike-type (M3 vs CP) is per-target
// metadata in `targetType`, set by the Sensor Picker when a device is
// selected — not exposed as a top-level mode.
enum BikeSourceMode : uint8_t {
    SOURCE_SIM = 0,
    SOURCE_BLE = 1,
};

// Active strategy under SOURCE_BLE.
enum BikeType : uint8_t {
    TARGET_NONE = 0,  // BLE mode but no pick yet — scanner runs, no auto-connect
    TARGET_M3 = 1,    // Keiser M3 manufacturer-data (scan-only, no GATT link)
    TARGET_CP = 2,    // BLE Cycling Power service (GATT client + subscribe)
};

// What source is selected and how it's targeted. Persists to Preferences,
// except `connectedAddress` which is live runtime state.
struct SourceConfig {
    BikeSourceMode mode = SOURCE_SIM;
    BikeType targetType = TARGET_NONE;
    uint16_t keiserBikeId = 0;        // co-written with bleAddress when targetType == M3
    char     bleAddress[18] = "";
    char     bleName[22] = "";
    char     connectedAddress[18] = ""; // runtime, not persisted
};

#endif
