// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include "ui_manager.h"

// Settings cursor layout. Source mode and Brightness are applied live; the
// other rows force a reboot on Settings exit (GATT/ANT need re-init).
// Order: Brightness at top (most-touched), Source mode, BLE rows, ANT last.
static constexpr int kSettingsRowCount = 8;
static constexpr int kRowBrightness = 0;
static constexpr int kRowSourceMode = 1;
static constexpr int kRowBleCp      = 2;
static constexpr int kRowBleCsc     = 3;
static constexpr int kRowBleFtms    = 4;
static constexpr int kRowAntPwr     = 5;
static constexpr int kRowAntCsc     = 6;
static constexpr int kRowAntFec     = 7;

UiManager::UiManager(M5Canvas& canvas, AntManager& ant, BleManager& ble,
                     UnifiedScanner& scanner, CyclePowerScanner& cpScanner)
    : _canvas(canvas), _ant(ant), _ble(ble), _scanner(scanner), _cpScanner(cpScanner) {}

const char* UiManager::modeLabel(BikeSourceMode mode) const {
    return (mode == SOURCE_SIM) ? "SIM" : "BLE";
}

const char* UiManager::typeLabel(BikeType type) const {
    switch (type) {
        case TARGET_M3: return "M3";
        case TARGET_CP: return "CP";
        default: return "--";
    }
}

void UiManager::persistMode(const SourceConfig& cfg, Preferences& prefs) {
    prefs.putUChar("source", (uint8_t)cfg.mode);
    prefs.putBool("sim_mode", cfg.mode == SOURCE_SIM);
}

void UiManager::persistTarget(const SourceConfig& cfg, Preferences& prefs) {
    prefs.putUChar("target_type", (uint8_t)cfg.targetType);
    prefs.putString("cp_addr", cfg.bleAddress);
    prefs.putString("cp_name", cfg.bleName);
    prefs.putUInt("bike_id", cfg.keiserBikeId);
}

void UiManager::handleInput(SourceConfig& cfg, ProfileConfig& profile, Preferences& prefs) {
    if (_currentView == VIEW_DASHBOARD) {
        if (M5.BtnA.wasHold()) {
            _currentView = VIEW_SETTINGS;
            _settingsCursor = 0;
            _settingsChanged = false;
            return;
        }
        if (M5.BtnA.wasClicked()) {
            // Open Sensor Picker. Start scanner if not already running so SIM
            // users still see BLE devices nearby.
            _currentView = VIEW_PICKER;
            _pickerIndex = 0;
            if (!_scanner.isRunning()) _scanner.begin();
        }
    } else if (_currentView == VIEW_PICKER) {
        if (M5.BtnA.wasHold()) {
            // Cancel. Revert temporary scanner start if we were in SIM mode.
            _currentView = VIEW_DASHBOARD;
            if (cfg.mode == SOURCE_SIM) _scanner.stop();
            return;
        }
        uint8_t count = _scanner.deviceCount();
        if (count > 0) {
            if (M5.BtnB.wasClicked()) {
                _pickerIndex = (_pickerIndex + count - 1) % count;
            }
            if (M5.BtnPWR.wasClicked()) {
                _pickerIndex = (_pickerIndex + 1) % count;
            }
            if (M5.BtnA.wasClicked()) {
                // Select + save. Force mode to BLE so the strategy can act.
                _scanner.saveBrowsed(_pickerIndex, cfg);
                if (cfg.mode == SOURCE_SIM) {
                    cfg.mode = SOURCE_BLE;
                    persistMode(cfg, prefs);
                }
                persistTarget(cfg, prefs);
                _currentView = VIEW_DASHBOARD;
            }
        }
    } else { // VIEW_SETTINGS
        if (M5.BtnA.wasHold()) {
            if (_settingsChanged) {
                prefs.putBool("ant_pwr", profile.ant_pwr);
                prefs.putBool("ant_csc", profile.ant_csc);
                prefs.putBool("ant_fec", profile.ant_fec);
                prefs.putBool("ble_cp", profile.ble_cp);
                prefs.putBool("ble_csc", profile.ble_csc);
                prefs.putBool("ble_ftms", profile.ble_ftms);

                _canvas.fillScreen(TFT_BLACK);
                _canvas.setTextColor(TFT_WHITE);
                _canvas.setFont(&fonts::FreeSans9pt7b);
                _canvas.drawCenterString("SAVING &", _canvas.width()/2, 30);
                _canvas.drawCenterString("REBOOTING...", _canvas.width()/2, 60);
                _canvas.pushSprite(0, 0);
                delay(1000);
                ESP.restart();
            }
            _currentView = VIEW_DASHBOARD;
            return;
        }
        if (M5.BtnB.wasClicked()) {
            _settingsCursor = (_settingsCursor + kSettingsRowCount - 1) % kSettingsRowCount;
        }
        if (M5.BtnPWR.wasClicked()) {
            _settingsCursor = (_settingsCursor + 1) % kSettingsRowCount;
        }
        if (M5.BtnA.wasClicked()) {
            switch (_settingsCursor) {
                case kRowSourceMode:
                    cfg.mode = (cfg.mode == SOURCE_SIM) ? SOURCE_BLE : SOURCE_SIM;
                    persistMode(cfg, prefs);
                    break;
                case kRowAntPwr:  profile.ant_pwr  = !profile.ant_pwr;  _settingsChanged = true; break;
                case kRowAntCsc:  profile.ant_csc  = !profile.ant_csc;  _settingsChanged = true; break;
                case kRowAntFec:  profile.ant_fec  = !profile.ant_fec;  _settingsChanged = true; break;
                case kRowBleCp:   profile.ble_cp   = !profile.ble_cp;   _settingsChanged = true; break;
                case kRowBleCsc:  profile.ble_csc  = !profile.ble_csc;  _settingsChanged = true; break;
                case kRowBleFtms: profile.ble_ftms = !profile.ble_ftms; _settingsChanged = true; break;
                case kRowBrightness:
                    profile.brightness = (profile.brightness % 5) + 1;
                    M5.Display.setBrightness(brightness_to_level(profile.brightness));
                    prefs.putUChar("brightness", profile.brightness);
                    break;
            }
        }
    }
}

