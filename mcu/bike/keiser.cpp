#include "keiser.h"

static constexpr float WHEEL_CIRCUMFERENCE = 2.096f;  // 700c × 25 (m)

void KeiserScanner::begin(BikeData& data) {
    _pData = &data;
    _pScan = NimBLEDevice::getScan();
    _pScan->setScanCallbacks(this, false);
    _pScan->setInterval(100);
    _pScan->setWindow(100);
    _pScan->setActiveScan(false);
    _pScan->start(0, false, false);
}

void KeiserScanner::update() {
    // Placeholder kept for API stability — scanner is callback-driven.
}

void KeiserScanner::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (_pData->isSimMode) return;
    if (!advertisedDevice->haveManufacturerData()) return;

    std::string msd = advertisedDevice->getManufacturerData();
    const uint8_t* pData = (const uint8_t*)msd.data();
    size_t totalLen = msd.length();

    // Keiser Manufacturer ID = 0x0102 (LE: 02 01), followed by 17-byte M3i payload.
    if (totalLen != 19 || pData[0] != 0x02 || pData[1] != 0x01) return;

    const uint8_t* payload = &pData[2];
    if (payload[2] != 0) return;  // Data Type: 0 = Real-Time Data

    uint8_t bike_id = payload[3];
    if (_pData->targetBikeId != 0 && bike_id != (uint16_t)_pData->targetBikeId) return;

    // Bytes 4-5: Cadence (unit 0.1 RPM). Bytes 8-9: Power (W).
    uint16_t rawCadence = payload[4] | (payload[5] << 8);
    uint16_t rawPower = payload[8] | (payload[9] << 8);

    float cadence_rpm = rawCadence / 10.0f;
    float power_w = (float)rawPower;

    uint32_t now_tick = get_tick_now();

    // Feed both channels as rate sources — the bridges synthesize the
    // discrete revolution events that CSC/CP/ANT payloads require.
    _crank_bridge.feed_rate(cadence_rpm / 60.0f, now_tick);
    float speed_mps = power_to_speed(power_w);
    _wheel_bridge.feed_rate(speed_mps / WHEEL_CIRCUMFERENCE, now_tick);

    // Latch scalar wire fields.
    _pData->power = rawPower;
    _pData->cadence = (uint8_t)cadence_rpm;
    _pData->speed_mps = speed_mps;

    // Latch bridge-derived event fields.
    _pData->crankRevs = (uint16_t)_crank_bridge.count_int();
    _pData->crankEventTime = (uint16_t)_crank_bridge.event_tick();
    _pData->wheelRevs = _wheel_bridge.count_int();
    _pData->wheelEventTime = (uint16_t)(_wheel_bridge.event_tick() * 2);  // 1/1024 → 1/2048

    // ANT+ power-page accumulators: step on every fresh advertisement,
    // same as the Python encoder's 20 Hz tick loop. Gating on power change
    // under-counts because Keiser often reports the same integer watt twice.
    _pData->accPower += rawPower;
    _pData->eventCount++;

    _pData->lastUpdateTick = now_tick;
    _pData->lastDataTime = millis();
}
