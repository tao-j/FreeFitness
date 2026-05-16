// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
//
// Cycling Power strategy.
//
// The scan list and adv-filtering now live in UnifiedScanner; this class
// receives already-CP-classified advertisements via onAdvertisement(), then
// gates a connect attempt on target match. Once connected, it owns the
// GATT client lifecycle (subscribe, parse, disconnect-on-failure).
//
// Important design notes (learned the hard way; do not regress):
//
//   1. **Defer connect() to the main loop, never call it from the scan
//      callback.** onAdvertisement() runs inside the NimBLE scan callback
//      (via UnifiedScanner::onResult). Calling NimBLEClient::connect() there
//      races the host stack. We stash the device pointer in _pPendingConnect
//      and let update() drain it on the next tick.
//
//   2. **NimBLEClient::setConnectTimeout() is in milliseconds**, not seconds.
//
//   3. Recreate the client between attempts so stale GAP state from a failed
//      prior attempt can't bleed through and short-circuit the next connect
//      with BLE_HS_EDONE.

#include "cycle_power.h"
#include <NimBLERemoteService.h>
#include "../bridge.h"

static const NimBLEUUID CP_SERVICE_UUID("1818");
static const NimBLEUUID CP_MEASUREMENT_UUID("2A63");

CyclePowerScanner* CyclePowerScanner::_instance = nullptr;