void UiManager::update(const Encoder& enc, const SourceConfig& cfg, const SystemStatus& status, ProfileConfig& profile) {
    if (millis() - _lastUiUpdate < 100) return;
    _lastUiUpdate = millis();

    _canvas.fillScreen(TFT_BLACK);

    if (_currentView == VIEW_DASHBOARD) {
        renderDashboard(enc, cfg, status);
    } else if (_currentView == VIEW_PICKER) {
        renderPicker(cfg);
    } else {
        renderSettings(cfg, profile);
    }

    _canvas.pushSprite(0, 0);
}

void UiManager::renderDashboard(const Encoder& enc, const SourceConfig& cfg, const SystemStatus& status) {
    bool isSim = cfg.mode == SOURCE_SIM;
    bool hasSignal = isSim || status.sensorConnected;
    uint32_t statusColor = isSim ? TFT_BLUE : (hasSignal ? TFT_DARKGREEN : TFT_YELLOW);

    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.fillRect(0, 0, _canvas.width(), 24, statusColor);
    _canvas.setTextColor(TFT_WHITE, statusColor);
    char sourceText[12];
    snprintf(sourceText, sizeof(sourceText), " %s", modeLabel(cfg.mode));
    _canvas.drawString(sourceText, 4, 3);

    char targetLbl[20];
    if (cfg.mode == SOURCE_SIM) {
        snprintf(targetLbl, sizeof(targetLbl), "LOCAL");
    } else if (cfg.targetType == TARGET_M3) {
        if (cfg.keiserBikeId == 0) snprintf(targetLbl, sizeof(targetLbl), "M3 ANY");
        else snprintf(targetLbl, sizeof(targetLbl), "M3 #%u", (unsigned)cfg.keiserBikeId);
    } else if (cfg.targetType == TARGET_CP) {
        if (cfg.bleName[0]) snprintf(targetLbl, sizeof(targetLbl), "%.17s", cfg.bleName);
        else snprintf(targetLbl, sizeof(targetLbl), "CP SAVED");
    } else {
        snprintf(targetLbl, sizeof(targetLbl), "BLE NO PICK");
    }
    _canvas.drawCenterString(targetLbl, _canvas.width() / 2, 3);

    char batLbl[8];
    snprintf(batLbl, sizeof(batLbl), "%d%%", status.batteryLevel);
    _canvas.drawRightString(batLbl, _canvas.width() - 4, 3);

    if (hasSignal) {
        _canvas.setFont(&fonts::FreeSansBold18pt7b);
        _canvas.setTextColor(TFT_YELLOW);
        _canvas.setCursor(10, 32);
        _canvas.printf("%d", enc.power_w());
        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.drawString("W", _canvas.getCursorX() + 4, 49);

        _canvas.setFont(&fonts::FreeSansBold18pt7b);
        _canvas.setTextColor(TFT_ORANGE);
        _canvas.setCursor(130, 32);
        _canvas.printf("%d", enc.cadence_rpm());
        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.drawString("RPM", _canvas.getCursorX() + 4, 49);

        _canvas.setFont(&fonts::FreeSansBold18pt7b);
        _canvas.setTextColor(TFT_GREENYELLOW);
        _canvas.setCursor(10, 72);
        _canvas.printf("%.1f", enc.speed_mps() * 2.2369f);
        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.drawString("mph", _canvas.getCursorX() + 4, 89);
    } else {
        char msg[32];
        if (cfg.targetType == TARGET_NONE) {
            snprintf(msg, sizeof(msg), "PRESS A TO PICK");
        } else if (cfg.targetType == TARGET_M3) {
            snprintf(msg, sizeof(msg), "SEARCHING M3 #%u", (unsigned)cfg.keiserBikeId);
        } else {
            snprintf(msg, sizeof(msg), _cpScanner.connecting() ? "CONNECTING BLE CP" : "SEARCHING BLE CP");
        }

        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.setTextColor(TFT_RED);
        _canvas.drawCenterString(msg, _canvas.width() / 2, 40);

        _canvas.setTextColor(TFT_LIGHTGRAY);
        _canvas.drawCenterString("A: pick   hold A: settings", _canvas.width() / 2, 70);
    }

    _canvas.drawFastHLine(0, 110, _canvas.width(), TFT_DARKGRAY);
    _canvas.setFont(nullptr);
    _canvas.setTextSize(1);
    _canvas.setTextColor(TFT_LIGHTGRAY);
    _canvas.setCursor(6, 116);
    _canvas.printf("ANT#%u  BLE#%04X", _ant.getDeviceId(), _ant.getDeviceId());

    static uint8_t dotAnim = 0;
    dotAnim++;
    if (hasSignal) {
        _canvas.fillCircle(_canvas.width() - 15, 120, 4, (dotAnim % 10 < 5) ? statusColor : TFT_BLACK);
    }
    if (status.bleConnected) _canvas.fillCircle(_canvas.width() - 35, 120, 4, TFT_BLUE);
    else _canvas.drawCircle(_canvas.width() - 35, 120, 4, TFT_DARKGRAY);
}

