# Linux Implementation (Python)

This directory contains the Python-based implementation of the FreeFitness Data Adapter, designed to run on Linux systems (PC, Raspberry Pi, etc.) using a USB ANT+ dongle and the system's Bluetooth stack.

## Current Implementation and Similar Solutions

[ptx2/gymnasticon](https://github.com/ptx2/gymnasticon) was an earlier project implementing similar functionality, which appears to have been adapted by [k2pi](https://k2pi.company.site/) for their $80-100 commercial product. However, their solution has limited features and is relatively bulky with higher power consumption.

Due to the original JavaScript project's large codebase and unmaintained BLE stack, we've developed this Python-based implementation for better maintainability and extensibility. Our solution adds Keiser bike ID selection and ANT+ speed data transmission.

The Linux implementation successfully interfaces with both Garmin devices (via ANT+) and Apple Watch (via BLE).

## System Design

The system consists of two modules working asynchronously:

1. **`bike`**: Receives sensor data or generates simulated data.
   - Emits a signal when new readings are available.
   - Handles raw data capture from sources like Keiser M3i (BLE advertisement).
2. **`tx`**: Sends the data in BLE or ANT protocol defined format.
   - Handles protocol-specific formatting and timing (typically 4Hz).
   - If no data is received within 2 seconds, transmission should stop.

The converter `tx.conv` transforms raw `bike` data into the floating-point values used by `tx`. It performs algorithmic estimations such as:
- **CounterGenerator**: For generating revolution events.
- **Speed Estimation**: Estimating speed from power.
- **Wheel Revolution Estimation**: Estimating wheel revolutions from speed.

## Supported Bikes

### Keiser M3i
Keiser M series BLE broadcast is public ([spec](https://dev.keiser.com/mseries/direct/)). These bikes transmit readings in GAP Manufacturer Specific Data messages. The BikeID of interest can be configured.

### Simulation
Includes a mock source for testing without a physical bike.

## Data Transmission Details

### ANT+
Requires an ANT+ transceiver:
- `ANT-USB`
- `ANT-USBm` (based on nRF24L01P?)
- Other transceivers supporting ANT+ Tx (may need serial driver changes).

*Note: CYCPLUS branded dongles are reported not to work.*

### Bluetooth (BLE GATT)
Linux uses `dbus` to manage `bluez`. The [`bluez-peripheral`](https://github.com/spacecheese/bluez_peripheral) library is used to abstract the peripheral role.

**Known Limitations:**
- The cross-platform solution `bless` has trouble advertising several GATT profiles on Linux. If two profiles are defined, only one may be readable. `bluez-peripheral` is used to mitigate this.
- Bluetooth SIG specs can be difficult to access; community-maintained XML definitions (like [gatt-xml](https://github.com/oesmith/gatt-xml)) are used for bitmask definitions.
