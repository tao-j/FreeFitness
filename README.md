# Keiser BLE GAP to ANT+

This project listens to Keiser M series BLE broadcast ([GAP spec](https://dev.keiser.com/mseries/direct/)) and retransmit them in ANT+ compatible profiles. ANT+ specs can be obtained but not distributed at [here](https://www.thisisant.com/my-ant/join-adopter/).

## Hardware requirement
+ A BLE compatible receiver
+ An ANT+ transceiver like
    + `ANT-USB`
    + `ANT-USBm` (based on NRF 24L01P)
    + Or other ones supports Ant+ Tx

It is believed that CYCPLUS branded ones do not work.

## Dependencies
```
python-ant
bleak
```

## System Design

There are two modules works asynchronously:
+ Keiser Data Listener: `kl`, scans BLE advertisements and filter the data to find `bike_id` of interest.
+ ANT+ Data Transmitter: `ant_tx`, transmits data pages ~~~in 4Hz.~~~ whenever there is a fresh data fed from `kl`.

And there is a converter `kl2ant` that transform from `kl` into `ant_tx` that conforms the ANT+ specs.

For BLE(GATT) Tx, there is a separate `ble_tx` and `kl2ble` for use with shared `kl`.

# BLE GATT part

This project attempts to implement a translator from Keiser BLE GAP message to a BLE GATT peripheral.

Currently, progress has been made from last time:
+ CSC/CP works for watchOS 10. But sometimes it is unable to setup wheel dimension.
+ CSC connects to Garmin when wheel revolution is enabled, and the wheel diameter can be set. If crank revolution data bit is set then it is unable to set wheel diameter regardless of the presence of wheel data. The trick is to add an empty SC control point. CP works if only send power data.
+ CSC/CP can be picked up by Zwift, but displays no data.
+ CSC can be picked up by nRF Toolbox with Battery Service implemented, wheel speed is fine, but cadence jumps between 0 and a sensible value.

Related info
+ Linux uses `dbus` to manage `bluez`. [`bluez-peripheral`](https://github.com/spacecheese/bluez_peripheral) is a wrapper abstracts peripheral role devices. Quick concept overview of dbus [by bootlin](https://bootlin.com/pub/conferences/2016/meetup/dbus/josserand-dbus-meetup.pdf)
+ Bluetooth specs are useless and the Bluetooth SIG hides the actual bits definition xml files. Fortunately there is some accessible [backup on Github](
https://github.com/oesmith/gatt-xml/blob/master/org.bluetooth.characteristic.cycling_power_measurement.xml). 
+ [1](https://ihaque.org/posts/2021/01/04/pelomon-part-iv-software/) [2](https://github.com/olympum/ble-cycling-power) [3](https://teaandtechtime.com/arduino-ble-cycling-power-service). [4](https://github.com/PunchThrough/espresso-ble/blob/master/ble.py) [5](https://github.com/Jumperr-labs/python-gatt-server/blob/master/gatt_server.py)

Other related info:
+ There is some issue in `bless` that prevents advertising several GATT profiles. If two profiles are defined, only one profile can be read by `nRF connect` app on a phone during testing.
+ To do it another way, actually in Circuit Python can be used, which does not support generic Linux host at the moment. `ESP32-S3` or old `ESP32` or others can be used.
