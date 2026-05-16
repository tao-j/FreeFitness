// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef BIKE_DATA_H
#define BIKE_DATA_H

#include <Arduino.h>

// Pure telemetry, port of linux/bike/__init__.py::BikeState.
// Sources stamp whichever fields they natively measure; the Encoder fills in
// the missing half (rate from events, or events from rate) for tx.
//
// All event ticks are 1/1024 s. Wire scaling (BLE CP's 1/2048 s) lives in
// the Encoder, not here. 0 / false / 0.0 = "unknown" — we don't have NaN on
// this MCU, so the encoder gates on the has_*_event flags and on power_w > 0.
struct BikeData {
    float    cadence_rpm = 0.0f;     // crank rate (if no event)
    float    speed_mps = 0.0f;       // wheel rate (if no event and no power)
    uint16_t power_w = 0;

    bool     has_crank_event = false;
    uint32_t crank_revs = 0;
    uint32_t crank_event_tick = 0;   // 1/1024 s

    bool     has_wheel_event = false;
    uint32_t wheel_revs = 0;
    uint32_t wheel_event_tick = 0;   // 1/1024 s — NOT pre-scaled for CP

    uint32_t last_update_tick = 0;
};

#endif
