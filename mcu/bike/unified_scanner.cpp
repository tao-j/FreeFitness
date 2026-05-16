// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "unified_scanner.h"
#include "keiser.h"
#include "cycle_power.h"

static const NimBLEUUID CP_SERVICE_UUID("1818");
static constexpr uint16_t KEISER_MANUFACTURER_ID = 0x0102;
static constexpr size_t M3_PAYLOAD_LEN = 19;  // 2 byte ID + 17 byte payload

void UnifiedScanner::begin() {
    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    if (pAdv && pAdv->isAdvertising()) pAdv->stop();
    _pScan = NimBLEDevice::getScan();
    if (_pScan->isScanning()) _pScan->stop();
    _pScan->setScanCallbacks(this, true);
    // Initial duty cycle: aggressive, will be tuned per state in a follow-up.
    _pScan->setInterval(160);  // 100 ms (in 0.625 ms slots)
    _pScan->setWindow(80);     // 50 ms → 50% duty
    _pScan->setActiveScan(true);
    _pScan->start(0, false, true);
    if (pAdv && !pAdv->isAdvertising()) pAdv->start();
    Serial.println("UnifiedScanner: started");
}

void UnifiedScanner::stop() {
    if (_pScan && _pScan->isScanning()) _pScan->stop();
    _entryCount = 0;
    Serial.println("UnifiedScanner: stopped");
}

void UnifiedScanner::attach(KeiserScanner* keiser, CyclePowerScanner* cp) {
    _keiser = keiser;
    _cp = cp;
}

bool UnifiedScanner::tryParseM3(const NimBLEAdvertisedDevice* dev, uint8_t& outBikeId, const uint8_t** outPayload) const {
    if (!dev->haveManufacturerData()) return false;
    std::string msd = dev->getManufacturerData();
    const uint8_t* pData = (const uint8_t*)msd.data();
    if (msd.length() != M3_PAYLOAD_LEN) return false;
    if (pData[0] != (KEISER_MANUFACTURER_ID & 0xFF) || pData[1] != (KEISER_MANUFACTURER_ID >> 8)) return false;
    const uint8_t* payload = &pData[2];
    if (payload[2] != 0) return false;  // Data Type 0 = Real-Time Data
    outBikeId = payload[3];
    if (outPayload) *outPayload = payload;
    return true;
}

void UnifiedScanner::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    std::string address = advertisedDevice->getAddress().toString();
    std::string name = advertisedDevice->haveName() ? advertisedDevice->getName() : "";
    int8_t rssi = advertisedDevice->getRSSI();

    if (millis() - _lastAnyAdvertLog > 3000) {
        Serial.printf("BLE scan: addr=%s name=%s rssi=%d\n",
                      address.c_str(), name.c_str(), rssi);
        _lastAnyAdvertLog = millis();
    }

    // M3 filter
    uint8_t m3BikeId = 0;
    const uint8_t* m3Payload = nullptr;
    if (tryParseM3(advertisedDevice, m3BikeId, &m3Payload)) {
        char label[22];
        if (!name.empty()) snprintf(label, sizeof(label), "%s", name.c_str());
        else snprintf(label, sizeof(label), "Keiser M3");
        upsert(TARGET_M3, address.c_str(), label, m3BikeId, rssi);
        if (_keiser) _keiser->onAdvertisement(advertisedDevice, m3BikeId, m3Payload);
        return;
    }

    // CP filter — service UUID OR known CP sim name
    bool advertisesCp = advertisedDevice->isAdvertisingService(CP_SERVICE_UUID);
    bool knownCpName = name == "FreeFitness CP Sim";
    if (advertisesCp || knownCpName) {
        char label[22];
        if (!name.empty()) snprintf(label, sizeof(label), "%s", name.c_str());
        else snprintf(label, sizeof(label), "Cycling Power");
        upsert(TARGET_CP, address.c_str(), label, 0, rssi);
        if (_cp) _cp->onAdvertisement(advertisedDevice);
    }
}

uint8_t UnifiedScanner::upsert(BikeType type, const char* address, const char* name, uint8_t m3BikeId, int8_t rssi) {
    // For M3: match by (address, bike_id) so collisions on the default bike
    // ID with multiple bikes produce separate entries.
    uint8_t slot = _entryCount;
    for (uint8_t i = 0; i < _entryCount; i++) {
        if (_entries[i].type != type) continue;
        if (strncmp(_entries[i].address, address, sizeof(_entries[i].address)) != 0) continue;
        if (type == TARGET_M3 && _entries[i].m3BikeId != m3BikeId) continue;
        slot = i;
        break;
    }
    if (slot >= MAX_ENTRIES) slot = MAX_ENTRIES - 1;
    else if (slot == _entryCount) _entryCount++;

    _entries[slot].type = type;
    strlcpy(_entries[slot].address, address, sizeof(_entries[slot].address));
    strlcpy(_entries[slot].name, name, sizeof(_entries[slot].name));
    _entries[slot].m3BikeId = m3BikeId;
    _entries[slot].rssi = rssi;
    _entries[slot].lastSeen = millis();
    return slot;
}

void UnifiedScanner::saveBrowsed(uint8_t index, SourceConfig& cfg) {
    if (index >= _entryCount) return;
    const ScanEntry& e = _entries[index];
    cfg.targetType = e.type;
    strlcpy(cfg.bleAddress, e.address, sizeof(cfg.bleAddress));
    strlcpy(cfg.bleName, e.name, sizeof(cfg.bleName));
    cfg.keiserBikeId = (e.type == TARGET_M3) ? e.m3BikeId : 0;
}

void UnifiedScanner::clearTarget(SourceConfig& cfg) {
    cfg.targetType = TARGET_NONE;
    cfg.bleAddress[0] = '\0';
    cfg.bleName[0] = '\0';
    cfg.keiserBikeId = 0;
    cfg.connectedAddress[0] = '\0';
}
