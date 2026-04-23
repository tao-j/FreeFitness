#include "ui_manager.h"

#define SIGNAL_TIMEOUT_MS 2000

UiManager::UiManager(M5Canvas& canvas, AntManager& ant, BleManager& ble) 
    : _canvas(canvas), _ant(ant), _ble(ble) {}

void UiManager::handleInput(BikeData& data, ProfileConfig& config, Preferences& prefs) {
    if (M5.BtnA.wasHold()) {
        if (_currentView == VIEW_DASHBOARD) {
            _currentView = VIEW_SETTINGS;
            _settingsCursor = 0;
            _settingsChanged = false;
        } else {
            if (_settingsChanged) {
                // Save and Reboot
                prefs.putBool("ant_pwr", config.ant_pwr);
                prefs.putBool("ant_csc", config.ant_csc);
                prefs.putBool("ant_fec", config.ant_fec);
                prefs.putBool("ble_cp", config.ble_cp);
                prefs.putBool("ble_csc", config.ble_csc);
                prefs.putBool("ble_ftms", config.ble_ftms);
                
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
        }
        return; // Don't process other buttons if view changed
    }

    if (_currentView == VIEW_DASHBOARD) {
        if (M5.BtnA.wasClicked()) {
            data.isSimMode = !data.isSimMode;
            data.sourceName = data.isSimMode ? "SIM" : "REAL";
            prefs.putBool("sim_mode", data.isSimMode);
        }
        if (M5.BtnB.wasClicked()) {
            data.targetBikeId++;
            prefs.putUInt("bike_id", data.targetBikeId);
        }
        if (M5.BtnPWR.wasClicked()) {
            if (data.targetBikeId > 0) {
                data.targetBikeId--;
                prefs.putUInt("bike_id", data.targetBikeId);
            }
        }
        if (M5.BtnB.wasHold()) {
            data.targetBikeId = 0;
            prefs.putUInt("bike_id", 0);
        }
    } else {
        // Settings Mode - Swapped Navigation
        if (M5.BtnB.wasClicked()) { // Now UP
            _settingsCursor = (_settingsCursor + 5) % 6;
        }
        if (M5.BtnPWR.wasClicked()) { // Now DOWN
            _settingsCursor = (_settingsCursor + 1) % 6;
        }
        if (M5.BtnA.wasClicked()) {
            _settingsChanged = true;
            switch(_settingsCursor) {
                case 0: config.ant_pwr = !config.ant_pwr; break;
                case 1: config.ant_csc = !config.ant_csc; break;
                case 2: config.ant_fec = !config.ant_fec; break;
                case 3: config.ble_cp = !config.ble_cp; break;
                case 4: config.ble_csc = !config.ble_csc; break;
                case 5: config.ble_ftms = !config.ble_ftms; break;
            }
        }
    }
}

void UiManager::update(BikeData& data, ProfileConfig& config) {
    if (millis() - _lastUiUpdate < 100) return;
    _lastUiUpdate = millis();

    _canvas.fillScreen(TFT_BLACK);

    if (_currentView == VIEW_DASHBOARD) {
        renderDashboard(data);
    } else {
        renderSettings(config);
    }

    _canvas.pushSprite(0, 0);
}

void UiManager::renderDashboard(const BikeData& data) {
    bool hasSignal = data.isSimMode || (millis() - data.lastDataTime < SIGNAL_TIMEOUT_MS);
    uint32_t statusColor = data.isSimMode ? TFT_BLUE : (hasSignal ? TFT_DARKGREEN : TFT_YELLOW);

    // Header: status | BIKE# | battery
    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.fillRect(0, 0, _canvas.width(), 24, statusColor);
    _canvas.setTextColor(TFT_WHITE, statusColor);
    _canvas.drawString(data.isSimMode ? " SIM" : " REAL", 4, 3);

    char bikeLbl[16];
    if (data.targetBikeId == 0) snprintf(bikeLbl, sizeof(bikeLbl), "BIKE#ANY");
    else snprintf(bikeLbl, sizeof(bikeLbl), "BIKE#%u", (unsigned)data.targetBikeId);
    _canvas.drawCenterString(bikeLbl, _canvas.width() / 2, 3);

    char batLbl[8];
    snprintf(batLbl, sizeof(batLbl), "%d%%", data.batteryLevel);
    _canvas.drawRightString(batLbl, _canvas.width() - 4, 3);

    if (hasSignal) {
        // Power
        _canvas.setFont(&fonts::FreeSansBold18pt7b);
        _canvas.setTextColor(TFT_YELLOW);
        _canvas.setCursor(10, 32);
        _canvas.printf("%d", data.power);
        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.drawString("W", _canvas.getCursorX() + 4, 49);

        // Cadence
        _canvas.setFont(&fonts::FreeSansBold18pt7b);
        _canvas.setTextColor(TFT_ORANGE);
        _canvas.setCursor(130, 32);
        _canvas.printf("%d", data.cadence);
        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.drawString("RPM", _canvas.getCursorX() + 4, 49);

        // Speed
        _canvas.setFont(&fonts::FreeSansBold18pt7b);
        _canvas.setTextColor(TFT_GREENYELLOW);
        _canvas.setCursor(10, 72);
        _canvas.printf("%.1f", data.speed_mps * 2.2369f);
        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.drawString("mph", _canvas.getCursorX() + 4, 89);
    } else {
        // Searching: replace metrics area with status + button hint.
        char msg[32];
        if (data.targetBikeId == 0) snprintf(msg, sizeof(msg), "SEARCHING ANY BIKE");
        else snprintf(msg, sizeof(msg), "SEARCHING BIKE #%u", (unsigned)data.targetBikeId);

        _canvas.setFont(&fonts::FreeSans9pt7b);
        _canvas.setTextColor(TFT_RED);
        _canvas.drawCenterString(msg, _canvas.width() / 2, 40);

        _canvas.setTextColor(TFT_LIGHTGRAY);
        _canvas.drawCenterString("B:+  PWR:-  hold B:ANY", _canvas.width() / 2, 70);
    }

    // Footer: small built-in font for ANT#/BLE#
    _canvas.drawFastHLine(0, 110, _canvas.width(), TFT_DARKGRAY);
    _canvas.setFont(nullptr);
    _canvas.setTextSize(1);
    _canvas.setTextColor(TFT_LIGHTGRAY);
    _canvas.setCursor(6, 116);
    _canvas.printf("ANT#%u  BLE#%04X", _ant.getDeviceId(), _ant.getDeviceId());

    // Indicators
    static uint8_t dotAnim = 0;
    dotAnim++;
    if (hasSignal) {
        _canvas.fillCircle(_canvas.width() - 15, 120, 4, (dotAnim % 10 < 5) ? statusColor : TFT_BLACK);
    }
    if (data.bleConnected) _canvas.fillCircle(_canvas.width() - 35, 120, 4, TFT_BLUE);
    else _canvas.drawCircle(_canvas.width() - 35, 120, 4, TFT_DARKGRAY);
}

void UiManager::renderSettings(const ProfileConfig& config) {
    _canvas.setFont(&fonts::FreeSans9pt7b);
    _canvas.setTextColor(TFT_WHITE, TFT_BLUE);
    _canvas.fillRect(0, 0, _canvas.width(), 24, TFT_BLUE);
    _canvas.drawString(" SETTINGS", 4, 3);
    
    const char* names[] = {"ANT+ PWR", "ANT+ CSC", "ANT+ FEC", "BLE CP", "BLE CSC", "BLE FTMS"};
    bool vals[] = {config.ant_pwr, config.ant_csc, config.ant_fec, config.ble_cp, config.ble_csc, config.ble_ftms};
    
    int drawTop = _settingsCursor < 4 ? 0 : 2;
    for (int i=0; i<4; i++) {
        int idx = drawTop + i;
        if (idx >= 6) break;
        
        int y = 30 + i * 22;
        if (idx == _settingsCursor) {
            _canvas.fillRect(0, y - 2, _canvas.width(), 22, TFT_DARKGRAY);
        }
        
        _canvas.setTextColor(TFT_WHITE);
        _canvas.drawString(names[idx], 10, y);
        
        bool val = vals[idx];
        _canvas.setTextColor(val ? TFT_GREEN : TFT_RED);
        _canvas.drawString(val ? "ON" : "OFF", _canvas.width() - 45, y);
    }
    
    _canvas.drawFastHLine(0, 118, _canvas.width(), TFT_DARKGRAY);
    _canvas.setFont(nullptr);
    _canvas.setTextColor(TFT_LIGHTGRAY);
    _canvas.drawString("Hold BtnA to Exit", 10, 122);
    _canvas.drawString("BtnB: Up | BtnPWR: Down", _canvas.width() - 140, 122);
}
