// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <Arduino.h>

// MCU runtime state. Not persisted.
//
// `sensorConnected` is the same liveness signal Python uses (see
// linux/tx/encoder.py SILENCE_TICKS gate): true while fresh data arrives
// within the silence window, false after. Derived each main-loop tick by
// comparing bikeData.last_update_tick to get_tick_now().
struct SystemStatus {
    bool    sensorConnected = false; // upstream: source producing fresh data
    bool    bleConnected = false;    // downstream: phone/head-unit paired
    uint8_t batteryLevel = 100;
};

#endif
