// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
//
// nRF52840 Arduino Cycling Power peripheral simulator.
//
// This intentionally mirrors macos/main.py: advertise only the Cycling Power
// service, publish feature + sensor-location characteristics, and notify a
// 4 Hz Cycling Power Measurement with instantaneous power, wheel revolution
// data, and crank revolution data.
//
// Build with -DENABLE_WHEEL_REV to include wheel revolution data (default: off).

#include <math.h>
#include <stdint.h>

#include <Arduino.h>
#include <bluefruit.h>
#include "bridge.h"

static constexpr const char* DEVICE_NAME = "FreeFitness CP Sim";
static constexpr uint8_t MAX_PRPH_CONNECTIONS = 3;
static constexpr uint16_t CP_SERVICE_UUID = 0x1818;
static constexpr uint16_t CP_MEASUREMENT_UUID = 0x2A63;
static constexpr uint16_t CP_FEATURE_UUID = 0x2A65;
static constexpr uint16_t SENSOR_LOCATION_UUID = 0x2A5D;

static constexpr uint16_t CP_M_CRANK_REV = 0x0020;
static constexpr uint32_t CP_F_CRANK_REV = 0x00000008;
static constexpr uint8_t SENSOR_LOCATION_REAR_WHEEL = 12;

#ifdef ENABLE_WHEEL_REV
static constexpr uint16_t CP_M_WHEEL_REV = 0x0010;
static constexpr uint32_t CP_F_WHEEL_REV = 0x00000004;
static constexpr float WHEEL_CIRCUMFERENCE_M = 2.096f;
static constexpr uint16_t PAYLOAD_LEN = 14;
#else
static constexpr uint16_t PAYLOAD_LEN = 8;
#endif

static BLEService cp_service(CP_SERVICE_UUID);
static BLECharacteristic cp_measurement(CP_MEASUREMENT_UUID);
static BLECharacteristic cp_feature_char(CP_FEATURE_UUID);
static BLECharacteristic sensor_location_char(SENSOR_LOCATION_UUID);

static float rand_pm(float amplitude) {
    int32_t bucket = random(-1000, 1001);
    return (float)bucket / 1000.0f * amplitude;
}

struct SimState {
    uint16_t power = 150;
#ifdef ENABLE_WHEEL_REV
    uint32_t wheel_revs = 0;
    uint16_t wheel_tick_1024 = 0;
#endif
    uint16_t crank_revs = 0;
    uint16_t crank_tick = 0;
};

static SimState sim;
#ifdef ENABLE_WHEEL_REV
static RateEventBridge wheel_bridge;
#endif
static float rpm = 70.0f;
static float power_w = 150.0f;
static int32_t time_to_next_crank_rev_ms = 0;
static volatile bool advertising_update_pending = false;

static void build_measurement(uint8_t payload[PAYLOAD_LEN]) {
    uint16_t flags = CP_M_CRANK_REV;
#ifdef ENABLE_WHEEL_REV
    flags |= CP_M_WHEEL_REV;
#endif
    payload[0] = (uint8_t)(flags & 0xFF);
    payload[1] = (uint8_t)(flags >> 8);
    payload[2] = (uint8_t)(sim.power & 0xFF);
    payload[3] = (uint8_t)(sim.power >> 8);
#ifdef ENABLE_WHEEL_REV
    uint16_t wheel_tick_2048 = (uint16_t)(sim.wheel_tick_1024 * 2U);
    payload[4] = (uint8_t)(sim.wheel_revs & 0xFF);
    payload[5] = (uint8_t)((sim.wheel_revs >> 8) & 0xFF);
    payload[6] = (uint8_t)((sim.wheel_revs >> 16) & 0xFF);
    payload[7] = (uint8_t)((sim.wheel_revs >> 24) & 0xFF);
    payload[8] = (uint8_t)(wheel_tick_2048 & 0xFF);
    payload[9] = (uint8_t)(wheel_tick_2048 >> 8);
    payload[10] = (uint8_t)(sim.crank_revs & 0xFF);
    payload[11] = (uint8_t)(sim.crank_revs >> 8);
    payload[12] = (uint8_t)(sim.crank_tick & 0xFF);
    payload[13] = (uint8_t)(sim.crank_tick >> 8);
#else
    payload[4] = (uint8_t)(sim.crank_revs & 0xFF);
    payload[5] = (uint8_t)(sim.crank_revs >> 8);
    payload[6] = (uint8_t)(sim.crank_tick & 0xFF);
    payload[7] = (uint8_t)(sim.crank_tick >> 8);
#endif
}

