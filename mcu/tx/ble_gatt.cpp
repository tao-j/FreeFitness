// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "ble_gatt.h"
#include "../config.h"
#include <esp_flash.h>

// UUIDs
#define BAT_SERVICE_UUID       "180F"
#define BAT_LEVEL_UUID         "2A19"
#define DI_SERVICE_UUID        "180A"
#define CP_SERVICE_UUID        "1818"
#define CP_MEASUREMENT_UUID    "2A63"
#define CP_FEATURE_UUID        "2A65"
#define CP_CONTROL_POINT_UUID  "2A66"
#define CSC_SERVICE_UUID       "1816"
#define CSC_MEASUREMENT_UUID   "2A5B"
#define CSC_FEATURE_UUID       "2A5C"
#define SC_CONTROL_POINT_UUID  "2A55"
#define SENSOR_LOCATION_UUID   "2A5D"

namespace {
void updateAdvertisingForCapacity(NimBLEServer* pServer) {
    if (pServer->getConnectedCount() < NIMBLE_MAX_CONNECTIONS) {
        NimBLEDevice::startAdvertising();
    } else {
        NimBLEDevice::getAdvertising()->stop();
    }
}
} // namespace

void BleManager::ServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    _status.bleConnected = true;
    Serial.printf("BLE: Connected from %s (total %u)\n",
                  connInfo.getAddress().toString().c_str(),
                  pServer->getConnectedCount());
    updateAdvertisingForCapacity(pServer);
}

void BleManager::ServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    _status.bleConnected = (pServer->getConnectedCount() > 0);
    Serial.printf("BLE: Disconnected from %s, reason: %d (total %u)\n",
                  connInfo.getAddress().toString().c_str(),
                  reason,
                  pServer->getConnectedCount());
    updateAdvertisingForCapacity(pServer);
}

