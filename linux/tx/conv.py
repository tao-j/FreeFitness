import time
from . import *
from bike import Bike
import asyncio


def uint8(val):
    return int(val) & 0xFF


def uint16(val):
    return int(val) & 0xFFFF


def uint32(val):
    return int(val) & 0xFFFFFFFF


class Conv:
    def __init__(self, data_src: Bike) -> None:
        self.data_src = data_src
        self.last_feed_time = time.time()

        self.flag_crank_encoder = True if hasattr(data_src, "rev_inc") else False
        self.flag_power_to_speed = True if hasattr(data_src, "power") else False

        # common concepts
        self.power = 0
        self.cadence = 0  # crank rpm
        self.speed = 0  # wheel m/s

        # BLE concepts
        self.cr = 0
        self.cev = 0
        self.wr = 0
        self.wev = 0

        # ANT+ concepts
        self.power_event_counts = 0
        self.cum_power = 0

        self.no_data = True

    async def loop(self):
        wheel_count = CountGenerator()
        crank_count = CountGenerator()

        while True:
            # await self.data_src.new_data.wait()
            await asyncio.sleep(0.05)

            if self.data_src.no_data:
                self.no_data = True
                continue
            else:
                self.no_data = False

            now = time.time()
            dt = now - self.last_feed_time
            self.last_feed_time = now

            if self.flag_crank_encoder:
                inc = self.data_src.rev_inc

                crank_count.add(inc, now)
                self.cadence = crank_count.rpm
            else:
                self.cadence = self.data_src.cadence
                crank_count.add(self.cadence * dt / 60, now)

            if self.flag_power_to_speed:
                speed = power_to_speed(self.power)
                # set wheel to 700c*25 or ~2096mm
                WHEEL_CIRCUMFERENCE = 2.096
                inc = (speed + self.speed) / 2 * dt / WHEEL_CIRCUMFERENCE
                self.speed = speed
            else:
                inc = self.data_src.rev_inc
            self.power = self.data_src.power
            wheel_count.add(inc, now)

            self.cr, self.cev = crank_count.get()
            self.wr, self.wev = wheel_count.get()

            # power events
            self.power_event_counts += 1
            self.cum_power += self.power


class BLEConv(Conv):
    def get_wr(self):
        return uint32(self.wr)

    def get_cr(self):
        return uint16(self.cr)

    def get_wev(self):
        return uint16(self.wev)

    def get_cev(self):
        return uint16(self.cev)

    def get_power(self):
        return uint16(self.power)


class ANTConv(Conv):
    def get_event_count(self):
        return uint8(self.power_event_counts)

    def get_cum_power(self):
        return uint16(self.cum_power)

    def get_cadence(self):
        return uint8(self.cadence)

    def get_power(self):
        return uint16(self.power)

    def get_cum_rev_count(self):
        return uint16(self.wr)

    def get_event_time_ms(self):
        return uint16(self.wev)
