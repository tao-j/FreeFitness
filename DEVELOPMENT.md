# Development Documentation

Architecture, data structures, protocol mappings, and internal design notes for FreeFitness.

## Key Concepts
- **CP (Cycling Power)**: Standard Bluetooth service for power and cadence. Usually sufficient for most applications.
- **CSC (Cycling Speed and Cadence)**: Dedicated Bluetooth service for speed and cadence.
- **MCU (Microcontroller Unit)**: Small, low-power processors (like ESP32 or nRF52) used for standalone hardware.
- **PWR (Bicycle Power)**: The ANT+ profile for broadcasting power and cadence data.
- **SPD (Bicycle Speed)**: The ANT+ profile for broadcasting speed and distance data.

## 1. Architecture overview

The pipeline has four layers, each with one job. Source → BikeData → Encoder → tx. UI reads everything, mutates source config.

```
            Preferences (NVM)
                  │ load / save
                  ▼
       ┌──────────────────────┐
       │     SourceConfig     │◄────────┐  mutate (picker / settings)
       │  mode, targetType,   │         │
       │  bleAddress, bleName │         │
       └──────────┬───────────┘         │
                  │ read                │
                  ▼                     │
   ┌──────────────────────────────────┐ │
   │   Active source (1 of):          │ │
   │   ┌──────┐ ┌──────────────────┐  │ │
   │   │ Sim  │ │ UnifiedScanner   │  │ │
   │   │      │ │  + M3 strategy   │  │ │
   │   │      │ │  + CP strategy   │  │ │
   │   └───┬──┘ └────────┬─────────┘  │ │
   └───────┼─────────────┼────────────┘ │
           │ stamp telemetry only       │
           ▼             ▼              │
       ┌──────────────────────┐         │
       │       BikeData       │         │   ┌──────────────────┐
       │  pure telemetry      │─────────┼──►│   UiManager      │
       │  (Python BikeState)  │         │   │ reads:           │
       └──────────┬───────────┘         │   │  - Encoder       │
                  │ read                │   │  - SourceConfig  │
                  ▼                     │   │  - SystemStatus  │
       ┌──────────────────────┐         │   │ writes:          │
       │       Encoder        │         │   │  - SourceConfig  │
       │  rate↔event bridges, │         │   │    (mode, target)│
       │  wire scaling,       │         │   │  - ProfileConfig │
       │  ANT+ accumulators   │         │   └─────────┬────────┘
       └──────────┬───────────┘         │             │ button events
                  │ read latched        │             ▼
                  ▼                     │     ┌──────────────────┐
       ┌──────────┴──────────┐          │     │   SystemStatus   │
       ▼                     ▼          │     │ sensorConnected, │
  ┌──────────────────┐   ┌──────────┐   │     │ bleConnected,    │
  │ BleManager       │   │AntManager│   │     │ batteryLevel     │
  │ GATT server      │   │  (UART)  │   │     │                  │
  └──────────────────┘   └─────┬────┘   │     └────────┬─────────┘
        │ write to             │write to│              │ read by UI
        ▼                     ▼         │              ▲ derived
       ┌──────────────────────┐         │              │ from last_update_tick
       │    SystemStatus      │─────────┘              │
       └──────────────────────┘────────────────────────┘
```

The four interfaces, in plain words:

1. **Source → BikeData.** Source stamps only what it natively measures (rates, events, power). No bridge work, no wire scaling, no accumulators. Mirrors the contract in `linux/bike/__init__.py::BikeState`.
2. **BikeData → Encoder → tx.** Encoder owns the rate/event bridges and produces protocol-ready latched fields. The BLE CP 1/2048 s wheel-event quirk lives here, nowhere else.
3. **UI ↔ SourceConfig.** UI mutates source mode and saved target through the Sensor Picker + Settings. Persists to NVS.
4. **UI ↔ Encoder + SystemStatus.** UI reads latched display values from the encoder and liveness/connectivity from SystemStatus. `sensorConnected` is derived each tick from `bikeData.last_update_tick` vs `get_tick_now()`.

