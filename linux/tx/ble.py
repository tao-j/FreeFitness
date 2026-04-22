import asyncio
import struct
import time

from bluez_peripheral.gatt.service import Service, ServiceCollection
from bluez_peripheral.gatt.characteristic import (
    characteristic,
    CharacteristicFlags as CharFlags,
)
from bluez_peripheral.util import *
from bluez_peripheral.advert import Advertisement, AdvertisingIncludes
from bluez_peripheral.agent import NoIoAgent

# cycling speed and cadence
CSC_UUID = "1816"
CSC_MEASUREMENT_UUID = "2A5B"
CSC_FEATURE_UUID = "2A5C"
SC_CONTROL_POINT_UUID = "2A55"

# cycling power
CP_UUID = "1818"
CP_MEASUREMENT_UUID = "2A63"
CP_VECTOR_UUID = "2A64"
CP_FEATURE_UUID = "2A65"
CP_CONTROL_POINT_UUID = "2A66"

SENSOR_LOCATION_UUID = "2A5D"
SENSOR_LOCATION_CHAR_UUID = 0x2A5D
SENSOR_LOCATION_OTHER = 0
SENSOR_LOCATION_TOP_OF_SHOE = 1
SENSOR_LOCATION_IN_SHOE = 2
SENSOR_LOCATION_HIP = 3
SENSOR_LOCATION_FRONT_WHEEL = 4
SENSOR_LOCATION_LEFT_CRANK = 5
SENSOR_LOCATION_RIGHT_CRANK = 6
SENSOR_LOCATION_LEFT_PEDAL = 7
SENSOR_LOCATION_RIGHT_PEDAL = 8
SENSOR_LOCATION_FRONT_HUB = 9
SENSOR_LOCATION_REAR_DROPOUT = 10
SENSOR_LOCATION_CHAINSTAY = 11
SENSOR_LOCATION_REAR_WHEEL = 12
SENSOR_LOCATION_REAR_HUB = 13
SENSOR_LOCATION_CHEST = 14
SENSOR_LOCATION_SPIDER = 15
SENSOR_LOCATION_CHAIN_RING = 16

# device information
DI_UUID = "180A"
DI_SYSTEM_ID_UUID = "2A23"
DI_MODEL_NUMBER_UUID = "2A24"
DI_SERIAL_NUMBER_UUID = "2A25"
DI_FIRMWARE_REVISION_UUID = "2A26"
DI_HARDWARE_REVISION_UUID = "2A27"
DI_SOFTWARE_REVISION_UUID = "2A28"
DI_MANUFACTURER_NAME_UUID = "2A29"

# battery service
BAT_UUID = "180F"
BAT_LEVEL_UUID = "2A19"

CSC_F_BIT_WHEEL_REVOLUTION_DATA_PRESENT = 0b0000_0001
CSC_F_BIT_CRANK_REVOLUTION_DATA_PRESENT = 0b0000_0010

# feature flags
CP_F_BIT_WHEEL_REVOLUTION_DATA_PRESENT = 0b0001_0000
CP_F_BIT_CRANK_REVOLUTION_DATA_PRESENT = 0b0010_0000
# measurement flags
CP_M_BIT_WHEEL_REVOLUTION_DATA_PRESENT = 0b0000_0100
CP_M_BIT_CRANK_REVOLUTION_DATA_PRESENT = 0b0000_1000


class BatteryService(Service):
    def __init__(self):
        super().__init__(BAT_UUID, primary=True)

    @characteristic(BAT_LEVEL_UUID, CharFlags.READ | CharFlags.NOTIFY)
    def battery_level(self, options):
        return struct.pack("<B", *[100])


