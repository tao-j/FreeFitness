// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "encoder.h"

static constexpr float WHEEL_CIRCUMFERENCE = 2.096f;  // 700c × 25 (m)
static constexpr uint32_t SILENCE_TICKS = 2 * 1024;   // 2 s @ 1/1024 s/tick

void Encoder::update(const BikeData& data) {
    uint32_t now_tick = get_tick_now();

    if (data.last_update_tick == 0 ||
        (now_tick - data.last_update_tick) > SILENCE_TICKS) {
        _noData = true;
        return;
    }
    _noData = false;

    // Crank: events take priority, else rate.
    if (data.has_crank_event) {
        _crank.feed_event(data.crank_revs, data.crank_event_tick, now_tick);
    } else if (data.cadence_rpm > 0.0f) {
        _crank.feed_rate(data.cadence_rpm / 60.0f, now_tick);
    }

    // Wheel: events > explicit speed > power-derived speed.
    float speed_mps = data.speed_mps;
    if (data.has_wheel_event) {
        _wheel.feed_event(data.wheel_revs, data.wheel_event_tick, now_tick);
        speed_mps = _wheel.rate_rps() * WHEEL_CIRCUMFERENCE;
    } else {
        if (speed_mps == 0.0f && data.power_w > 0) {
            speed_mps = power_to_speed(data.power_w);
        }
        if (speed_mps > 0.0f) {
            _wheel.feed_rate(speed_mps / WHEEL_CIRCUMFERENCE, now_tick);
        }
    }

    // Latch protocol-ready fields.
    _cadence_rpm = (uint8_t)constrain(_crank.rate_rps() * 60.0f, 0.0f, 255.0f);
    _crank_revs = _crank.count_int();
    _crank_event_tick_1024 = (uint16_t)_crank.event_tick();

    _speed_mps = speed_mps;
    _wheel_revs = _wheel.count_int();
    _wheel_event_tick_1024 = (uint16_t)_wheel.event_tick();
    _wheel_event_tick_2048 = (uint16_t)(_wheel.event_tick() * 2);

    // Power: pass-through + ANT+ accumulators. Increments once per encoder
    // tick (4 Hz from the main loop), independent of source rate. ANT+ avg
    // power = ΔaccPower / ΔeventCount, so the absolute rate doesn't matter
    // as long as both fields advance together.
    if (data.power_w > 0) {
        _power_w = data.power_w;
        _powerEventCount++;
        _accPower += data.power_w;
    }
}