void BleManager::begin(SystemStatus& status, const ProfileConfig& config) {
    uint64_t uniqueId = 0;
    esp_flash_read_unique_chip_id(NULL, &uniqueId);
    char devName[32];
    snprintf(devName, sizeof(devName), "%s-%04X", DEVICE_NAME_PREFIX, (uint16_t)uniqueId);
    char serialStr[17];
    snprintf(serialStr, sizeof(serialStr), "%08X%08X", (uint32_t)(uniqueId >> 32), (uint32_t)uniqueId);

    NimBLEDevice::init(devName);
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(status));

    NimBLEService* pBatService = pServer->createService(BAT_SERVICE_UUID);
    _pBatLevel = pBatService->createCharacteristic(BAT_LEVEL_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    uint8_t batLevel = 100;
    _pBatLevel->setValue(&batLevel, 1);

    NimBLEService* pDIService = pServer->createService(DI_SERVICE_UUID);
    pDIService->createCharacteristic("2A29", NIMBLE_PROPERTY::READ)->setValue(MANUFACTURER_NAME);
    pDIService->createCharacteristic("2A24", NIMBLE_PROPERTY::READ)->setValue(devName);
    pDIService->createCharacteristic("2A25", NIMBLE_PROPERTY::READ)->setValue(serialStr);
    pDIService->createCharacteristic("2A27", NIMBLE_PROPERTY::READ)->setValue(VERSION_HW_STR);
    pDIService->createCharacteristic("2A26", NIMBLE_PROPERTY::READ)->setValue(VERSION_SW_STR);
    pDIService->createCharacteristic("2A28", NIMBLE_PROPERTY::READ)->setValue(VERSION_FW_STR);
    uint64_t sysId = uniqueId;
    pDIService->createCharacteristic("2A23", NIMBLE_PROPERTY::READ)->setValue((uint8_t*)&sysId, 8);

    if (config.ble_cp) {
        NimBLEService* pCPService = pServer->createService(CP_SERVICE_UUID);
        _pCPMeasurement = pCPService->createCharacteristic(CP_MEASUREMENT_UUID, NIMBLE_PROPERTY::NOTIFY);

        NimBLECharacteristic* pCPFeature = pCPService->createCharacteristic(CP_FEATURE_UUID, NIMBLE_PROPERTY::READ);
        uint32_t cpFeatureVal = 0x0000000C;
        pCPFeature->setValue((uint8_t*)&cpFeatureVal, 4);

        NimBLECharacteristic* pCPSensorLoc = pCPService->createCharacteristic(SENSOR_LOCATION_UUID, NIMBLE_PROPERTY::READ);
        uint8_t sensorLoc = 0x0D;
        pCPSensorLoc->setValue(&sensorLoc, 1);

        pCPService->createCharacteristic(CP_CONTROL_POINT_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    }

    if (config.ble_csc) {
        NimBLEService* pCSCService = pServer->createService(CSC_SERVICE_UUID);
        _pCSCMeasurement = pCSCService->createCharacteristic(CSC_MEASUREMENT_UUID, NIMBLE_PROPERTY::NOTIFY);

        NimBLECharacteristic* pCSCFeature = pCSCService->createCharacteristic(CSC_FEATURE_UUID, NIMBLE_PROPERTY::READ);
        uint16_t cscFeatureVal = 0x0003;
        pCSCFeature->setValue((uint8_t*)&cscFeatureVal, 2);

        NimBLECharacteristic* pCSCSensorLoc = pCSCService->createCharacteristic(SENSOR_LOCATION_UUID, NIMBLE_PROPERTY::READ);
        uint8_t sensorLoc = 0x0D;
        pCSCSensorLoc->setValue(&sensorLoc, 1);

        pCSCService->createCharacteristic(SC_CONTROL_POINT_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    }

    if (config.ble_ftms) {
        NimBLEService* pFTMSService = pServer->createService("1826");
        NimBLECharacteristic* pFTMSFeature = pFTMSService->createCharacteristic("2ACC", NIMBLE_PROPERTY::READ);
        uint32_t ftmsFeatureVal = 0;
        pFTMSFeature->setValue((uint8_t*)&ftmsFeatureVal, 4);
    }

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BAT_SERVICE_UUID);
    pAdvertising->addServiceUUID(DI_SERVICE_UUID);
    if (config.ble_cp) pAdvertising->addServiceUUID(CP_SERVICE_UUID);
    if (config.ble_csc) pAdvertising->addServiceUUID(CSC_SERVICE_UUID);
    if (config.ble_ftms) pAdvertising->addServiceUUID("1826");
    pAdvertising->setAppearance(0x0485);
    pAdvertising->start();

    Serial.println("BLE Module Initialized.");
}

void BleManager::update(const Encoder& enc) {
    if (enc.noData()) return;
    if (NimBLEDevice::getServer()->getConnectedCount() == 0) return;

    if (_pCPMeasurement) {
        uint8_t cpData[14];
        cpData[0] = 0x30;
        cpData[1] = 0x00;
        uint16_t p = enc.power_w();
        cpData[2] = (uint8_t)(p & 0xFF);
        cpData[3] = (uint8_t)(p >> 8);

        uint32_t wrev = enc.wheel_revs();
        uint16_t wtick = enc.wheel_event_tick_2048();
        cpData[4] = (uint8_t)(wrev & 0xFF);
        cpData[5] = (uint8_t)((wrev >> 8) & 0xFF);
        cpData[6] = (uint8_t)((wrev >> 16) & 0xFF);
        cpData[7] = (uint8_t)((wrev >> 24) & 0xFF);
        cpData[8] = (uint8_t)(wtick & 0xFF);
        cpData[9] = (uint8_t)(wtick >> 8);

        uint32_t crev = enc.crank_revs();
        uint16_t ctick = enc.crank_event_tick_1024();
        cpData[10] = (uint8_t)(crev & 0xFF);
        cpData[11] = (uint8_t)((crev >> 8) & 0xFF);
        cpData[12] = (uint8_t)(ctick & 0xFF);
        cpData[13] = (uint8_t)(ctick >> 8);

        _pCPMeasurement->setValue(cpData, 14);
        _pCPMeasurement->notify();
    }

    if (_pCSCMeasurement) {
        uint8_t cscData[11];
        cscData[0] = 0x03;
        uint32_t wrev = enc.wheel_revs();
        uint16_t wtick = enc.wheel_event_tick_1024();
        cscData[1] = (uint8_t)(wrev & 0xFF);
        cscData[2] = (uint8_t)((wrev >> 8) & 0xFF);
        cscData[3] = (uint8_t)((wrev >> 16) & 0xFF);
        cscData[4] = (uint8_t)((wrev >> 24) & 0xFF);
        cscData[5] = (uint8_t)(wtick & 0xFF);
        cscData[6] = (uint8_t)(wtick >> 8);
        uint32_t crev = enc.crank_revs();
        uint16_t ctick = enc.crank_event_tick_1024();
        cscData[7] = (uint8_t)(crev & 0xFF);
        cscData[8] = (uint8_t)((crev >> 8) & 0xFF);
        cscData[9] = (uint8_t)(ctick & 0xFF);
        cscData[10] = (uint8_t)(ctick >> 8);
        _pCSCMeasurement->setValue(cscData, 11);
        _pCSCMeasurement->notify();
    }
}

void BleManager::updateBattery(uint8_t level) {
    _pBatLevel->setValue(&level, 1);
    _pBatLevel->notify();
}

void BleManager::startAdvertising() {
    NimBLEDevice::startAdvertising();
}

void BleManager::stopAdvertising() {
    NimBLEDevice::getAdvertising()->stop();
}
