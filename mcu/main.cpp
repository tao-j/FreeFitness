// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include <M5Unified.h>
#include "bike/bike_data.h"
#include "bike/sim.h"
#include "bike/keiser.h"
#include "bike/cycle_power.h"
#include "bike/unified_scanner.h"
#include "source_config.h"
#include "system_status.h"
#include "tx/encoder.h"
#include "tx/ant_plus.h"
#include "tx/ble_gatt.h"
#include "ui_manager.h"
#include "bridge.h"
#include <Preferences.h>
#include <esp_flash.h>
#include <esp_mac.h>

static BikeData      bikeData;
static SourceConfig  sourceCfg;
static SystemStatus  sysStatus;
static Encoder       encoder;
static ProfileConfig activeProfile;

static Simulator         sim;
static KeiserScanner     keiser;
static CyclePowerScanner cyclePower;
static UnifiedScanner    unifiedScanner;
static AntManager        antManager;
static BleManager        bleManager;
static M5Canvas          canvas(&M5.Display);
static Preferences       preferences;
static UiManager         ui(canvas, antManager, bleManager, unifiedScanner, cyclePower);

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.setTextDatum(top_left);

    Serial.begin(115200);
    Serial.println("\n--- Hardware Identification ---");

    uint64_t baseMac = ESP.getEfuseMac();
    Serial.printf("Base MAC:    %04X%08X\n", (uint16_t)(baseMac >> 32), (uint32_t)baseMac);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    Serial.printf("Wi-Fi STA:   %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_read_mac(mac, ESP_MAC_BT);
    Serial.printf("Bluetooth:  %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
    Serial.printf("Cores: %d\n", ESP.getChipCores());

    uint32_t flashId = 0;
    uint64_t uniqueId = 0;
    esp_flash_read_id(NULL, &flashId);
    esp_flash_read_unique_chip_id(NULL, &uniqueId);
    Serial.printf("Flash ID: %08X\n", flashId);
    Serial.printf("Flash Unique ID: %08X%08X\n", (uint32_t)(uniqueId >> 32), (uint32_t)uniqueId);
    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("Flash Speed: %d MHz\n", ESP.getFlashChipSpeed() / 1000000);

    Serial.println("--- System Startup (Buffered UI) ---");

    init_power_to_speed_lut();

    bleManager.begin(sysStatus, activeProfile);

    preferences.begin("freefitness", false);

    // Migrate the old 3-value `source` key into the new SIM|BLE mode +
    // targetType pair. Old layout: 0=SIM, 1=KEISER, 2=BLE_CP.
    uint8_t savedSource = preferences.getUChar("source", preferences.getBool("sim_mode", true) ? 0 : 1);
    if (savedSource == 0) {
        sourceCfg.mode = SOURCE_SIM;
        sourceCfg.targetType = TARGET_NONE;
    } else if (savedSource == 1) {
        sourceCfg.mode = SOURCE_BLE;
        sourceCfg.targetType = TARGET_M3;
    } else {
        sourceCfg.mode = SOURCE_BLE;
        sourceCfg.targetType = TARGET_CP;
    }
    // Allow an explicit target_type override if previously saved.
    if (preferences.isKey("target_type")) {
        uint8_t t = preferences.getUChar("target_type", TARGET_NONE);
        if (t <= TARGET_CP) sourceCfg.targetType = (BikeType)t;
    }

    sourceCfg.keiserBikeId = preferences.getUInt("bike_id", 0);
    String addr = preferences.getString("cp_addr", "");
    String name = preferences.getString("cp_name", "");
    strlcpy(sourceCfg.bleAddress, addr.c_str(), sizeof(sourceCfg.bleAddress));
    strlcpy(sourceCfg.bleName, name.c_str(), sizeof(sourceCfg.bleName));

    activeProfile.ant_pwr = preferences.getBool("ant_pwr", true);
    activeProfile.ant_csc = preferences.getBool("ant_csc", false);
    activeProfile.ant_fec = preferences.getBool("ant_fec", false);
    activeProfile.ble_cp = preferences.getBool("ble_cp", true);
    activeProfile.ble_csc = preferences.getBool("ble_csc", false);
    activeProfile.ble_ftms = preferences.getBool("ble_ftms", false);
    activeProfile.brightness = preferences.getUChar("brightness", 5);
    M5.Display.setBrightness(brightness_to_level(activeProfile.brightness));

    Serial.printf("Settings Loaded: mode=%d targetType=%d bikeId=%d cpName=%s cpAddr=%s\n",
                  sourceCfg.mode, sourceCfg.targetType, sourceCfg.keiserBikeId,
                  sourceCfg.bleName, sourceCfg.bleAddress);

    keiser.begin(bikeData, sourceCfg);
    cyclePower.begin(bikeData, sourceCfg);
    unifiedScanner.attach(&keiser, &cyclePower);

    if (sourceCfg.mode == SOURCE_BLE) {
        unifiedScanner.begin();
    }

    pinMode(36, INPUT);
    gpio_pulldown_dis(GPIO_NUM_36);
    gpio_pullup_dis(GPIO_NUM_36);
    Serial1.begin(9600, SERIAL_8N1, 26, 25);
    antManager.begin(Serial1, activeProfile);

    sysStatus.batteryLevel = M5.Power.getBatteryLevel();
}

void loop() {
    M5.update();
    static BikeSourceMode activeSource = sourceCfg.mode;

    ui.handleInput(sourceCfg, activeProfile, preferences);

    if (sourceCfg.mode != activeSource) {
        if (activeSource == SOURCE_BLE) unifiedScanner.stop();
        activeSource = sourceCfg.mode;
        if (activeSource == SOURCE_BLE) unifiedScanner.begin();
    }

    antManager.parse();

    static uint32_t last_tx = 0;
    if (millis() - last_tx > 250) {
        uint32_t now = millis();
        uint32_t dt = now - last_tx;
        last_tx = now;

        if (sourceCfg.mode == SOURCE_SIM) {
            sim.update(bikeData, dt);
        } else {
            cyclePower.update();  // drains pending connects; M3 path is callback-driven
        }

        encoder.update(bikeData);

        sysStatus.sensorConnected = (sourceCfg.mode == SOURCE_SIM) || !encoder.noData();

        static bool wasSignal = true;
        if (sysStatus.sensorConnected) {
            if (!wasSignal) {
                antManager.openChannel();
                bleManager.startAdvertising();
            }
            antManager.update(encoder, sysStatus.batteryLevel);
            bleManager.update(encoder);
            wasSignal = true;
        } else if (wasSignal) {
            antManager.closeChannel();
            bleManager.stopAdvertising();
            wasSignal = false;
        }
    }

    static uint32_t last_bat = 0;
    if (millis() - last_bat > 10000) {
        last_bat = millis();
        sysStatus.batteryLevel = M5.Power.getBatteryLevel();
        bleManager.updateBattery(sysStatus.batteryLevel);
    }

    ui.update(encoder, sourceCfg, sysStatus, activeProfile);
}
