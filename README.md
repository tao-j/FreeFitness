# FreeFitness Data Adapter

A bridge to connect fitness equipment with sports watches and apps, converting raw data from various sources into required formats using scientific estimations to ensure maximum accuracy.

## Features
- **Multi-Source**: Connect to Keiser M3i bikes, simulation modes, and more.
- **Dual-Protocol**: Transmit data via **Bluetooth** (specifically **Bluetooth Low Energy** or **BLE**) and **ANT+**.
- **Cross-Platform Support**: Compatible with Garmin, Apple Watch, Zwift, and various fitness apps.
- **Hardware Agnostic**: Available as either a dedicated **Microcontroller Unit (MCU)** device or a Linux/SBC solution.

*Feel free to request other bikes or apps you'd like us to test for compatibility!*


---

## Solutions

The project is implemented in two distinct ways depending on available hardware and user needs:

### 1. MCU Solution
A portable, low-cost and low-power solution designed for ESP32 or nRF52 based hardware that's easier to carry around. See the [MCU README](mcu/README.md).

### 2. Linux Solution
A Python-based implementation for running on Linux. See the [Linux README](linux/README.md).

---

## Compatibility

| Feature / Platform | Apple Watch | Garmin | Zwift (iOS) | Zwift (Android) |
| :--- | :---: | :---: | :---: | :---: |
| **Power / Cadence (Bluetooth)** | ✅ | ✅ | ✅ | ✅ |
| **Speed (Bluetooth)** | (Opt) | (Opt) | (Opt) | (Opt) |
| **Power / Cadence (ANT+)** | ➖ | ✅ | ➖ | ➖ |
| **Speed (ANT+)** | ➖ | ✅ | ➖ | ➖ |

*Note: ✅ Supported | (Opt) Optional/Redundant | ➖ Not Needed/Supported*

### Platform Specific Notes

### watchOS 10+
**Bluetooth**: Just use the default wheel size 700x25mm.

### Garmin
**Fenix 7**: 
- Bluetooth: Search for PWR sensor, Bluetooth device should only advertise Cycle Power not Cycling Speed and Cadence. 
- ANT+: Search and connect the PWR and SPD sensors separately. 
In both cases, set the wheel size to be 2096mm (700x25mm).

### Zwift
Bluetooth: Connect the speed and power source to the same BLE device discovered.

### Samsung Galaxy Watch 8
First add Workout to the Health App on phone. Then go to Bike Workout on watch inside the Health App. However, there is no "Accessories" option to add a sensor. 

## Design & Development
For technical documentation on protocol mappings, data structures, and internal logic, see [DEVELOPMENT.md](DEVELOPMENT.md).

## License
Copyright (C) 2023-2026 Tao Jin.

FreeFitness is free software: you can redistribute it and/or modify it under the terms of version 3 of the GNU General Public License as published by the Free Software Foundation. See [LICENSE](LICENSE) for the full text.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

Third-party components retain their own licenses (NimBLE-Arduino: Apache-2.0; M5Unified/M5GFX: MIT; ant-arduino / antplus-arduino: MIT; Arduino-ESP32: Apache-2.0 / LGPL)