static uint16_t read_u16(const uint8_t* bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static int16_t read_s16(const uint8_t* bytes) {
    return (int16_t)read_u16(bytes);
}

static uint32_t read_u32(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

void CyclePowerScanner::begin(BikeData& data, SourceConfig& cfg) {
    _pData = &data;
    _pCfg = &cfg;
    _instance = this;
    _enabled = true;
}

void CyclePowerScanner::stop() {
    _enabled = false;
    if (_pClient && _pClient->isConnected()) _pClient->disconnect();
    _connected = false;
    _connecting = false;
    if (_pCfg) _pCfg->connectedAddress[0] = '\0';
}

void CyclePowerScanner::update() {
    if (!_enabled || !_pCfg) return;
    if (_pCfg->mode != SOURCE_BLE || _pCfg->targetType != TARGET_CP) return;

    if (_pPendingConnect && !_connected && !_connecting) {
        const NimBLEAdvertisedDevice* dev = _pPendingConnect;
        _pPendingConnect = nullptr;
        connectDevice(dev);
        return;
    }

    if (_connected || _connecting) return;
    if (millis() - _lastStatusLog > 5000) {
        Serial.printf("BLE CP: waiting target=%s name=%s\n",
                      _pCfg->bleAddress, _pCfg->bleName);
        _lastStatusLog = millis();
    }
}

void CyclePowerScanner::onAdvertisement(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (!_enabled || !_pCfg) return;
    if (_pCfg->mode != SOURCE_BLE || _pCfg->targetType != TARGET_CP) return;

    std::string address = advertisedDevice->getAddress().toString();
    std::string name = advertisedDevice->haveName() ? advertisedDevice->getName() : "";
    if (!targetMatches(address.c_str(), name.c_str())) return;
    if (_connected || _connecting) return;
    if (!advertisedDevice->isConnectable()) return;
    if (millis() - _lastConnectAttempt < 5000) return;

    _pPendingConnect = advertisedDevice;
}

bool CyclePowerScanner::targetMatches(const char* address, const char* name) const {
    if (!_pCfg) return false;
    // targetType==CP with an address set: must match by address or name.
    if (_pCfg->bleAddress[0] != '\0') {
        if (strncmp(_pCfg->bleAddress, address, sizeof(_pCfg->bleAddress)) == 0) return true;
        return _pCfg->bleName[0] != '\0' && name && name[0] != '\0' &&
               strncmp(_pCfg->bleName, name, sizeof(_pCfg->bleName)) == 0;
    }
    return false;  // no auto-connect without a picked target
}

void CyclePowerScanner::connectDevice(const NimBLEAdvertisedDevice* advertisedDevice) {
    _lastConnectAttempt = millis();
    _connecting = true;

    if (_pClient) {
        if (_pClient->isConnected()) _pClient->disconnect();
        _pClient->cancelConnect();
        NimBLEDevice::deleteClient(_pClient);
        _pClient = nullptr;
    }
    _pClient = NimBLEDevice::createClient();
    _pClient->setClientCallbacks(this, false);
    _pClient->setConnectTimeout(10000);

    Serial.printf("BLE CP: connecting to %s\n", advertisedDevice->getAddress().toString().c_str());
    if (!_pClient->connect(advertisedDevice, true, false, true)) {
        Serial.printf("BLE CP: connect failed err=%d\n", _pClient->getLastError());
        _connecting = false;
        _connected = false;
        _lastConnectAttempt = millis();
        return;
    }

    _connected = subscribeMeasurement();
    _connecting = false;
    if (!_connected) {
        _pClient->disconnect();
    }
}

bool CyclePowerScanner::subscribeMeasurement() {
    NimBLERemoteService* service = _pClient->getService(CP_SERVICE_UUID);
    if (!service) {
        Serial.println("BLE CP: service 1818 not found");
        return false;
    }

    NimBLERemoteCharacteristic* measurement = service->getCharacteristic(CP_MEASUREMENT_UUID);
    if (!measurement || !measurement->canNotify()) {
        Serial.println("BLE CP: measurement notify characteristic not found");
        return false;
    }

    bool ok = measurement->subscribe(true, notifyCallback, true);
    if (ok) {
        strlcpy(_pCfg->connectedAddress, _pClient->getPeerAddress().toString().c_str(), sizeof(_pCfg->connectedAddress));
        Serial.printf("BLE CP: subscribed to %s\n", _pCfg->connectedAddress);
    }
    return ok;
}

void CyclePowerScanner::onConnect(NimBLEClient* pClient) {
    (void)pClient;
    Serial.println("BLE CP: connected");
}

void CyclePowerScanner::onConnectFail(NimBLEClient* pClient, int reason) {
    (void)pClient;
    Serial.printf("BLE CP: connect failed reason=%d\n", reason);
    _connected = false;
    _connecting = false;
    if (_pCfg) _pCfg->connectedAddress[0] = '\0';
}

void CyclePowerScanner::onDisconnect(NimBLEClient* pClient, int reason) {
    (void)pClient;
    Serial.printf("BLE CP: disconnected reason=%d\n", reason);
    _connected = false;
    _connecting = false;
    if (_pCfg) _pCfg->connectedAddress[0] = '\0';
}

void CyclePowerScanner::notifyCallback(NimBLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify) {
    (void)characteristic;
    (void)isNotify;
    if (_instance) _instance->parseMeasurement(data, length);
}

void CyclePowerScanner::parseMeasurement(uint8_t* bytes, size_t len) {
    if (!_pData || !_pCfg || _pCfg->mode != SOURCE_BLE || _pCfg->targetType != TARGET_CP || len < 4) return;

    uint16_t flags = read_u16(bytes);
    int16_t power = read_s16(bytes + 2);
    size_t offset = 4;
    uint32_t now_tick = get_tick_now();

    if (flags & 0x01) offset += 1;
    if (flags & 0x04) offset += 2;

    if (flags & 0x10) {
        if (offset + 6 > len) return;
        uint32_t wheelRevs = read_u32(bytes + offset);
        uint16_t wheelEventTime = read_u16(bytes + offset + 4);
        _pData->wheel_revs = wheelRevs;
        _pData->wheel_event_tick = wheelEventTime / 2;  // CP wire 1/2048 → internal 1/1024
        _pData->has_wheel_event = true;
        offset += 6;
    } else {
        _pData->has_wheel_event = false;
    }

    if (flags & 0x20) {
        if (offset + 4 > len) return;
        uint16_t crankRevs = read_u16(bytes + offset);
        uint16_t crankEventTime = read_u16(bytes + offset + 2);
        _pData->crank_revs = crankRevs;
        _pData->crank_event_tick = crankEventTime;
        _pData->has_crank_event = true;
        offset += 4;
    } else {
        _pData->has_crank_event = false;
    }

    if (flags & 0x40) offset += 4;
    if (flags & 0x80) offset += 4;
    if (flags & 0x0100) offset += 3;
    if (flags & 0x0200) offset += 2;
    if (flags & 0x0400) offset += 2;
    if (flags & 0x0800) offset += 2;

    _pData->power_w = (uint16_t)max((int16_t)0, power);
    _pData->last_update_tick = now_tick;
}
