// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "ant_plus.h"
#include "../config.h"
#include <esp_flash.h>
#include <CommonDataPages/ANTPLUS_CommonDataPageDefines.h>

const uint8_t AntManager::NETWORK_KEY[] = {0xB9, 0xA5, 0x21, 0xFB, 0xBD, 0x72, 0xC3, 0x45};

static uint16_t getUniqueDeviceId() {
    uint64_t id = 0;
    esp_flash_read_unique_chip_id(NULL, &id);
    return (uint16_t)id;
}

AntManager::AntManager() : _bikePower(getUniqueDeviceId(), 5, 0), _bikeSpeed(getUniqueDeviceId(), 5, 0) {}

void AntManager::begin(HardwareSerial& serial, const ProfileConfig& config) {
    _config = config;
    _ant.setSerial(serial);

    _router.setDriver(&_ant);
    _router.setAntPlusNetworkKey((uint8_t*)NETWORK_KEY);

    uint8_t profileCount = 0;

    if (_config.ant_pwr) {
        _router.setProfile(profileCount++, &_bikePower);
        _bikePower.createManufacturersInformationMsg(manufacturerIDHandler, (uintptr_t)this);
        _bikePower.createProductInformationMsg(productIDHandler, (uintptr_t)this);
        _bikePower.createBicyclePowerStandardPowerOnlyMsg(powerOnlyHandler, (uintptr_t)this);
        _bikePower.createBatteryStatusMsg(batteryStatusHandler, (uintptr_t)this);
        _bikePower.begin();
    }

    if (_config.ant_csc) {
        _router.setProfile(profileCount++, &_bikeSpeed);
        _bikeSpeed.createManufacturersInformationMsg(manufacturerIDHandler, (uintptr_t)this);
        _bikeSpeed.createProductInformationMsg(productIDHandler, (uintptr_t)this);
        _bikeSpeed.createBicycleSpeedDataPage0Msg(speedHandler, (uintptr_t)this);
        _bikeSpeed.createBatteryStatusMsg(batteryStatusHandler, (uintptr_t)this);
        _bikeSpeed.begin();
    }

    Serial.println("ANT+ Module Initialized.");
}

void AntManager::update(const Encoder& enc, uint8_t batteryLevel) {
    _currentEnc = &enc;
    _batteryLevel = batteryLevel;
}

void AntManager::parse() {
    _router.loop();
}

void AntManager::openChannel() {
    if (_config.ant_pwr) _bikePower.begin();
    if (_config.ant_csc) _bikeSpeed.begin();
}

void AntManager::closeChannel() {
    if (_config.ant_pwr) _bikePower.stop();
    if (_config.ant_csc) _bikeSpeed.stop();
}

uint16_t AntManager::getDeviceId() const {
    return getUniqueDeviceId();
}

void AntManager::manufacturerIDHandler(ManufacturersInformationMsg& msg, uintptr_t data) {
    msg.setHWRevision(VERSION_HW_REV);
    msg.setManufacturerId(MANUFACTURER_ID);
    msg.setModelNumber(1);
}

void AntManager::productIDHandler(ProductInformationMsg& msg, uintptr_t data) {
    msg.setSWRevisionMain(VERSION_SW_MAJOR);
    msg.setSWRevisionSupplemental(VERSION_SW_MINOR);

    uint64_t uniqueId = 0;
    esp_flash_read_unique_chip_id(NULL, &uniqueId);
    msg.setSerialNumber((uint32_t)uniqueId);
}

void AntManager::powerOnlyHandler(BicyclePowerStandardPowerOnlyMsg& msg, uintptr_t data) {
    AntManager* self = (AntManager*)data;
    if (!self->_currentEnc) return;

    const Encoder& e = *(self->_currentEnc);
    msg.setUpdateEventCount(e.powerEventCount());
    msg.setPedalPower(0xFF);
    msg.setInstantaneousCadence(e.cadence_rpm());
    msg.setAccumulatedPower(e.accPower());
    msg.setInstantaneousPower(e.power_w());
}

void AntManager::speedHandler(BicycleSpeedBaseMainDataPageMsg& msg, uintptr_t data) {
    AntManager* self = (AntManager*)data;
    if (!self->_currentEnc) return;

    const Encoder& e = *(self->_currentEnc);
    msg.setBikeSpeedEventTime(e.wheel_event_tick_1024());
    msg.setCumulativeSpeedRevolutionCount(e.wheel_revs());
}

void AntManager::batteryStatusHandler(BatteryStatusMsg& msg, uintptr_t data) {
    AntManager* self = (AntManager*)data;
    msg.setCoarseBatteryVoltage(0x0F);
    msg.setFractionalBatteryVoltage(0xFF);
    msg.setBatteryStatus(self->_batteryLevel > 20 ?
        ANTPLUS_COMMON_DATAPAGE_BATTERYSTATUS_BATTERYSTATUS_GOOD :
        ANTPLUS_COMMON_DATAPAGE_BATTERYSTATUS_BATTERYSTATUS_LOW);
}
