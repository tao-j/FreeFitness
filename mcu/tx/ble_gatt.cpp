#include "ble_gatt.h"
#include "config.h"
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

void BleManager::ServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    _data.bleConnected = true;
    Serial.printf("BLE: Connected from %s (total %u)\n",
                  connInfo.getAddress().toString().c_str(),
                  pServer->getConnectedCount());
    // NimBLE stops advertising on connect; resume so additional centrals can
    // still discover and connect (bounded by CONFIG_BT_NIMBLE_MAX_CONNECTIONS).
    NimBLEDevice::startAdvertising();
}

void BleManager::ServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    _data.bleConnected = false;
    Serial.printf("BLE: Disconnected, reason: %d\n", reason);
    NimBLEDevice::startAdvertising();
}

void BleManager::begin(BikeData& data, const ProfileConfig& config) {
    uint64_t uniqueId = 0;
    esp_flash_read_unique_chip_id(NULL, &uniqueId);
    char devName[32];
    snprintf(devName, sizeof(devName), "%s-%04X", DEVICE_NAME_PREFIX, (uint16_t)uniqueId);
    char serialStr[17];
    snprintf(serialStr, sizeof(serialStr), "%08X%08X", (uint32_t)(uniqueId >> 32), (uint32_t)uniqueId);

    NimBLEDevice::init(devName);
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(data));

    // 1. Battery Service
    NimBLEService* pBatService = pServer->createService(BAT_SERVICE_UUID);
    _pBatLevel = pBatService->createCharacteristic(BAT_LEVEL_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    uint8_t batLevel = 100;
    _pBatLevel->setValue(&batLevel, 1);

    // 2. Device Information Service
    NimBLEService* pDIService = pServer->createService(DI_SERVICE_UUID);
    pDIService->createCharacteristic("2A29", NIMBLE_PROPERTY::READ)->setValue(MANUFACTURER_NAME);
    pDIService->createCharacteristic("2A24", NIMBLE_PROPERTY::READ)->setValue(devName);
    pDIService->createCharacteristic("2A25", NIMBLE_PROPERTY::READ)->setValue(serialStr);
    pDIService->createCharacteristic("2A27", NIMBLE_PROPERTY::READ)->setValue(VERSION_HW_STR);
    pDIService->createCharacteristic("2A26", NIMBLE_PROPERTY::READ)->setValue(VERSION_SW_STR);
    pDIService->createCharacteristic("2A28", NIMBLE_PROPERTY::READ)->setValue(VERSION_FW_STR);
    uint64_t sysId = uniqueId; // Use actual unique ID for system ID
    pDIService->createCharacteristic("2A23", NIMBLE_PROPERTY::READ)->setValue((uint8_t*)&sysId, 8);

    // 3. Cycling Power Service
    if (config.ble_cp) {
        NimBLEService* pCPService = pServer->createService(CP_SERVICE_UUID);
        _pCPMeasurement = pCPService->createCharacteristic(CP_MEASUREMENT_UUID, NIMBLE_PROPERTY::NOTIFY);
        
        NimBLECharacteristic* pCPFeature = pCPService->createCharacteristic(CP_FEATURE_UUID, NIMBLE_PROPERTY::READ);
        uint32_t cpFeatureVal = 0x0000000C; // Bit 2: Wheel, Bit 3: Crank Data Supported
        pCPFeature->setValue((uint8_t*)&cpFeatureVal, 4);
        
        NimBLECharacteristic* pCPSensorLoc = pCPService->createCharacteristic(SENSOR_LOCATION_UUID, NIMBLE_PROPERTY::READ);
        uint8_t sensorLoc = 0x0D; // Rear Hub
        pCPSensorLoc->setValue(&sensorLoc, 1);
        
        pCPService->createCharacteristic(CP_CONTROL_POINT_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    }

    // 4. Cycling Speed and Cadence Service
    if (config.ble_csc) {
        NimBLEService* pCSCService = pServer->createService(CSC_SERVICE_UUID);
        _pCSCMeasurement = pCSCService->createCharacteristic(CSC_MEASUREMENT_UUID, NIMBLE_PROPERTY::NOTIFY);
        
        NimBLECharacteristic* pCSCFeature = pCSCService->createCharacteristic(CSC_FEATURE_UUID, NIMBLE_PROPERTY::READ);
        uint16_t cscFeatureVal = 0x0003; 
        pCSCFeature->setValue((uint8_t*)&cscFeatureVal, 2);
        
        NimBLECharacteristic* pCSCSensorLoc = pCSCService->createCharacteristic(SENSOR_LOCATION_UUID, NIMBLE_PROPERTY::READ);
        uint8_t sensorLoc = 0x0D; // Rear Hub
        pCSCSensorLoc->setValue(&sensorLoc, 1);

        pCSCService->createCharacteristic(SC_CONTROL_POINT_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::INDICATE);
    }

    // 5. FTMS Service (Stub)
    if (config.ble_ftms) {
        NimBLEService* pFTMSService = pServer->createService("1826"); // FTMS
        NimBLECharacteristic* pFTMSFeature = pFTMSService->createCharacteristic("2ACC", NIMBLE_PROPERTY::READ);
        uint32_t ftmsFeatureVal = 0; // Stub
        pFTMSFeature->setValue((uint8_t*)&ftmsFeatureVal, 4);
    }

    // Advertising
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

void BleManager::update(BikeData& data) {
    // Sync connection status directly from NimBLE server (source of truth)
    data.bleConnected = (NimBLEDevice::getServer()->getConnectedCount() > 0);
    
    if (!data.bleConnected) return;

    // CP Measurement
    if (_pCPMeasurement) {
        uint8_t cpData[14];
        cpData[0] = 0x30; 
        cpData[1] = 0x00;
        cpData[2] = (uint8_t)(data.power & 0xFF);
        cpData[3] = (uint8_t)(data.power >> 8);
        
        // Wheel Data
        cpData[4] = (uint8_t)(data.wheelRevs & 0xFF);
        cpData[5] = (uint8_t)((data.wheelRevs >> 8) & 0xFF);
        cpData[6] = (uint8_t)((data.wheelRevs >> 16) & 0xFF);
        cpData[7] = (uint8_t)((data.wheelRevs >> 24) & 0xFF);
        cpData[8] = (uint8_t)(data.wheelEventTime & 0xFF);
        cpData[9] = (uint8_t)(data.wheelEventTime >> 8);
        
        // Crank Data
        cpData[10] = (uint8_t)(data.crankRevs & 0xFF);
        cpData[11] = (uint8_t)(data.crankRevs >> 8);
        cpData[12] = (uint8_t)(data.crankEventTime & 0xFF);
        cpData[13] = (uint8_t)(data.crankEventTime >> 8);
        
        _pCPMeasurement->setValue(cpData, 14);
        _pCPMeasurement->notify();
    }

    // CSC Measurement
    if (_pCSCMeasurement) {
        uint8_t cscData[11];
        cscData[0] = 0x03; 
        cscData[1] = (uint8_t)(data.wheelRevs & 0xFF);
        cscData[2] = (uint8_t)((data.wheelRevs >> 8) & 0xFF);
        cscData[3] = (uint8_t)((data.wheelRevs >> 16) & 0xFF);
        cscData[4] = (uint8_t)((data.wheelRevs >> 24) & 0xFF);
        cscData[5] = (uint8_t)((data.wheelEventTime / 2) & 0xFF); // CSC uses 1/1024s
        cscData[6] = (uint8_t)((data.wheelEventTime / 2) >> 8);
        cscData[7] = (uint8_t)(data.crankRevs & 0xFF);
        cscData[8] = (uint8_t)(data.crankRevs >> 8);
        cscData[9] = (uint8_t)(data.crankEventTime & 0xFF);
        cscData[10] = (uint8_t)(data.crankEventTime >> 8);
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