static void update_sim(uint32_t dt_ms) {
    uint32_t now_tick = get_tick_now();

    rpm = fminf(95.0f, fmaxf(50.0f, rpm + rand_pm(0.6f)));
    power_w = fminf(240.0f, fmaxf(80.0f, power_w + rand_pm(4.0f)));

    time_to_next_crank_rev_ms -= (int32_t)dt_ms;
    while (time_to_next_crank_rev_ms <= 0) {
        int32_t overshoot_ms = -time_to_next_crank_rev_ms;
        uint32_t event_tick = now_tick - (uint32_t)((int64_t)overshoot_ms * 1024 / 1000);
        sim.crank_revs++;
        sim.crank_tick = (uint16_t)event_tick;
        time_to_next_crank_rev_ms += (int32_t)(60000.0f / rpm);
    }

#ifdef ENABLE_WHEEL_REV
    float speed_mps = power_to_speed(power_w);
    wheel_bridge.feed_rate(speed_mps / WHEEL_CIRCUMFERENCE_M, now_tick);
    sim.wheel_revs = wheel_bridge.count_int();
    sim.wheel_tick_1024 = (uint16_t)wheel_bridge.event_tick();
#endif
    sim.power = (uint16_t)power_w;
}

static void start_advertising() {
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(cp_service);
    Bluefruit.ScanResponse.addName();

    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.start(0);
}

static void update_advertising_for_capacity() {
    if (Bluefruit.Periph.connected() < MAX_PRPH_CONNECTIONS) {
        Bluefruit.Advertising.start(0);
    }
}

static void connect_callback(uint16_t conn_hdl) {
    Serial.print("BLE connected: handle ");
    Serial.print(conn_hdl);
    Serial.print(" total ");
    Serial.println(Bluefruit.Periph.connected());
    advertising_update_pending = true;
}

static void disconnect_callback(uint16_t conn_hdl, uint8_t reason) {
    Serial.print("BLE disconnected: handle ");
    Serial.print(conn_hdl);
    Serial.print(" reason ");
    Serial.println(reason);
    advertising_update_pending = true;
}

static void notify_connected_centrals(const uint8_t* payload, uint16_t len) {
    for (uint8_t conn_hdl = 0; conn_hdl < MAX_PRPH_CONNECTIONS; conn_hdl++) {
        if (Bluefruit.Periph.connected(conn_hdl)) {
            cp_measurement.notify(conn_hdl, payload, len);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);

    randomSeed(micros());

    Bluefruit.begin(MAX_PRPH_CONNECTIONS, 0);
    Bluefruit.setName(DEVICE_NAME);
    Bluefruit.setTxPower(4);
    Bluefruit.Periph.setConnectCallback(connect_callback);
    Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

    cp_service.begin();

    cp_measurement.setProperties(CHR_PROPS_NOTIFY);
    cp_measurement.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    cp_measurement.setFixedLen(PAYLOAD_LEN);
    cp_measurement.begin();

    uint32_t cp_feature = CP_F_CRANK_REV;
#ifdef ENABLE_WHEEL_REV
    cp_feature |= CP_F_WHEEL_REV;
#endif
    cp_feature_char.setProperties(CHR_PROPS_READ);
    cp_feature_char.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    cp_feature_char.setFixedLen(4);
    cp_feature_char.begin();
    cp_feature_char.write(&cp_feature, sizeof(cp_feature));

    uint8_t sensor_location = SENSOR_LOCATION_REAR_WHEEL;
    sensor_location_char.setProperties(CHR_PROPS_READ);
    sensor_location_char.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    sensor_location_char.setFixedLen(1);
    sensor_location_char.begin();
    sensor_location_char.write8(sensor_location);

    start_advertising();
    Serial.print("Advertising ");
    Serial.print(DEVICE_NAME);
    Serial.println(" with Cycling Power service");
}

void loop() {
    static uint32_t last_ms = millis();
    uint32_t now_ms = millis();
    uint32_t dt_ms = now_ms - last_ms;

    if (advertising_update_pending) {
        advertising_update_pending = false;
        update_advertising_for_capacity();
    }

    if (dt_ms < 250) {
        delay(5);
        return;
    }

    last_ms = now_ms;
    update_sim(dt_ms);

    uint8_t payload[PAYLOAD_LEN];
    build_measurement(payload);
    notify_connected_centrals(payload, sizeof(payload));

    Serial.print("CP | pwr ");
    Serial.print(sim.power);
    Serial.print(" W |");
#ifdef ENABLE_WHEEL_REV
    Serial.print(" wheel ");
    Serial.print(sim.wheel_revs);
    Serial.print(" rev @ ");
    Serial.print(sim.wheel_tick_1024);
    Serial.print(" tk |");
#endif
    Serial.print(" crank ");
    Serial.print(sim.crank_revs);
    Serial.print(" rev @ ");
    Serial.print(sim.crank_tick);
    Serial.println(" tk");
}
