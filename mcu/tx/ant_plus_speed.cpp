// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "ant_plus_speed.h"
#include <Profiles/BicycleSpeed/ANTPLUS_BicycleSpeedDefines.h>
#include <CommonDataPages/ANTPLUS_CommonDataPageDefines.h>
#include <ANTPLUS_PrivateDefines.h>

#define SENSOR_CHANNELTYPE   CHANNEL_TYPE_BIDIRECTIONAL_TRANSMIT
#define SENSOR_TRANSMISSIONTYPE (TRANSMISSION_TYPE_INDEPENDENT | TRANSMISSION_TYPE_GLOBALDATAPGESUSED)
#define BICYCLE_SPEED_CHANNELPERIOD 8118
#define BICYCLE_SPEED_SEARCHTIMEOUT 10

ProfileBicycleSpeedSensorImpl::ProfileBicycleSpeedSensorImpl(
        uint16_t deviceNumber,
        uint8_t transmissionType,
        uint32_t flags) :
    BaseMasterProfile(deviceNumber,
            ANTPLUS_TRANSMISSION_SET_LSN(
                transmissionType, SENSOR_TRANSMISSIONTYPE)),
    _flags(flags) {
    setChannelConfig();
}

void ProfileBicycleSpeedSensorImpl::transmitNextDataPage() {
    uint8_t page = 0;

    if (isRequestedPagePending()) {
        transmitDataPage(getRequestedPage());
        return;
    }

    _patternStep++;
    if (_patternStep == 120) {
        _patternStep = 0;
    }

    page = getBackgroundPage();
    if (page == 0) {
        page = ANTPLUS_BICYCLESPEED_DATAPAGE_DEFAULT_NUMBER;
    }

    transmitDataPage(page);
}

uint8_t ProfileBicycleSpeedSensorImpl::getBackgroundPage() {
    if (_patternStep == 118) {
        return ANTPLUS_COMMON_DATAPAGE_MANUFACTURERSINFORMATION_NUMBER;
    }

    if (_patternStep == 119) {
        return ANTPLUS_COMMON_DATAPAGE_PRODUCTINFORMATION_NUMBER;
    }

    if ((_patternStep == 33 || _patternStep == 93) && _createBatteryStatusMsg.func) {
        return ANTPLUS_COMMON_DATAPAGE_BATTERYSTATUS_NUMBER;
    }

    return 0;
}

void ProfileBicycleSpeedSensorImpl::transmitDataPage(uint8_t page) {
    switch (page) {
    case ANTPLUS_BICYCLESPEED_DATAPAGE_DEFAULT_NUMBER:
        transmitBicycleSpeedDataPage0Msg();
        break;
    case ANTPLUS_COMMON_DATAPAGE_PRODUCTINFORMATION_NUMBER:
        transmitProductInformationMsg();
        break;
    case ANTPLUS_COMMON_DATAPAGE_MANUFACTURERSINFORMATION_NUMBER:
        transmitManufacturersInformationMsg();
        break;
    case ANTPLUS_COMMON_DATAPAGE_BATTERYSTATUS_NUMBER:
        transmitBatteryStatusMsg();
        break;
    }
}

void ProfileBicycleSpeedSensorImpl::transmitBicycleSpeedDataPage0Msg() {
    BicycleSpeedBaseMainDataPageMsg msg(ANTPLUS_BICYCLESPEED_DATAPAGE_DEFAULT_NUMBER);
    _createBicycleSpeedDataPage0Msg.call(msg);
    transmitMsg(msg);
}

void ProfileBicycleSpeedSensorImpl::transmitProductInformationMsg() {
    ProductInformationMsg msg;
    _createProductInformationMsg.call(msg);
    transmitMsg(msg);
}

void ProfileBicycleSpeedSensorImpl::transmitManufacturersInformationMsg() {
    ManufacturersInformationMsg msg;
    _createManufacturersInformationMsg.call(msg);
    transmitMsg(msg);
}

void ProfileBicycleSpeedSensorImpl::transmitBatteryStatusMsg() {
    BatteryStatusMsg msg;
    _createBatteryStatusMsg.call(msg);
    transmitMsg(msg);
}

void ProfileBicycleSpeedSensorImpl::setChannelConfig() {
    setChannelType(SENSOR_CHANNELTYPE);
    setDeviceType(ANTPLUS_BICYCLESPEED_DEVICETYPE);
    setChannelPeriod(BICYCLE_SPEED_CHANNELPERIOD);
    setSearchTimeout(BICYCLE_SPEED_SEARCHTIMEOUT);
}

bool ProfileBicycleSpeedSensorImpl::isDataPageValid(uint8_t dataPage) {
    switch (dataPage) {
    case ANTPLUS_BICYCLESPEED_DATAPAGE_DEFAULT_NUMBER:
    case ANTPLUS_COMMON_DATAPAGE_PRODUCTINFORMATION_NUMBER:
    case ANTPLUS_COMMON_DATAPAGE_MANUFACTURERSINFORMATION_NUMBER:
        return true;
    case ANTPLUS_COMMON_DATAPAGE_BATTERYSTATUS_NUMBER:
        return (bool)_createBatteryStatusMsg.func;
    default:
        return false;
    }
}
