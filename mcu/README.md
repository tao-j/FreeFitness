# MCU Implementation & Hardware Selection

This directory contains the firmware implementation for the FreeFitness Data Adapter on MCU platforms. For architecture, protocol mappings, and design notes see [DEVELOPMENT.md](../DEVELOPMENT.md).

## Hardware Implementation Options

### BLE-Only Adoption
Currently, **Bluetooth Low Energy (BLE)** is sufficient for most modern devices, including Apple Watch, Garmin, Wahoo, and Zwift. A standalone MCU can handle this with ease, avoiding the added cost and hardware complexity of ANT+.

### BLE + ANT+ Adoption

#### Option 1: nRF52840
The nRF52840 offers integrated ANT+ and BLE capabilities, making it a strong candidate for this project. Key considerations:
- Requires $1600 one-time license fee and regulatory approval for ANT+ stack
- Development-friendly options available through Adafruit:
  - [nRF52840 board](https://www.adafruit.com/product/4062)
  - [OLED display](https://www.adafruit.com/product/4650)
  - [Battery management](https://www.adafruit.com/product/3898)

Potential licensing workaround:
- Distribute base firmware with BLE only
- Provide optional ANT+ stack firmware update for evaluation purposes
- Note: This approach may have legal implications

#### Option 2: nRF52832 (ANT D52 Module)
A cost-effective solution with pre-certified components:
- D52 BOM cost: ~$10
- No license fees
- Pre-approved regulatory certification
- Requires Garmin distribution agreement (free)
- Reference: [D52 Module](https://www.arrow.com/en/products/d52md2m8ia-tray/dynastream-innovations)

Limitations:
- No pre-made breakout boards available
- Manual assembly required
- SoftDevice S332 required for combined BLE/ANT+ functionality, and using Nordic nRF52 SDK is very painful.

#### Option 3: nRF24AP2 + ESP32-S3
A modular approach combining separate chips:
- nRF24AP2 (~$5): ANT+ capabilities
  - No license fees
  - NRND status may affect availability
  - Serial interface simplifies integration
- ESP32-S3 (~$5): Main processor
  - Provides BLE and WiFi connectivity
  - Handles core processing tasks

Considerations:
- Requires careful antenna design
- Total solution cost competitive with other options
- More complex PCB design but simpler software stack

## Sample Implementation: ESP32 with optional separate ANT radio

This implementation uses an ESP32 or ESP32-S3 as the primary MCU, with an optional separate ANT radio module like the nRF24AP2 (Option 3). All Bluetooth (BLE) and ANT+ profiles are fully implemented, tested, and can be toggled on/off independently via the on-device UI.

The **M5StickC** series is highly recommended as it provides a well-integrated development board with a screen, buttons, battery, and proper housing—making it ready for real-world use without assembly hassle.

### Connection to top 8-pin header if using ANT+ module

The following pin mappings are used for connecting an external ANT module:

1.  **ANT TX** -> M5Stick **G26**
2.  **ANT RX** -> M5Stick **G25**
3.  **ANT Reset** -> M5Stick **G0** (Optional)
4.  **VCC (3.3V)** and **GND** connections as usual.
5.  **nRF24AP2 Configuration**: Connect **SLEEP** to GND and **SUSPEND** to 3.3V.

## Operations & UI

Two pages — **Dashboard** (default) and **Settings** — plus a transient **Sensor Picker** reached by short-press from Dashboard.

### Button contract

| Button | Short press | Long press |
|---|---|---|
| **BtnA** (front) | Primary action: open picker / select entry / toggle row | Open Settings; in Settings, save & exit |
| **BtnB** (side) | Scroll up / previous | — |
| **BtnPWR** (bottom) | Scroll down / next | — |

### Dashboard
The default screen shows Power (W), Cadence (RPM), and Speed (mph), with a header chip showing the source mode (`SIM` / `BLE`) and the current target (`LOCAL`, `M3 #42`, the saved CP name, or `BLE NO PICK`), plus battery %.

- **Short BtnA**: open Sensor Picker.
- **Hold BtnA**: enter Settings.

### Sensor Picker
Lists every BLE bike currently being heard — Keiser M3 advertisements *and* Cycling Power services land in one merged list, each tagged `M3` or `CP`. M3 entries show the bike ID; both show name and BLE address.

- **BtnB / BtnPWR**: browse list.
- **Short BtnA**: select. Saves target + auto-flips Source to BLE if previously SIM. Returns to Dashboard, which immediately starts showing data from the new sensor.
- **Hold BtnA**: cancel and return to Dashboard.

The scanner runs always-on while in BLE mode (so the picker opens with a populated list). In SIM mode the picker temporarily starts the scanner — picking a sensor switches mode to BLE.

### Settings
Long-press BtnA from Dashboard.

| Row | Type | Notes |
|---|---|---|
| Brightness | 1..5 | Live application. Maps to `M5.Display.setBrightness({32, 80, 128, 192, 255})`. Level 1 stays visible (no dark blank). |
| Source | SIM / BLE | Live mode toggle. |
| BLE CP / BLE CSC / BLE FTMS | bool | Reboot on exit to re-init GATT services. |
| ANT+ PWR / ANT+ CSC / ANT+ FEC | bool | Reboot on exit to re-init ANT channels. |

- **BtnB / BtnPWR**: scroll cursor.
- **Short BtnA**: toggle / cycle the row under the cursor.
- **Hold BtnA**: save & exit. Reboots only if a row that requires reboot was changed; Brightness and Source changes apply live with no reboot.

### Persistence
All settings — source mode, target type, saved M3 bike ID + address, saved CP address/name, enabled profiles, brightness — are stored in **Non-Volatile Storage (NVS)** and automatically preserved across power cycles. Old `source` keys from previous firmware versions migrate automatically.
