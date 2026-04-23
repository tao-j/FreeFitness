#ifndef BRIDGE_H
#define BRIDGE_H

#include <Arduino.h>

// Time base and rate/event conversion primitives for the pipeline.
//
// The canonical time unit is the **tick** — 1/1024 second, matching the BLE
// CSC/CP crank event-time wire encoding (and the ANT+ event times). All
// `*_tick` variables carry this unit; derive seconds only where physics
// needs them (power_to_speed, dt integration) by dividing by 1024.
//
// Note: BLE Cycling Power wheel event time is encoded as 1/2048 s on the
// wire (spec quirk). The tx layer scales internal ticks at assignment time
// where needed; internal state stays 1/1024.

static inline uint32_t get_tick_now() {
    // millis() in 1/1024 s. Wraps at ~48.5 days; deltas within a session
    // are safe even across the wraparound thanks to unsigned arithmetic.
    return (uint32_t)(((uint64_t)millis() * 1024) / 1000);
}

float power_to_speed(float power_w);


// Bidirectional bridge between the rate view and event view of a rotating
// channel (crank or wheel).
//
//   - rate view:  revolutions per second (continuous)
//   - event view: cumulative revolution count + event tick (discrete)
//
// A source populates whichever half it natively measures — this bridge
// fills in the other half so the tx layer always has both.
//
// Use one of two feeds per tick:
//   - feed_rate(rate_rps, now_tick):  integrate forward; back-interpolate
//     an event tick when the running count crosses the next integer.
//   - feed_event(cum_revs, event_tick, now_tick): adopt hardware events;
//     derive the rate from Δcount / Δtick across consecutive events.
//
// Rate unit is revs/sec. Callers convert to the channel's natural unit
// (rpm for crank: rps * 60; m/s for wheel: rps * WHEEL_CIRCUMFERENCE).
class RateEventBridge {
public:
    void feed_rate(float rate_rps, uint32_t now_tick);
    void feed_event(uint32_t cum_revs, uint32_t event_tick, uint32_t now_tick);

    uint32_t count_int() const { return _count_int; }
    uint32_t event_tick() const { return _event_tick; }
    float rate_rps() const { return _rate_rps; }

private:
    float _count_float = 0.0f;
    uint32_t _count_int = 0;
    uint32_t _event_tick = 0;
    float _rate_rps = 0.0f;

    uint32_t _last_feed_tick = 0;
    uint32_t _last_event_revs = 0;
    uint32_t _last_event_tick = 0;

    void _interpolate_event(uint32_t now_tick);
};

#endif
