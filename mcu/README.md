# MCU Implementation & Hardware Selection

This directory contains the firmware implementation for the FreeFitness Data Adapter on MCU platforms.

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

The firmware provides an interactive UI for real-time monitoring and configuration.

### Dashboard Mode (Home)
This is the default screen showing Power (W), Cadence (RPM), and connection status.

-   **Toggle Real/Sim Mode**: Click **BtnA** (the large main button) to switch between simulation and real sensor data.
-   **Select Bike ID**:
    -   Click **BtnB** (side button) to increase the target Keiser Bike ID.
    -   Click **BtnPWR** (bottom/power button) to decrease the target Bike ID.
    -   Hold **BtnB** to reset the Bike ID to `0` (this enables "Search Any" mode, any Bike ID's data will be accepted, if there are multiple bikes indoor, data might be mixed).
-   **Enter Settings**: Hold **BtnA** to enter the Protocol Settings menu.

### Settings Mode (Profile Configuration)
This menu allows you to enable or disable specific Bluetooth and ANT+ profiles.

-   **Navigation**: Use **BtnB** (Up) and **BtnPWR** (Down) to move the cursor.
-   **Toggle**: Click **BtnA** to turn a protocol ON or OFF.
-   **Save & Exit**: Hold **BtnA**. If any changes were made, the device will **save to memory and reboot** to apply the new protocol stack configuration.

### Persistence
All settings, including simulation mode, target Bike ID, and enabled profiles, are stored in **Non-Volatile Storage (NVS)**. They are automatically preserved across power cycles and reboots.