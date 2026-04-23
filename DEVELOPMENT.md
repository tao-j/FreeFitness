# Development Documentation

This document outlines the data structures and protocol mappings used in the Free Fitness Bridge.

## Key Concepts
- **CP (Cycling Power)**: Standard Bluetooth service for power and cadence. Usually sufficient for most applications.
- **CSC (Cycling Speed and Cadence)**: Dedicated Bluetooth service for speed and cadence.
- **MCU (Microcontroller Unit)**: Small, low-power processors (like ESP32 or nRF52) used for standalone hardware.
- **PWR (Bicycle Power)**: The ANT+ profile for broadcasting power and cadence data.
- **SPD (Bicycle Speed)**: The ANT+ profile for broadcasting speed and distance data. 

## 1. Keiser M3i BLE Advertisement
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

## 2. Internal Data Structure (`BikeData`)
All modules share a central `BikeData` structure which acts as the source of truth for the bridge.

| Field | Type | Description |
| :--- | :--- | :--- |
| `power` | uint16 | Instantaneous power in Watts. |
| `cadence` | uint8 | Instantaneous cadence in RPM. |
| `accPower` | uint16 | Cumulative power (sum of power values at each event). |
| `eventCount` | uint8 | Increments every time a new power reading is processed. |
| `wheelRevs` | uint32 | Cumulative wheel revolutions (simulated or estimated). |
| `wheelEventTime`| uint16 | Timestamp of last wheel revolution (unit: 1/1024 seconds). |
| `crankRevs` | uint16 | Cumulative crank revolutions (simulated or estimated). |
| `crankEventTime`| uint16 | Timestamp of last crank revolution (unit: 1/1024 seconds). |

## 3. Bluetooth GATT Transmission
The bridge uses a dynamic Device Name in the format `FreeFitness-XXXX`, where `XXXX` is the last 4 hex digits of the Flash Unique ID.

> [!TIP]
> Some BLE central devices (like nRF Toolbox or certain head units) require the presence of a **Battery Service (0x180F)** and **Control Point characteristics** (SC/CP Control Point) to correctly discover and maintain a connection to the sensor.

### Cycling Power (CP) - Service `0x1818`
> [!NOTE]
> CP provides **both power and cadence data**. In most cases, this service is sufficient for all target applications.
- **Measurement Characteristic (`2A63`)**:
  - Flags: `0x0000` (Instantaneous Power only).
  - Power: `uint16` (Little Endian, Watts).

### Cycling Speed and Cadence (CSC) - Service `0x1816`
> [!NOTE]
> CSC is primarily used for speed and cadence data. It is often redundant when CP is active.
- **Measurement Characteristic (`2A5B`)**:
  - Flags: `0x03` (Wheel + Crank Data Present).
  - Wheel Data: `uint32` (Cumulative Revs) + `uint16` (Last Event Time).
  - Crank Data: `uint16` (Cumulative Revs) + `uint16` (Last Event Time).

## 4. ANT+ Transmission
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
| `0x00` | Speed Data | Cumulative revolutions and last event time. |
| `0x50` | Manufacturer Info | Synchronized with Channel 0. |
| `0x51` | Product Info | Synchronized with Channel 0. |
| `0x52` | Battery Status | Synchronized with Channel 0. |

## 5. Device Identification & Versioning
Configuration is managed centrally in `mcu/config.h`.

| Attribute | Source | Value |
| :--- | :--- | :--- |
| Device Serial | Flash Unique ID | 64-bit Hex String |
| ANT Device # | Flash Unique ID | 16-bit Truncated ID |
| BLE Name | `config.h` + ID | `-XXXX` |
