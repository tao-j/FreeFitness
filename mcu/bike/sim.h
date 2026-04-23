#ifndef SIM_H
#define SIM_H

#include <Arduino.h>
#include "../bridge.h"

// Shared source-of-truth state for all sinks (BLE, ANT, UI). Mirrors the
// Python BikeState (linux/bike/__init__.py) in structure, with wire-ready
// uint types and the MCU-specific tx/config/status fields appended.
//
// Rotational state (crank, wheel) is stored in the event-view: cumulative
// revolution count + event tick (1/1024 s). Rates (cadence, speed, power)
// are latched scalars. Whichever half a source natively measures, the
// encoder-side RateEventBridge in the source fills in the other half
// before writing here, so every tx consumer gets a fully populated view.
//
// Wire convention: wheelEventTime is stored in 1/2048 s units (BLE CP's
// native encoding); ANT+ and CSC paths halve it. This is the one place
// the pipeline's internal 1/1024 tick is pre-scaled for wire convenience.
struct BikeData {
    uint16_t power = 0;
    uint8_t cadence = 0;
    uint16_t accPower = 0;
    uint8_t eventCount = 0;
    float speed_mps = 0.0f;   // for UI / telemetry; not on the BLE/ANT wire directly

    // Rotation events
    uint32_t wheelRevs = 0;
    uint16_t wheelEventTime = 0;  // 1/2048 s
    uint16_t crankRevs = 0;
    uint16_t crankEventTime = 0;  // 1/1024 s

    // Silence detection (compare against get_tick_now())
    uint32_t lastUpdateTick = 0;

    // Status (not port-identical to Python BikeState)
    bool bleConnected = false;
    uint8_t antMaxChannels = 0;
    uint8_t antMaxNetworks = 0;
    uint8_t batteryLevel = 100;

    // Source Tracking
    bool isSimMode = true;
    const char* sourceName = "SIM";
    uint32_t lastDataTime = 0;    // legacy wall-clock ms; UI/signal-timeout code still reads this
    uint16_t targetBikeId = 0;    // 0 = Any bike
};


// Simulates a hardware hall-effect crank sensor plus a power meter.
//
// Crank: emits one event per revolution (hall-sensor style), exercising
//        the bridge's event → rate derivation for cadence.
// Wheel: no direct measurement — derived from power via power_to_speed()
//        and integrated by the wheel bridge.
//
// rpm and power follow slow random walks so the signal varies realistically
// rather than ramping in a saw-tooth.
class Simulator {
public:
    void update(uint32_t dt_ms);
    BikeData& getMutableData() { return _data; }

private:
    BikeData _data;

    // Source-owned bridges. A real hall sensor on MCU would feed the crank
    // bridge via interrupts; here update() emulates that cadence.
    RateEventBridge _crank_bridge;
    RateEventBridge _wheel_bridge;

    float _rpm = 70.0f;
    float _power_w = 150.0f;
    int32_t _time_to_next_rev_ms = 0;
};

#endif