void UiManager::renderPicker(const SourceConfig& cfg) {
    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    _canvas.fillRect(0, 0, _canvas.width(), 24, TFT_DARKCYAN);
    _canvas.drawString(" PICK SENSOR", 4, 3);

    uint8_t count = _scanner.deviceCount();
    if (count == 0) {
        _canvas.setTextColor(TFT_YELLOW);
        _canvas.drawCenterString("Scanning...", _canvas.width() / 2, 50);
        _canvas.setTextColor(TFT_LIGHTGRAY);
        _canvas.drawCenterString("hold A: cancel", _canvas.width() / 2, 90);
        return;
    }

    if (_pickerIndex >= count) _pickerIndex = 0;
    const ScanEntry& e = _scanner.entry(_pickerIndex);

    _canvas.setTextColor(TFT_GREENYELLOW);
    char line1[28];
    if (e.type == TARGET_M3) {
        snprintf(line1, sizeof(line1), "M3 \"%.14s\" #%u", e.name, e.m3BikeId);
    } else {
        snprintf(line1, sizeof(line1), "CP \"%.18s\"", e.name);
    }
    _canvas.drawString(line1, 10, 36);

    _canvas.setFont(nullptr);
    _canvas.setTextSize(1);
    _canvas.setTextColor(TFT_LIGHTGRAY);
    _canvas.setCursor(10, 62);
    _canvas.print(e.address);

    bool isSaved = cfg.targetType == e.type &&
                   strncmp(cfg.bleAddress, e.address, sizeof(cfg.bleAddress)) == 0 &&
                   (e.type != TARGET_M3 || cfg.keiserBikeId == e.m3BikeId);

    _canvas.setCursor(10, 80);
    _canvas.printf("%u/%u", (unsigned)(_pickerIndex + 1), (unsigned)count);
    if (isSaved) {
        _canvas.setTextColor(TFT_GREEN);
        _canvas.drawString("SAVED", _canvas.width() - 42, 80);
    }

    _canvas.drawFastHLine(0, 100, _canvas.width(), TFT_DARKGRAY);
    _canvas.setTextColor(TFT_LIGHTGRAY);
    _canvas.setCursor(6, 106);
    _canvas.print("B/PWR scroll");
    _canvas.setCursor(6, 118);
    _canvas.print("A: connect | hold A: cancel");
}

