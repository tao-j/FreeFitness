import asyncio
import struct
from bleak import BleakScanner

from . import *


class KeiserBike(Bike):
    def __init__(self, bike_id=0) -> None:
        super().__init__()

        self.bike_id = bike_id & 0xFF
        self.version_major = 0
        self.version_minor = 0
        self.data_type = 0
        self.cadence = 0
        self.heart_rate = 0
        self.power = 0
        self.calories = 0
        self.minutes = 0
        self.seconds = 0
        self.distance = 0
        self.gear = 0

        self.scanner = BleakScanner(self.callback)

    def callback(self, device, advertisement_data):
        if device.name == "M3":
            # print(advertisement_data)
            if hasattr(advertisement_data, "manufacturer_data"):
                msd = advertisement_data.manufacturer_data
                if self.parse_keiser_msd(msd):
                    self.new_data.set()

    def parse_keiser_msd(self, msd: dict):
        for k, v in msd.items():
            if k == 0x0102 and 17 == len(v) and v[3] == self.bike_id and v[2] == 0:
                (
                    self.version_major,
                    self.version_minor,
                    self.data_type,
                    self.bike_id,
                    self.cadence,
                    self.heart_rate,
                    self.power,
                    self.calories,
                    self.minutes,
                    self.seconds,
                    self.distance,
                    self.gear,
                ) = struct.unpack("<BBB" + "BHHH" + "HBBHB", v)

                self.cadence /= 10
                self.heart_rate /= 10
                if self.distance >> 15 == 0:
                    # mile
                    self.distance * 1609.344
                # km
                self.distance = (self.distance & 0x7FFF) / 10
                self.resistence = self.gear / 24 * 100
                # print(f"Version Major: {version_major}")
                # print(f"Version Minor: {version_minor}")
                # print(f"Data Type: {data_type}")
                # print(f"Equipment ID: {bike_id}")
                # print(f"Cadence: {cadence}")
                # print(f"Heart Rate: {heart_rate}")
                # print(f"Power: {power}")
                # print(f"Caloric Burn: {calories}")
                # print(f"Duration Minutes: {minutes}")
                # print(f"Duration Seconds: {seconds}")
                # print(f"Distance: {distance}")
                # print(f"Gear: {gear}")

                return True
        return False

    async def loop(self):
        while True:
            await self.scanner.start()
            try:
                async with asyncio.timeout(2):
                    await self.new_data.wait()
                    self.no_data = False
            except asyncio.TimeoutError:
                print("Scan timeout, restarting\r", end="")
                self.no_data = True
            await self.scanner.stop()
            self.new_data.clear()
