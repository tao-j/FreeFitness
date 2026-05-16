// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef ANT_PLUS_H
#define ANT_PLUS_H

#include <ANT.h>
#include <ANTPLUS.h>
#include "ant_plus_speed.h"
#include "encoder.h"
#include "../config.h"

class AntManager {
public:
    AntManager();
    void begin(HardwareSerial& serial, const ProfileConfig& config);
    void update(const Encoder& enc, uint8_t batteryLevel);
    void parse();
    void openChannel();
    void closeChannel();
    uint16_t getDeviceId() const;

private:
    static void manufacturerIDHandler(ManufacturersInformationMsg& msg, uintptr_t data);
    static void productIDHandler(ProductInformationMsg& msg, uintptr_t data);
    static void powerOnlyHandler(BicyclePowerStandardPowerOnlyMsg& msg, uintptr_t data);
    static void speedHandler(BicycleSpeedBaseMainDataPageMsg& msg, uintptr_t data);
    static void batteryStatusHandler(BatteryStatusMsg& msg, uintptr_t data);

    ArduinoSerialAntWithCallbacks _ant;
    AntPlusRouter _router;
    ProfileBicyclePowerSensor _bikePower;
    ProfileBicycleSpeedSensorImpl _bikeSpeed;

    const Encoder* _currentEnc = nullptr;
    uint8_t _batteryLevel = 100;
    static const uint8_t NETWORK_KEY[];
    ProfileConfig _config;
};

#endif
