// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef ENCODER_H
#define ENCODER_H

#include "../bike/bike_data.h"
#include "../bridge.h"

// Translation layer between BikeData (telemetry) and the tx layers
// (BLE GATT, ANT+). Mirrors linux/tx/encoder.py::ProtocolEncoder.
//
// Owns the rate↔event bridges and the ANT+ power-page accumulators —
// these used to be split across the source classes and BikeData. Wire
// scaling (BLE CP's 1/2048 s wheel event time) lives here too.
//
// update() is called once per tx tick from main loop. Silence detection
// gates output: if BikeData.last_update_tick hasn't advanced within
// SILENCE_TICKS, noData() returns true and tx layers skip emission.
class Encoder {
public:
    void update(const BikeData& data);

    bool noData() const { return _noData; }

    uint16_t power_w() const { return _power_w; }
    uint8_t  cadence_rpm() const { return _cadence_rpm; }
    float    speed_mps() const { return _speed_mps; }

    uint32_t crank_revs() const { return _crank_revs; }
    uint32_t wheel_revs() const { return _wheel_revs; }

    uint16_t crank_event_tick_1024() const { return _crank_event_tick_1024; }
    uint16_t wheel_event_tick_1024() const { return _wheel_event_tick_1024; }
    uint16_t wheel_event_tick_2048() const { return _wheel_event_tick_2048; }

    uint16_t accPower() const { return _accPower; }
    uint8_t  powerEventCount() const { return _powerEventCount; }

private:
    RateEventBridge _crank, _wheel;
    bool     _noData = true;
    uint16_t _power_w = 0;
    uint8_t  _cadence_rpm = 0;
    float    _speed_mps = 0.0f;
    uint32_t _crank_revs = 0;
    uint32_t _wheel_revs = 0;
    uint16_t _crank_event_tick_1024 = 0;
    uint16_t _wheel_event_tick_1024 = 0;
    uint16_t _wheel_event_tick_2048 = 0;
    uint16_t _accPower = 0;
    uint8_t  _powerEventCount = 0;
};

#endif