### 1.1 BikeData (telemetry, port of Python BikeState)

`mcu/bike/bike_data.h`. Pure telemetry — source-agnostic, no wire scaling, no encoder accumulators, no source config, no system status. All event ticks are 1/1024 s. `0` / `false` / `0.0` means "unknown" (no NaN on this MCU).

| Field | Type | Description |
|---|---|---|
| `cadence_rpm` | `float` | Crank rate; encoder uses it if `!has_crank_event`. |
| `speed_mps` | `float` | Wheel rate; encoder falls back to `power_to_speed(power_w)` if `!has_wheel_event && speed_mps == 0`. |
| `power_w` | `uint16_t` | Instantaneous power in Watts. |
| `has_crank_event` | `bool` | Source owns `crank_revs` / `crank_event_tick`. |
| `crank_revs` | `uint32_t` | Cumulative crank revolutions. |
| `crank_event_tick` | `uint32_t` | Tick of last crank event (1/1024 s). |
| `has_wheel_event` | `bool` | Source owns `wheel_revs` / `wheel_event_tick`. |
| `wheel_revs` | `uint32_t` | Cumulative wheel revolutions. |
| `wheel_event_tick` | `uint32_t` | Tick of last wheel event (1/1024 s — NOT wire-scaled). |
| `last_update_tick` | `uint32_t` | Tick at which the source last stamped fresh data. Silence detection compares against `get_tick_now()`. |

### 1.2 SourceConfig (persisted)

`mcu/source_config.h`. What source is selected and how it's targeted.

```cpp
enum BikeSourceMode { SOURCE_SIM, SOURCE_BLE };
enum BikeType       { TARGET_NONE, TARGET_M3, TARGET_CP };

struct SourceConfig {
    BikeSourceMode mode = SOURCE_SIM;
    BikeType targetType = TARGET_NONE;
    uint16_t keiserBikeId = 0;        // co-written with bleAddress for M3 picks
    char     bleAddress[18] = "";
    char     bleName[22] = "";
    char     connectedAddress[18] = ""; // runtime, not persisted
};
```

Two user-facing source modes: `SIM` and `BLE`. The bike *type* (M3 vs CP) is per-target metadata, set by the Sensor Picker when a device is selected — never asked separately.

`targetType == TARGET_NONE` means "BLE mode but no pick yet" — scanner runs, no auto-connect. Once the user picks from the picker, `bleAddress` is always set; for M3 picks `keiserBikeId` is co-written in the same store so the M3 strategy can do strict-match-both filtering (defeats default-bike-ID collisions in gym environments).

### 1.3 SystemStatus (runtime)

`mcu/system_status.h`. MCU-only, not persisted.

```cpp
struct SystemStatus {
    bool    sensorConnected = false; // upstream: source producing fresh data
    bool    bleConnected = false;    // downstream: phone/head-unit paired
    uint8_t batteryLevel = 100;
};
```

`sensorConnected` is the same liveness signal Python uses (`linux/tx/encoder.py` `SILENCE_TICKS`): true while fresh data arrives within the silence window, false after. Derived each main-loop tick by comparing `bikeData.last_update_tick` to `get_tick_now()` — no per-source bookkeeping. Keiser flips it true while advertisements keep arriving; BLE CP flips it true while the GATT link is alive AND fresh notifications keep arriving; SIM keeps it true while running.

## 2. Why a bridge is needed

BLE (FTMS, CSC, CP) and ANT+ (Power, Speed/Cadence) are modeled around physical hardware sensors. They require **discrete, monotonically increasing integer counts** (cumulative wheel / crank revolutions) paired with **precise event timestamps** at 1/1024-second resolution.