class DeviceInformationService(Service):
    def __init__(self):
        super().__init__(DI_UUID)

    @characteristic(DI_SYSTEM_ID_UUID, CharFlags.READ)
    def system_id(self, options):
        return struct.pack("<Q", *[0x0000022001100000])

    @characteristic(DI_MODEL_NUMBER_UUID, CharFlags.READ)
    def model_number(self, options):
        return bytearray(b"Keiser M to GATT")

    @characteristic(DI_SERIAL_NUMBER_UUID, CharFlags.READ)
    def serial_number(self, options):
        return bytearray(b"12345678")

    @characteristic(DI_FIRMWARE_REVISION_UUID, CharFlags.READ)
    def firmware_revision(self, options):
        return bytearray(b"0.0.1")

    @characteristic(DI_HARDWARE_REVISION_UUID, CharFlags.READ)
    def hardware_revision(self, options):
        return bytearray(b"0.1.1")

    @characteristic(DI_SOFTWARE_REVISION_UUID, CharFlags.READ)
    def software_revision(self, options):
        return bytearray(b"1.0beta")

    @characteristic(DI_MANUFACTURER_NAME_UUID, CharFlags.READ)
    def manufacturer_name(self, options):
        return bytearray(b"t-j")


class CPService(Service):
    def __init__(self):
        self.measure_flags = (
            0x0
            # | CP_M_BIT_CRANK_REVOLUTION_DATA_PRESENT
            # | CP_M_BIT_WHEEL_REVOLUTION_DATA_PRESENT
        )
        self.feature_flags = (
            0x0
            # | CP_F_BIT_CRANK_REVOLUTION_DATA_PRESENT
            # | CP_F_BIT_WHEEL_REVOLUTION_DATA_PRESENT
        )
        super().__init__(CP_UUID, primary=True)

    @characteristic(CP_MEASUREMENT_UUID, CharFlags.NOTIFY | CharFlags.READ)
    def cp_measurement(self, options):
        pass

    def notify_new_rate(self, power, w_event_ms, c_event_ms, crank_rev, wheel_rev):
        rate = struct.pack(
            "<Hh",
            *[
                self.measure_flags,
                power & 0x7FFF,
                # wheel_rev & 0xFFFFFFFF,
                # w_event_ms & 0xFFFF,
                # crank_rev & 0xFFFF,
                # c_event_ms & 0xFFFF,
            ],
        )
        self.cp_measurement.changed(rate)

    @characteristic(CP_FEATURE_UUID, CharFlags.READ)
    def cp_feature(self, options):
        return struct.pack("<I", *[self.feature_flags])

    @characteristic(SENSOR_LOCATION_UUID, CharFlags.READ)
    def sensor_location(self, options):
        return struct.pack("<B", *[SENSOR_LOCATION_REAR_WHEEL])

    @characteristic(CP_CONTROL_POINT_UUID, CharFlags.WRITE | CharFlags.INDICATE)
    def cp_control_point(self, options):
        pass