void UiManager::renderSettings(const SourceConfig& cfg, const ProfileConfig& profile) {
    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.setTextColor(TFT_WHITE, TFT_BLUE);
    _canvas.fillRect(0, 0, _canvas.width(), 24, TFT_BLUE);
    _canvas.drawString(" SETTINGS", 4, 3);

    const char* names[kSettingsRowCount] = {
        "Brightness", "Source", "BLE CP", "BLE CSC", "BLE FTMS", "ANT+ PWR", "ANT+ CSC", "ANT+ FEC"
    };

    int drawTop = _settingsCursor < 4 ? 0 : (_settingsCursor - 3);
    if (drawTop > kSettingsRowCount - 4) drawTop = kSettingsRowCount - 4;
    for (int i = 0; i < 4; i++) {
        int idx = drawTop + i;
        if (idx >= kSettingsRowCount) break;

        int y = 30 + i * 22;
        if (idx == _settingsCursor) {
            _canvas.fillRect(0, y - 2, _canvas.width(), 22, TFT_DARKGRAY);
        }

        _canvas.setTextColor(TFT_WHITE);
        _canvas.drawString(names[idx], 10, y);

        if (idx == kRowBrightness) {
            char pips[6];
            for (int p = 0; p < 5; p++) pips[p] = (p < profile.brightness) ? '|' : '.';
            pips[5] = '\0';
            _canvas.setTextColor(TFT_CYAN);
            _canvas.drawString(pips, _canvas.width() - 55, y);
        } else if (idx == kRowSourceMode) {
            _canvas.setTextColor(TFT_GREENYELLOW);
            _canvas.drawString(modeLabel(cfg.mode), _canvas.width() - 45, y);
        } else {
            bool val = false;
            switch (idx) {
                case kRowAntPwr:  val = profile.ant_pwr;  break;
                case kRowAntCsc:  val = profile.ant_csc;  break;
                case kRowAntFec:  val = profile.ant_fec;  break;
                case kRowBleCp:   val = profile.ble_cp;   break;
                case kRowBleCsc:  val = profile.ble_csc;  break;
                case kRowBleFtms: val = profile.ble_ftms; break;
            }
            _canvas.setTextColor(val ? TFT_GREEN : TFT_RED);
            _canvas.drawString(val ? "ON" : "OFF", _canvas.width() - 45, y);
        }
    }

    _canvas.drawFastHLine(0, 118, _canvas.width(), TFT_DARKGRAY);
    _canvas.setFont(nullptr);
    _canvas.setTextColor(TFT_LIGHTGRAY);
    _canvas.drawString("Hold A: Exit", 10, 122);
    _canvas.drawString("B: Up | PWR: Down", _canvas.width() - 110, 122);
}