Incoming sources rarely give us those directly:
- **Keiser M3i** broadcasts *instantaneous rates* — cadence (RPM) and power (W).
- **Simulator** emulates a hall-effect sensor: one event per revolution + power.
- **BLE Cycling Power** sensors give CP measurement notifications — events when reported, rates otherwise.

The pipeline normalizes all three into the same two-view state so protocol encoders can read whichever half they need.

### 2.1 Rate view vs event view

Every rotating channel (crank, wheel) has two equivalent representations:
- **Rate view**: revolutions per second — continuous, what humans read.
- **Event view**: cumulative revolution count + timestamp of the most recent integer crossing (1/1024 s ticks) — what the wire formats require.

`RateEventBridge` (`mcu/bridge.cpp`) converts between the two:
- `feed_rate(rate_rps, now_tick)` integrates the rate and back-interpolates an event tick whenever the running count crosses the next integer.
- `feed_event(cum_revs, event_tick, now_tick)` adopts hardware events and derives rate from Δcount / Δtick across consecutive events.

This bidirectional fill lets event-driven sources expose cadence and rate-driven sources expose event timing without the source having to know which consumer needs what.

## 3. The Encoder layer

`mcu/tx/encoder.{h,cpp}`. Mirrors `linux/tx/encoder.py::ProtocolEncoder`. Runs once per tx tick (4 Hz from the main loop). Reads `BikeData`, fills derived fields, owns the rate/event bridges and the ANT+ power accumulators.

### 3.1 Dispatch rules

Per rotational channel, the encoder feeds whichever half the source provides:
- **Crank**: `has_crank_event` → `bridge.feed_event(...)`; else if `cadence_rpm > 0` → `bridge.feed_rate(cadence_rpm / 60, now_tick)`.
- **Wheel**: `has_wheel_event` → `bridge.feed_event(...)`; else if `speed_mps > 0` → `bridge.feed_rate(...)`; else if `power_w > 0` → derive via `power_to_speed(power_w)` then feed. Power → speed is the only physics hop in the pipeline.

### 3.2 Wire scaling

The BLE CP wheel-event-time wire format uses **1/2048 s**, while CSC and ANT+ use **1/1024 s**. The Encoder caches both: `wheel_event_tick_1024` for CSC/ANT and `wheel_event_tick_2048` for BLE CP. Sources stamp 1/1024 only; the encoder is the single place where the 1/2048 quirk lives.

### 3.3 Silence detection

If `bikeData.last_update_tick` has not advanced within ~2 s of `get_tick_now()`, the encoder flips `noData() = true` and the tx layers skip transmitting. The encoder polls on every tx tick so it can observe silence — an event-blocked encoder never would.

### 3.4 ANT+ power accumulators

`accPower` and `powerEventCount` increment once per encoder tick (4 Hz) whenever `power_w > 0`. ANT+ average power = `ΔaccPower / ΔeventCount`, so the absolute rate doesn't matter as long as both fields advance together. These are encoder-side; sources never touch them.

## 4. UnifiedScanner

`mcu/bike/unified_scanner.{h,cpp}`. One NimBLE scanner, two filters (M3 + CP) in the same callback. Runs always-on while `mode == SOURCE_BLE`.

### 4.1 Filters

- **M3 match**: 19-byte manufacturer-data payload starting with `02 01` (Keiser ID 0x0102) and `data_type == 0`.
- **CP match**: advertises service UUID `0x1818`, or known names like `FreeFitness CP Sim`.

A matching ad is upserted into `ScanEntry[]` with its `type` tag (`TARGET_M3` / `TARGET_CP`). M3 entries dedup by (address, bike_id) so default-ID collisions in a gym produce distinct entries. The Sensor Picker reads from this list.

### 4.2 Dispatch to strategies

