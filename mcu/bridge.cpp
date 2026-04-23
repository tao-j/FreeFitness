#include "bridge.h"
#include <math.h>


void RateEventBridge::feed_rate(float rate_rps, uint32_t now_tick) {
    if (_last_feed_tick == 0) {
        _last_feed_tick = now_tick;
        _event_tick = now_tick;
        _rate_rps = rate_rps;
        return;
    }
    float dt_sec = (float)(now_tick - _last_feed_tick) / 1024.0f;
    _last_feed_tick = now_tick;
    _count_float += rate_rps * dt_sec;
    _rate_rps = rate_rps;
    _interpolate_event(now_tick);
}

void RateEventBridge::feed_event(uint32_t cum_revs, uint32_t event_tick,
                                 uint32_t now_tick) {
    _last_feed_tick = now_tick;
    if (event_tick != _last_event_tick && cum_revs > _last_event_revs) {
        uint32_t dt_tick = event_tick - _last_event_tick;
        if (dt_tick > 0 && _last_event_tick != 0) {
            uint32_t d_revs = cum_revs - _last_event_revs;
            _rate_rps = (float)d_revs * 1024.0f / (float)dt_tick;
        }
        _last_event_revs = cum_revs;
        _last_event_tick = event_tick;
    }
    _count_int = cum_revs;
    _count_float = (float)cum_revs;
    _event_tick = event_tick;
}

void RateEventBridge::_interpolate_event(uint32_t now_tick) {
    float diff = _count_float - (float)_count_int;
    if (diff >= 1.0f) {
        uint32_t dt_tick = now_tick - _event_tick;
        float frac_over = (diff - floorf(diff)) / diff;
        _event_tick = now_tick - (uint32_t)((float)dt_tick * frac_over);
        _count_int = (uint32_t)_count_float;
    }
}


float power_to_speed(float power_w) {
    // Solve P_drag·v^3 + P_roll·v − P = 0 for v (m/s).
    const float Cd = 0.9f;
    const float A = 0.5f;
    const float rho = 1.225f;
    const float Crr = 0.0045f;
    const float F_gravity = 75.0f * 9.81f;

    const float coeff_P_drag = 0.5f * Cd * A * rho;
    const float coeff_P_roll = Crr * F_gravity;

    float p = coeff_P_roll / coeff_P_drag;
    float q = -power_w / coeff_P_drag;

    float delta = p * p * p / 27.0f + q * q / 4.0f;
    float sqrt_delta = sqrtf(delta);

    auto cbrt_signed = [](float x) -> float {
        return (x >= 0.0f) ? cbrtf(x) : -cbrtf(-x);
    };

    float u = cbrt_signed(-q / 2.0f + sqrt_delta);
    float v = cbrt_signed(-q / 2.0f - sqrt_delta);
    return u + v;
}
