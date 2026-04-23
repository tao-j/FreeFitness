#include "keiser.h"

void KeiserScanner::begin(BikeData& data) {
    _pData = &data;
    _pScan = NimBLEDevice::getScan();
    _pScan->setScanCallbacks(this, false);
    _pScan->setInterval(100);
    _pScan->setWindow(100);
    _pScan->setActiveScan(false);
    _pScan->start(0, false, false); 
}

void KeiserScanner::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (_pData->isSimMode) return;

    if (advertisedDevice->haveManufacturerData()) {
        std::string msd = advertisedDevice->getManufacturerData();
        const uint8_t* pData = (const uint8_t*)msd.data();
        size_t totalLen = msd.length();

        // Keiser Manufacturer ID is 0x0102 (Little Endian: 02 01)
        // NimBLE getManufacturerData() returns [ID_L] [ID_H] [Payload...]
        // So totalLen should be 2 (ID) + 17 (M3i Payload) = 19 bytes.
        if (totalLen == 19 && pData[0] == 0x02 && pData[1] == 0x01) {
            const uint8_t* payload = &pData[2];
            
            // Byte 2 of payload is Data Type (0 = Real Time Data)
            // Byte 3 of payload is Bike ID
            if (payload[2] != 0) return;

            uint8_t bike_id = payload[3];
            if (_pData->targetBikeId != 0 && bike_id != (uint16_t)_pData->targetBikeId) {
                return;
            }

            _pData->lastDataTime = millis();

            // Bytes 4-5: Cadence (Little Endian, unit 0.1 RPM)
            uint16_t rawCadence = payload[4] | (payload[5] << 8);
            // Bytes 8-9: Power (Little Endian, Watts)
            uint16_t rawPower = payload[8] | (payload[9] << 8);

            _pData->cadence = rawCadence / 10;
            _pData->power = rawPower;
            
            static uint16_t lastPower = 0;
            if (rawPower != lastPower) {
                _pData->accPower += rawPower;
                _pData->eventCount++;
                lastPower = rawPower;
            }
            
            // Update timestamps for BLE CSC / CP
            _pData->crankEventTime = (uint16_t)(millis() * 1024 / 1000);
            _pData->wheelEventTime = (uint16_t)(millis() * 2048 / 1000);
        }
    }
}
