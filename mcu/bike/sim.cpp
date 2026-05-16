// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "sim.h"
#include "../bridge.h"
#include <esp_random.h>

static inline float rand_pm(float amplitude) {
    // uniform in [-amplitude, +amplitude]
    return (float)((int32_t)(esp_random() % 2001) - 1000) / 1000.0f * amplitude;
}

void Simulator::update(BikeData& data, uint32_t dt_ms) {
    uint32_t now_tick = get_tick_now();

    // Slow random walks for rpm and power — smooth drift between pedal strokes.
    _rpm = constrain(_rpm + rand_pm(0.6f), 50.0f, 95.0f);
    _power_w = constrain(_power_w + rand_pm(4.0f), 80.0f, 240.0f);

    // Crank: hall-sensor emulation. Fire one event each time the revolution
    // period elapses; back-interpolate so the event tick isn't snapped to
    // the coarse update cadence.
    _time_to_next_rev_ms -= (int32_t)dt_ms;
    if (_time_to_next_rev_ms <= 0) {
        int32_t overshoot_ms = -_time_to_next_rev_ms;
        uint32_t event_tick = now_tick - (uint32_t)((int64_t)overshoot_ms * 1024 / 1000);
        data.crank_revs++;
        data.crank_event_tick = event_tick;
        _time_to_next_rev_ms += (int32_t)(60000.0f / _rpm);
    }

    // Stamp native fields. The Encoder derives cadence rate from the event
    // stream and wheel speed from power.
    data.has_crank_event = true;
    data.has_wheel_event = false;
    data.power_w = (uint16_t)_power_w;
    data.last_update_tick = now_tick;
}
