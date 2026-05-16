// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef CYCLE_POWER_H
#define CYCLE_POWER_H

#include <NimBLEDevice.h>
#include <NimBLERemoteCharacteristic.h>
#include "bike_data.h"
#include "../source_config.h"

// CP strategy: scans (via UnifiedScanner), connects as BLE central to the
// upstream Cycling Power sensor, subscribes to CP Measurement notifications,
// and parses the payload into the shared BikeData. Active only when
// SourceConfig.mode == SOURCE_BLE && targetType == TARGET_CP and a matching
// device appears.
class CyclePowerScanner : public NimBLEClientCallbacks {
public:
    void begin(BikeData& data, SourceConfig& cfg);
    void stop();
    void update();
    void onAdvertisement(const NimBLEAdvertisedDevice* advertisedDevice);

    bool connected() const { return _connected; }
    bool connecting() const { return _connecting; }

    void onConnect(NimBLEClient* pClient) override;
    void onConnectFail(NimBLEClient* pClient, int reason) override;
    void onDisconnect(NimBLEClient* pClient, int reason) override;

private:
    BikeData* _pData = nullptr;
    SourceConfig* _pCfg = nullptr;
    NimBLEClient* _pClient = nullptr;
    const NimBLEAdvertisedDevice* _pPendingConnect = nullptr;
    bool _connected = false;
    bool _connecting = false;
    bool _enabled = false;
    uint32_t _lastConnectAttempt = 0;
    uint32_t _lastStatusLog = 0;

    bool targetMatches(const char* address, const char* name) const;
    void connectDevice(const NimBLEAdvertisedDevice* advertisedDevice);
    bool subscribeMeasurement();
    void parseMeasurement(uint8_t* bytes, size_t len);

    static void notifyCallback(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify);
    static CyclePowerScanner* _instance;
};

#endif