class CSCService(Service):
    def __init__(self):
        super().__init__(CSC_UUID, True)
        self.feature = (
            0
            | CSC_F_BIT_WHEEL_REVOLUTION_DATA_PRESENT
            | CSC_F_BIT_CRANK_REVOLUTION_DATA_PRESENT
        )

    @characteristic(CSC_MEASUREMENT_UUID, CharFlags.NOTIFY | CharFlags.READ)
    def csc_measurement(self, options):
        pass

    # @csc_measurement.descriptor("2902")
    # def csc_measurement_descriptor(self, options):
    #     return struct.pack("<H", *[0x0001])

    def notify_all(self, wheel_rev, crank_rev, w_event_ms, c_event_ms):
        rate = struct.pack(
            "<BIHHH",
            CSC_F_BIT_CRANK_REVOLUTION_DATA_PRESENT
            | CSC_F_BIT_WHEEL_REVOLUTION_DATA_PRESENT,
            wheel_rev & 0xFFFFFFFF,
            w_event_ms & 0xFFFF,
            crank_rev & 0xFFFF,
            c_event_ms & 0xFFFF,
        )
        self.csc_measurement.changed(rate)

    def notify_crank(self, wheel_rev, crank_rev, w_event_ms, c_event_ms):
        rate = struct.pack(
            "<BHH",
            CSC_F_BIT_CRANK_REVOLUTION_DATA_PRESENT,
            crank_rev & 0xFFFF,
            c_event_ms & 0xFFFF,
        )
        self.csc_measurement.changed(rate)

    def notify_wheel(self, wheel_rev, crank_rev, w_event_ms, c_event_ms):
        rate = struct.pack(
            "<BIH",
            CSC_F_BIT_WHEEL_REVOLUTION_DATA_PRESENT,
            wheel_rev & 0xFFFFFFFF,
            w_event_ms & 0xFFFF,
        )
        self.csc_measurement.changed(rate)

    @characteristic(CSC_FEATURE_UUID, CharFlags.READ)
    def csc_feature(self, options):
        return struct.pack("<H", *[self.feature])

    @characteristic(SENSOR_LOCATION_UUID, CharFlags.READ)
    def sensor_location(self, options):
        return struct.pack("<B", *[SENSOR_LOCATION_OTHER])

    @characteristic(SC_CONTROL_POINT_UUID, CharFlags.WRITE | CharFlags.INDICATE)
    def sc_control_point(self, options):
        pass


class BLETx:
    def __init__(self) -> None:
        pass

    async def setup(self):
        bus = await get_message_bus()

        bat_service = BatteryService()
        di_service = DeviceInformationService()
        cp_service = CPService()
        csc_service = CSCService()
        svcs = ServiceCollection([bat_service, di_service, cp_service, csc_service])
        await svcs.register(bus)

        self.bat_service = bat_service
        self.di_service = di_service
        self.cp_service = cp_service
        self.csc_service = csc_service

        # An agent is required to handle pairing
        # This script may need superuser for this to work.
        agent = NoIoAgent()
        await agent.register(bus)

        adapter = await Adapter.get_first(bus)
        print(await adapter.get_address())

        # Start an advert that will last for 60 seconds.
        advert = Advertisement(
            localName="Keiser M to GATT",
            serviceUUIDs=[BAT_UUID, DI_UUID, CP_UUID, CSC_UUID],
            appearance=0x0480,
            timeout=0,
            includes=AdvertisingIncludes.TX_POWER,
        )
        await advert.register(bus, adapter)

    async def loop(self, bike_data):
        while True:
            await asyncio.sleep(0.25)
            if bike_data.no_data:
                print("BLE: No data")
                continue

            wr = bike_data.get_wr()
            cr = bike_data.get_cr()
            wev = bike_data.get_wev()
            cev = bike_data.get_cev()
            power = bike_data.get_power()
            print(
                "BLE TX: ",
                f"{power:3d} W {wr:6d} wREV {cr:6d} cREV",
                f"w{wev:5d} ms c{cev:5d} ms {bike_data.speed * 3.6 / 1.67:2.1f} mph",
                time.time(),
                end="\n",
            )
            # if crank_rev_cls.notify and wheel_rev_cls.notify:
            if 1:
                self.csc_service.notify_all(
                    wheel_rev=wr, crank_rev=cr, w_event_ms=wev, c_event_ms=cev
                )
                # crank_rev_cls.notify = False
                # wheel_rev_cls.notify = False
                # else:
                #     if crank_rev_cls.notify:
                #         csc_service.notify_crank(
                #             wheel_rev=wr, crank_rev=cr, w_event_ms=wev, c_event_ms=cev
                #         )
                #     if wheel_rev_cls.notify:
                #         csc_service.notify_wheel(
                #             wheel_rev=wr, crank_rev=cr, w_event_ms=wev, c_event_ms=cev
                #         )
            self.cp_service.notify_new_rate(
                power=power,
                wheel_rev=wr,
                crank_rev=cr,
                w_event_ms=wev,
                c_event_ms=cev,
            )
        # Handle dbus requests.
