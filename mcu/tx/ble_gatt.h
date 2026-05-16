// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <NimBLEDevice.h>
#include "../system_status.h"
#include "../config.h"
#include "encoder.h"

class BleManager {
public:
    void begin(SystemStatus& status, const ProfileConfig& config);
    void update(const Encoder& enc);
    void updateBattery(uint8_t level);
    void startAdvertising();
    void stopAdvertising();

private:
    NimBLECharacteristic* _pCPMeasurement = nullptr;
    NimBLECharacteristic* _pCSCMeasurement = nullptr;
    NimBLECharacteristic* _pBatLevel = nullptr;

    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        ServerCallbacks(SystemStatus& status) : _status(status) {}
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    private:
        SystemStatus& _status;
    };
};

#endif
