import asyncio
import math

from . import *
from bike import Bike
from clock import now_tick


def uint8(val):
    return int(val) & 0xFF


def uint16(val):
    return int(val) & 0xFFFF


def uint32(val):
    return int(val) & 0xFFFFFFFF


WHEEL_CIRCUMFERENCE = 2.096  # meters (700c * 25)
SILENCE_TICKS = 2 * 1024  # 2 seconds — stop transmitting after this long without source data


class ProtocolEncoder:
    """Consumes BikeState and produces the values BLE/ANT payloads need.

    Dispatch rule, per channel (crank, wheel), is events-first:
      1. If the source populates discrete events (has_*_event), pass them
         through unchanged — a real encoder's interrupt timestamps are
         always more accurate than anything we could interpolate.
      2. Otherwise, fall back to CountGenerator interpolation driven by
         the continuous rate (cadence for crank, speed for wheel).
      3. If even the rate is absent, derive it where possible: the only
         such site is power -> speed via power_to_speed(). This is the
         sole physics hop in the pipeline.

    Silence detection is derived, not signaled: the encoder compares
    `state.last_update_tick` against now; if it hasn't advanced for
    SILENCE_TICKS, self.no_data flips True and the tx layers skip
    transmitting. See memory: encoder polls at 20 Hz so it can observe
    silence (an event-blocked encoder never would).

    Latched fields (self.power, .cadence, .speed, .crank_revs/.crank_event_tick,
    .wheel_revs/.wheel_event_tick) hold the last known value so the tx loop
    always has something to send, mirroring how a real sensor keeps
    re-advertising its last event until a new one arrives.
    """

    def __init__(self, data_src: Bike) -> None:
        self.data_src = data_src
        self.last_feed_tick = now_tick()

        self.power = 0.0
        self.cadence = 0.0
        self.speed = 0.0

        # Mirror BikeState's event fields: cumulative revs + tick timestamp
        # (1/1024 s each) of the most recent integer-crossing event.
        self.crank_revs = 0
        self.crank_event_tick = 0
        self.wheel_revs = 0
        self.wheel_event_tick = 0

        # ANT+ power page accumulators
        self.power_event_count = 0
        self.cum_power = 0

        self.no_data = True

    async def loop(self):
        wheel_count = CountGenerator()
        crank_count = CountGenerator()

        while True:
            await asyncio.sleep(0.05)

            now = now_tick()
            state = self.data_src.state

            # Silence detection: if the source hasn't stamped fresh data
            # within SILENCE_TICKS, mark no_data and skip derivation.
            if state.last_update_tick == 0 or (now - state.last_update_tick) > SILENCE_TICKS:
                self.no_data = True
                continue
            self.no_data = False

            dt = (now - self.last_feed_tick) / 1024
            self.last_feed_tick = now

            # Crank: events > cadence rate.
            if state.has_crank_event:
                self.crank_revs = state.crank_revs
                self.crank_event_tick = state.crank_event_tick
            elif not math.isnan(state.cadence):
                self.cadence = state.cadence
                crank_count.inc_count(state.cadence * dt / 60, now)
                self.crank_revs, self.crank_event_tick = crank_count.get_event()

            # Wheel: events > speed > power-derived speed.
            if state.has_wheel_event:
                self.wheel_revs = state.wheel_revs
                self.wheel_event_tick = state.wheel_event_tick
                if not math.isnan(state.speed):
                    self.speed = state.speed
            else:
                speed = state.speed
                if math.isnan(speed) and not math.isnan(state.power):
                    speed = power_to_speed(state.power)
                if not math.isnan(speed):
                    inc = (speed + self.speed) / 2 * dt / WHEEL_CIRCUMFERENCE
                    self.speed = speed
                    wheel_count.inc_count(inc, now)
                    self.wheel_revs, self.wheel_event_tick = wheel_count.get_event()

            # Power: pass-through + ANT+ accumulators.
            if not math.isnan(state.power):
                self.power = state.power
                self.power_event_count += 1
                self.cum_power += int(state.power)


class BLEEncoder(ProtocolEncoder):
    def get_wheel_revs(self):
        return uint32(self.wheel_revs)

    def get_crank_revs(self):
        return uint16(self.crank_revs)

    def get_wheel_event_tick(self):
        return uint16(self.wheel_event_tick)

    def get_crank_event_tick(self):
        return uint16(self.crank_event_tick)

    def get_power(self):
        return uint16(self.power)


class ANTEncoder(ProtocolEncoder):
    def get_power_event_count(self):
        return uint8(self.power_event_count)

    def get_cum_power(self):
        return uint16(self.cum_power)

    def get_cadence(self):
        return uint8(self.cadence)

    def get_power(self):
        return uint16(self.power)

    def get_wheel_revs(self):
        return uint16(self.wheel_revs)

    def get_wheel_event_tick(self):
        return uint16(self.wheel_event_tick)
