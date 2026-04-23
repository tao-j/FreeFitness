#ifndef KEISER_H
#define KEISER_H

#include <NimBLEDevice.h>
#include "sim.h"

class KeiserScanner : public NimBLEScanCallbacks {
public:
    void begin(BikeData& data);
    void update(); 

    // NimBLEScanCallbacks
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;

private:
    BikeData* _pData;
    NimBLEScan* _pScan;
};

#endif
