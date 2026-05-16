// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <M5Unified.h>
#include "bike/bike_data.h"
#include "bike/cycle_power.h"
#include "bike/unified_scanner.h"
#include "source_config.h"
#include "system_status.h"
#include "tx/encoder.h"
#include "tx/ant_plus.h"
#include "tx/ble_gatt.h"
#include "config.h"
#include <Preferences.h>

// Two pages plus a transient picker. The picker is reached by short-press
// from Dashboard; cancel/select returns to Dashboard.
enum ViewState { VIEW_DASHBOARD, VIEW_PICKER, VIEW_SETTINGS };

class UiManager {
public:
    UiManager(M5Canvas& canvas, AntManager& ant, BleManager& ble,
              UnifiedScanner& scanner, CyclePowerScanner& cpScanner);

    void update(const Encoder& enc, const SourceConfig& cfg, const SystemStatus& status, ProfileConfig& profile);
    void handleInput(SourceConfig& cfg, ProfileConfig& profile, Preferences& prefs);

    ViewState getView() const { return _currentView; }

private:
    void renderDashboard(const Encoder& enc, const SourceConfig& cfg, const SystemStatus& status);
    void renderPicker(const SourceConfig& cfg);
    void renderSettings(const SourceConfig& cfg, const ProfileConfig& profile);
    const char* modeLabel(BikeSourceMode mode) const;
    const char* typeLabel(BikeType type) const;
    void persistTarget(const SourceConfig& cfg, Preferences& prefs);
    void persistMode(const SourceConfig& cfg, Preferences& prefs);

    M5Canvas& _canvas;
    AntManager& _ant;
    BleManager& _ble;
    UnifiedScanner& _scanner;
    CyclePowerScanner& _cpScanner;

    ViewState _currentView = VIEW_DASHBOARD;
    int _settingsCursor = 0;
    bool _settingsChanged = false;  // gates reboot on Settings exit
    uint8_t _pickerIndex = 0;
    uint32_t _lastUiUpdate = 0;
};

#endif
