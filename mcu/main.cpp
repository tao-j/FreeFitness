// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#include <M5Unified.h>
#include "bike/sim.h"
#include "tx/ant_plus.h"
#include "tx/ble_gatt.h"
#include "bike/keiser.h"
#include "ui_manager.h"
#include <Preferences.h>
#include <esp_flash.h>
#include <esp_mac.h>

// Global instances for system managers
Simulator sim;
KeiserScanner keiser;
AntManager antManager;
BleManager bleManager;
M5Canvas canvas(&M5.Display); // Off-screen buffer to prevent flickering
Preferences preferences;
UiManager ui(canvas, antManager, bleManager);

ProfileConfig activeConfig;

#define SIGNAL_TIMEOUT_MS 2000 // Time to wait before declaring signal lost

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    // Initialize Display and Canvas Buffer
    M5.Display.setRotation(1);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.setTextDatum(top_left);
    
    Serial.begin(115200);
    Serial.println("\n--- Hardware Identification ---");
    
    // 1. MAC Addresses
    uint64_t baseMac = ESP.getEfuseMac();
    Serial.printf("Base MAC:    %04X%08X\n", (uint16_t)(baseMac >> 32), (uint32_t)baseMac);
    
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    Serial.printf("Wi-Fi STA:   %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    esp_read_mac(mac, ESP_MAC_BT);
    Serial.printf("Bluetooth:  %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // 2. Chip Revision & Model
    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
    Serial.printf("Cores: %d\n", ESP.getChipCores());
    
    // 3. Flash Info
    uint32_t flashId = 0;
    uint64_t uniqueId = 0;
    esp_flash_read_id(NULL, &flashId);
    esp_flash_read_unique_chip_id(NULL, &uniqueId);
    Serial.printf("Flash ID: %08X\n", flashId);
    Serial.printf("Flash Unique ID: %08X%08X\n", (uint32_t)(uniqueId >> 32), (uint32_t)uniqueId);
    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("Flash Speed: %d MHz\n", ESP.getFlashChipSpeed() / 1000000);

    Serial.println("--- System Startup (Buffered UI) ---");

    // Initialize BLE Server (GATT) - Acts as the fitness sensor
    bleManager.begin(sim.getMutableData(), activeConfig);

    // Initialize BLE Scanner (Listener) - Listens for the real Keiser bike
    BikeData& initialData = sim.getMutableData();
    initialData.lastDataTime = millis() - (SIGNAL_TIMEOUT_MS + 100); 

    // Load Settings from NVM
    preferences.begin("freefitness", false);
    initialData.targetBikeId = preferences.getUInt("bike_id", 0);
    initialData.isSimMode = preferences.getBool("sim_mode", true);
    initialData.sourceName = initialData.isSimMode ? "SIM" : "REAL";

    activeConfig.ant_pwr = preferences.getBool("ant_pwr", true);
    activeConfig.ant_csc = preferences.getBool("ant_csc", false);
    activeConfig.ant_fec = preferences.getBool("ant_fec", false);
    activeConfig.ble_cp = preferences.getBool("ble_cp", true);
    activeConfig.ble_csc = preferences.getBool("ble_csc", false);
    activeConfig.ble_ftms = preferences.getBool("ble_ftms", false);

    Serial.printf("Settings Loaded: BikeID=%d, SimMode=%d\n", initialData.targetBikeId, initialData.isSimMode);

    keiser.begin(initialData);

    // Initialize ANT+ Module (24AP2 via UART)
    pinMode(36, INPUT);
    gpio_pulldown_dis(GPIO_NUM_36);
    gpio_pullup_dis(GPIO_NUM_36);
    Serial1.begin(9600, SERIAL_8N1, 26, 25);
    antManager.begin(Serial1, activeConfig);

    // Immediate Battery Check to avoid default 100
    initialData.batteryLevel = M5.Power.getBatteryLevel();
}

void loop() {
    M5.update();
    BikeData& data = sim.getMutableData();

    // UI Input Handling
    ui.handleInput(data, activeConfig, preferences);

    // 1. Parse incoming ANT responses
    antManager.parse(data);

    // 2. Data Update & Broadcast Logic (4Hz Target)
    static uint32_t last_tx = 0;
    if (millis() - last_tx > 250) {
        uint32_t now = millis();
        uint32_t dt = now - last_tx;
        last_tx = now;
        
        if (data.isSimMode) sim.update(dt);
        
        bool hasSignal = data.isSimMode || (now - data.lastDataTime < SIGNAL_TIMEOUT_MS);
        static bool wasSignal = true;

        if (hasSignal) {
            if (!wasSignal) {
                antManager.openChannel();
                bleManager.startAdvertising();
            }
            antManager.update(data);
            bleManager.update(data); 
            wasSignal = true;
        } else if (wasSignal) {
            antManager.closeChannel();
            bleManager.stopAdvertising();
            wasSignal = false;
        }
    }

    // 3. System Management: Update Battery Level every 10s
    static uint32_t last_bat = 0;
    if (millis() - last_bat > 10000) {
        last_bat = millis();
        data.batteryLevel = M5.Power.getBatteryLevel();
        bleManager.updateBattery(data.batteryLevel);
    }

    // 4. UI Rendering
    ui.update(data, activeConfig);
}