Every recognized advertisement is dispatched to both subscriber strategies:
- `KeiserScanner::onAdvertisement(dev, m3BikeId, payload)` — parses cadence + power from the M3 payload and stamps BikeData, gated on `targetType == TARGET_M3` and strict address+bike-ID match.
- `CyclePowerScanner::onAdvertisement(dev)` — gated on `targetType == TARGET_CP` and address/name match; schedules a deferred GATT connect via `_pPendingConnect`. The main-loop `update()` drains the pending connect (never call `NimBLEClient::connect()` from a scan callback — see § 7).

### 4.3 Lifecycle

- `mode == SOURCE_BLE` on boot or via Settings → `unifiedScanner.begin()`.
- `mode == SOURCE_SIM` → `unifiedScanner.stop()`.
- The Sensor Picker can temporarily start the scanner from SIM mode so users see nearby BLE bikes; if the picker is cancelled in SIM mode the scanner stops again. Selecting a sensor auto-flips mode to BLE so the scanner stays on.

## 5. Power-to-speed lookup table

`mcu/bridge.cpp` builds a 2000-entry `float[]` LUT at boot via `init_power_to_speed_lut()`. The runtime cubic `cbrtf()` (soft-float on Xtensa) was the dominant cost on the hot path; the LUT replaces it with one array lookup.

- **Indexed by integer watts** (0..1999). Power on BLE CP / ANT+ wires is integer, so no interpolation needed. `BikeData::power_w` is `uint16_t` — passes directly.
- **Float output** because the encoder's wheel bridge accumulates revolutions in fractional rps. ESP32-S3 has a hardware FPU, so float arithmetic is single-cycle; the win is dropping `cbrtf()` specifically.
- **2000 entries** is well above real-world peak power; values ≥ 2000 W clamp to entry 1999.
- **Boot cost** is well under 10 ms — 2000 cubic solves at startup. Invisible.

The cubic itself is unchanged: solves P_drag·v^3 + P_roll·v − P = 0 for v (m/s) given Cd=0.9, A=0.5, ρ=1.225, Crr=0.0045, m=75 kg.

## 6. Keiser M3i BLE advertisement format

The Keiser M3i bike broadcasts its data in the **Manufacturer Specific Data (MSD)** field of the BLE advertisement.

| Offset | Size (Bytes) | Field | Description |
| :--- | :--- | :--- | :--- |
| - | 2 | Manuf. ID | `0x0102` (Little Endian: `02 01`) |
| 0 | 1 | Version Major | Major firmware version. |
| 1 | 1 | Version Minor | Minor firmware version. |
| 2 | 1 | Data Type | `0x00` (Real-time data). |
| 3 | 1 | Bike ID | Ordinal ID. |
| 4-5 | 2 | Raw Cadence | Little Endian. RPM = `value / 10`. |
| 6-7 | 2 | Heart Rate | Little Endian. BPM = `value / 10`. |
| 8-9 | 2 | Power | Little Endian. Watts. |
| 14-15| 2 | Distance | Little Endian. Unit 0.1 (Miles or KM). |
| 16 | 1 | Gear | Current gear setting. |

## 7. BLE Cycling Power relay — implementation notes

When `targetType == TARGET_CP`, the m5stick is a **sensor relay**: BLE central toward an upstream Cycling Power sensor (a real bike, or the nRF52840 `sim_nrf.cpp` build) and BLE peripheral toward downstream phones / head-units, republishing the same data through `BleManager`.

Both roles share one radio. Two patterns are critical and easy to break:

