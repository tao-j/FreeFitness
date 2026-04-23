// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef ANT_PLUS_SPEED_H
#define ANT_PLUS_SPEED_H

#include <BaseClasses/ANTPLUS_BaseMasterProfile.h>
#include <Profiles/BicycleSpeed/ANTPLUS_BicycleSpeedDataPages.h>
#include <CommonDataPages/ANTPLUS_CommonDataPages.h>

class ProfileBicycleSpeedSensorImpl : public BaseMasterProfile {
public:
    explicit ProfileBicycleSpeedSensorImpl(uint16_t deviceNumber, uint8_t transmissionType = 0, uint32_t flags = 0);

    void createBicycleSpeedDataPage0Msg(void (*func)(BicycleSpeedBaseMainDataPageMsg&, uintptr_t), uintptr_t data = 0) { _createBicycleSpeedDataPage0Msg.set(func, data); }
    void createProductInformationMsg(void (*func)(ProductInformationMsg&, uintptr_t), uintptr_t data = 0) { _createProductInformationMsg.set(func, data); }
    void createManufacturersInformationMsg(void (*func)(ManufacturersInformationMsg&, uintptr_t), uintptr_t data = 0) { _createManufacturersInformationMsg.set(func, data); }
    void createBatteryStatusMsg(void (*func)(BatteryStatusMsg&, uintptr_t), uintptr_t data = 0) { _createBatteryStatusMsg.set(func, data); }

protected:
    void transmitNextDataPage() override;
    bool isDataPageValid(uint8_t dataPage) override;

private:
    void setChannelConfig();
    void transmitDataPage(uint8_t page);
    uint8_t getBackgroundPage();

    void transmitBicycleSpeedDataPage0Msg();
    void transmitProductInformationMsg();
    void transmitManufacturersInformationMsg();
    void transmitBatteryStatusMsg();

    AntCallback<BicycleSpeedBaseMainDataPageMsg&> _createBicycleSpeedDataPage0Msg = { .func = NULL };
    AntCallback<ProductInformationMsg&> _createProductInformationMsg = { .func = NULL };
    AntCallback<ManufacturersInformationMsg&> _createManufacturersInformationMsg = { .func = NULL };
    AntCallback<BatteryStatusMsg&> _createBatteryStatusMsg = { .func = NULL };

    uint8_t _patternStep = 0;
    uint32_t _flags = 0;
};

#endif
