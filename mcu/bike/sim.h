#ifndef SIM_H
#define SIM_H

#include <Arduino.h>

struct BikeData {
    uint16_t power = 0;
    uint8_t cadence = 0;
    uint16_t accPower = 0;
    uint8_t eventCount = 0;
    
    // For BLE CSC
    uint32_t wheelRevs = 0;
    uint16_t wheelEventTime = 0;
    uint16_t crankRevs = 0;
    uint16_t crankEventTime = 0;

    // Status
    bool bleConnected = false;
    uint8_t antMaxChannels = 0;
    uint8_t antMaxNetworks = 0;
    uint8_t batteryLevel = 100;

    // Source Tracking
    bool isSimMode = true;
    const char* sourceName = "SIM";
    uint32_t lastDataTime = 0;
    uint16_t targetBikeId = 0; // 0 = Any bike
};

class Simulator {
public:
    void update(uint32_t dt_ms);
    BikeData& getMutableData() { return _data; }

private:
    BikeData _data;
    float _crankRes = 0;
    float _wheelRes = 0;
    float _curPower = 60.0f;
    float _curCadence = 60.0f;
    bool _rampUp = true;
};

#endif