1. **Defer `NimBLEClient::connect()` to the main loop.** Calling it synchronously from inside `NimBLEScanCallbacks::onResult` (now indirectly via `UnifiedScanner::onResult → CyclePowerScanner::onAdvertisement`) races the host stack and the central connect times out with `BLE_HS_ETIMEOUT` (13) after the full retry window (~31 s). The scan callback only stashes a `const NimBLEAdvertisedDevice*` in `_pPendingConnect`; the next `CyclePowerScanner::update()` tick consumes it. Matches the canonical [NimBLE-Arduino client example](https://github.com/h2zero/NimBLE-Arduino/blob/master/examples/NimBLE_Client/NimBLE_Client.ino).

2. **Pause downstream advertising while scanning, resume it after subscribe.** The radio is single-role during the central handshake; once we are subscribed to the upstream CP measurement we restart advertising so phones can pair to the m5stick as a CP sensor. After the first successful subscribe, advertising stays up across reconnect cycles.

Other gotchas captured in `mcu/bike/cycle_power.cpp`: `setConnectTimeout()` is in **milliseconds** (not seconds — default 30000), and the `NimBLEClient` should be deleted and recreated between attempts to avoid `BLE_HS_EDONE` (14) from stale GAP state.

## 8. Bluetooth GATT transmission

The bridge uses a dynamic Device Name in the format `FreeFitness-XXXX`, where `XXXX` is the last 4 hex digits of the Flash Unique ID.

> [!TIP]
> Some BLE central devices (like nRF Toolbox or certain head units) require the presence of a **Battery Service (0x180F)** and **Control Point characteristics** (SC/CP Control Point) to correctly discover and maintain a connection to the sensor.

### Cycling Power (CP) — Service `0x1818`
> [!NOTE]
> CP provides **both power and cadence data**. In most cases, this service is sufficient for all target applications.
- **Measurement Characteristic (`2A63`)**:
  - Flags: `0x0030` (Wheel + Crank Data Present).
  - Power: `int16` (Little Endian, Watts).
  - Wheel Data: `uint32` (Cumulative Revs) + `uint16` (Last Event Time @ 1/2048 s).
  - Crank Data: `uint16` (Cumulative Revs) + `uint16` (Last Event Time @ 1/1024 s).

### Cycling Speed and Cadence (CSC) — Service `0x1816`
> [!NOTE]
> CSC is primarily used for speed and cadence data. It is often redundant when CP is active.
- **Measurement Characteristic (`2A5B`)**:
  - Flags: `0x03` (Wheel + Crank Data Present).
  - Wheel Data: `uint32` (Cumulative Revs) + `uint16` (Last Event Time @ 1/1024 s).
  - Crank Data: `uint16` (Cumulative Revs) + `uint16` (Last Event Time @ 1/1024 s).

## 9. ANT+ transmission

The bridge operates as a multi-sensor node using two independent ANT+ channels. The **Device Number** is dynamically derived from the Flash Unique ID.

### Channel 0: Bicycle Power
- **Device Type**: `0x0B` (11)
- **Transmission Type**: `5` (ANT+)
- **Period**: `8182` (4.00 Hz)

| Page | Name | Description |
| :--- | :--- | :--- |
| `0x10` | Power Only | Instantaneous and accumulated power + cadence. |
| `0x50` | Manufacturer Info | HW Revision, Manufacturer ID (`0x3862`), Model Number. |
| `0x51` | Product Info | SW Revision, 32-bit Serial Number (from Flash ID). |
| `0x52` | Battery Status | Coarse voltage and status (Good/Low). |

### Channel 1: Bicycle Speed
- **Device Type**: `0x7B` (123)
- **Transmission Type**: `5` (ANT+)
- **Period**: `8118` (4.03 Hz)

| Page | Name | Description |
| :--- | :--- | :--- |
| `0x00` | Speed Data | Cumulative revolutions and last event time (@ 1/1024 s). |
| `0x50` | Manufacturer Info | Synchronized with Channel 0. |
| `0x51` | Product Info | Synchronized with Channel 0. |
| `0x52` | Battery Status | Synchronized with Channel 0. |

## 10. Device identification & versioning

Configuration is managed centrally in `mcu/config.h`.

| Attribute | Source | Value |
| :--- | :--- | :--- |
| Device Serial | Flash Unique ID | 64-bit Hex String |
| ANT Device # | Flash Unique ID | 16-bit Truncated ID |
| BLE Name | `config.h` + ID | `FreeFitness-XXXX` |
