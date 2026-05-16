// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "keiser.h"
#include "../bridge.h"

void KeiserScanner::begin(BikeData& data, const SourceConfig& cfg) {
    _pData = &data;
    _pCfg = &cfg;
}

void KeiserScanner::stop() {
    _pData = nullptr;
}

void KeiserScanner::onAdvertisement(const NimBLEAdvertisedDevice* advertisedDevice,
                                    uint8_t m3BikeId, const uint8_t* payload) {
    if (!_pData || !_pCfg) return;
    if (_pCfg->mode != SOURCE_BLE || _pCfg->targetType != TARGET_M3) return;

    // Strict match-both filter for picked target. If targetType==M3 but no
    // address saved yet (shouldn't happen with picker-only flow), accept by ID.
    std::string address = advertisedDevice->getAddress().toString();
    if (_pCfg->bleAddress[0] != '\0') {
        if (strncmp(_pCfg->bleAddress, address.c_str(), sizeof(_pCfg->bleAddress)) != 0) return;
        if (_pCfg->keiserBikeId != 0 && m3BikeId != (uint8_t)_pCfg->keiserBikeId) return;
    } else if (_pCfg->keiserBikeId != 0 && m3BikeId != (uint8_t)_pCfg->keiserBikeId) {
        return;
    }

    // Bytes 4-5: Cadence (unit 0.1 RPM). Bytes 8-9: Power (W).
    uint16_t rawCadence = payload[4] | (payload[5] << 8);
    uint16_t rawPower = payload[8] | (payload[9] << 8);

    _pData->cadence_rpm = rawCadence / 10.0f;
    _pData->power_w = rawPower;
    _pData->has_crank_event = false;
    _pData->has_wheel_event = false;
    _pData->last_update_tick = get_tick_now();
}
