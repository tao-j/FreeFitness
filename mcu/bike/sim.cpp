#include "sim.h"

void Simulator::update(uint32_t dt_ms) {
    uint32_t now = millis();
    
    // 1. Basic Power/Cadence Simulation (Ramping 60-120W, 60-100RPM over 30s)
    float powerRange = 60.0f;
    float cadenceRange = 40.0f;
    float rampTimeS = 30.0f;

    float powerStep = (powerRange / rampTimeS) * (dt_ms / 1000.0f);
    float cadenceStep = (cadenceRange / rampTimeS) * (dt_ms / 1000.0f);

    if (_rampUp) {
        _curPower += powerStep;
        _curCadence += cadenceStep;
        if (_curPower >= 120.0f) {
            _curPower = 120.0f;
            _rampUp = false;
        }
    } else {
        _curPower -= powerStep;
        _curCadence -= cadenceStep;
        if (_curPower <= 60.0f) {
            _curPower = 60.0f;
            _rampUp = true;
        }
    }

    // Add small jitter for realism
    _data.power = (uint16_t)(_curPower + (esp_random() % 5) - 2);
    _data.cadence = (uint8_t)(_curCadence + (esp_random() % 3) - 1);
    _data.accPower += _data.power;
    _data.eventCount++;
    
    // 2. Crank Revolutions (BLE CSC)
    float crankInc = (_data.cadence / 60.0f) * (dt_ms / 1000.0f);
    _crankRes += crankInc;
    if (_crankRes >= 1.0f) {
        uint16_t revs = (uint16_t)_crankRes;
        _data.crankRevs += revs;
        _crankRes -= revs;
        _data.crankEventTime = (uint16_t)(now * 1024 / 1000); 
    }

    // 3. Wheel Revolutions (BLE CSC)
    float speed = 4.0f + (_curPower - 60.0f) * (4.0f / 60.0f); // 4-8 m/s (~14-28 km/h)
    float wheelCircumference = 2.096f; 
    float wheelInc = (speed / wheelCircumference) * (dt_ms / 1000.0f); 
    _wheelRes += wheelInc;
    if (_wheelRes >= 1.0f) {
        uint32_t revs = (uint32_t)_wheelRes;
        _data.wheelRevs += revs;
        _wheelRes -= revs;
        _data.wheelEventTime = (uint16_t)(now * 2048 / 1000);
    }
}
