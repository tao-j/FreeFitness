// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <M5Unified.h>
#include "bike/sim.h"
#include "tx/ant_plus.h"
#include "tx/ble_gatt.h"
#include "config.h"
#include <Preferences.h>

enum ViewState { VIEW_DASHBOARD, VIEW_SETTINGS };

class UiManager {
public:
    UiManager(M5Canvas& canvas, AntManager& ant, BleManager& ble);
    
    void update(BikeData& data, ProfileConfig& config);
    void handleInput(BikeData& data, ProfileConfig& config, Preferences& prefs);
    
    ViewState getView() const { return _currentView; }
    bool hasSettingsChanged() const { return _settingsChanged; }

private:
    void renderDashboard(const BikeData& data);
    void renderSettings(const ProfileConfig& config);

    M5Canvas& _canvas;
    AntManager& _ant;
    BleManager& _ble;

    ViewState _currentView = VIEW_DASHBOARD;
    int _settingsCursor = 0;
    bool _settingsChanged = false;
    uint32_t _lastUiUpdate = 0;
};

#endif
