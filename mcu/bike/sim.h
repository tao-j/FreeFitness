// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef SIM_H
#define SIM_H

#include <Arduino.h>
#include "bike_data.h"

// Simulates a hardware hall-effect crank sensor plus a power meter.
//
// Crank: emits one event per revolution (hall-sensor style) — exercises the
// Encoder's event→rate derivation for cadence.
// Wheel: no direct measurement — the Encoder derives speed from power via
// power_to_speed() and integrates with the wheel bridge.
//
// rpm and power follow slow random walks so the signal varies realistically
// rather than ramping in a saw-tooth.
class Simulator {
public:
    void update(BikeData& data, uint32_t dt_ms);

private:
    float _rpm = 70.0f;
    float _power_w = 150.0f;
    int32_t _time_to_next_rev_ms = 0;
};

#endif
