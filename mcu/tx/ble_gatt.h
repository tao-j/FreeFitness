// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <NimBLEDevice.h>
#include "bike/sim.h"
#include "config.h"

class BleManager {
public:
    void begin(BikeData& data, const ProfileConfig& config);
    void update(BikeData& data);
    void updateBattery(uint8_t level);
    void startAdvertising();
    void stopAdvertising();

private:
    NimBLECharacteristic* _pCPMeasurement;
    NimBLECharacteristic* _pCSCMeasurement;
    NimBLECharacteristic* _pBatLevel;
    
    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        ServerCallbacks(BikeData& data) : _data(data) {}
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    private:
        BikeData& _data;
    };
};

#endif
