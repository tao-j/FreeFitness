#include "sim.h"
#include <esp_random.h>

static constexpr float WHEEL_CIRCUMFERENCE = 2.096f;  // 700c × 25 (m)

static inline float rand_pm(float amplitude) {
    // uniform in [-amplitude, +amplitude]
    return (float)((int32_t)(esp_random() % 2001) - 1000) / 1000.0f * amplitude;
}

void Simulator::update(uint32_t dt_ms) {
    uint32_t now_tick = get_tick_now();

    // Slow random walks for rpm and power — smooth drift between pedal strokes.
    _rpm = constrain(_rpm + rand_pm(0.6f), 50.0f, 95.0f);
    _power_w = constrain(_power_w + rand_pm(4.0f), 80.0f, 240.0f);

    // Crank: hall-sensor emulation. Fire one event each time the revolution
    // period elapses, feeding the bridge so cadence is derived from
    // Δcount / Δtick (same as the Python sim).
    _time_to_next_rev_ms -= (int32_t)dt_ms;
    if (_time_to_next_rev_ms <= 0) {
        _data.crankRevs++;
        _crank_bridge.feed_event(_data.crankRevs, now_tick, now_tick);
        _data.crankEventTime = (uint16_t)now_tick;
        _time_to_next_rev_ms += (int32_t)(60000.0f / _rpm);
    }

    // Wheel: no direct measurement. Derive speed from power, feed the
    // wheel bridge continuously so it integrates revolutions and produces
    // back-interpolated event ticks.
    float speed_mps = power_to_speed(_power_w);
    _wheel_bridge.feed_rate(speed_mps / WHEEL_CIRCUMFERENCE, now_tick);
    _data.wheelRevs = _wheel_bridge.count_int();
    _data.wheelEventTime = (uint16_t)(_wheel_bridge.event_tick() * 2);  // 1/1024 → 1/2048

    // Latch scalars for tx consumers.
    _data.power = (uint16_t)_power_w;
    _data.cadence = (uint8_t)(_crank_bridge.rate_rps() * 60.0f);
    _data.speed_mps = speed_mps;

    // ANT+ power-page accumulators.
    _data.accPower += _data.power;
    _data.eventCount++;

    _data.lastUpdateTick = now_tick;
    _data.lastDataTime = millis();  // legacy field still consulted by main loop
}
