import asyncio
import math

from bike import Bike
from bridge import RateEventBridge, get_tick_now, power_to_speed


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

    Each rotational channel runs through a RateEventBridge: whichever half
    the source provides (rate or events), the bridge fills in the other,
    so downstream payload fields are always populated regardless of whether
    the source is rate-driven (Keiser) or event-driven (hall sensor, sim).

    Dispatch per channel:
      - Crank: has_crank_event → bridge.feed_event;
               else if !isnan(cadence) → bridge.feed_rate.
      - Wheel: has_wheel_event → bridge.feed_event;
               else if !isnan(speed) → bridge.feed_rate(speed);
               else if !isnan(power) → bridge.feed_rate(power_to_speed(power)).
        Power → speed is the one physics hop in the pipeline.

    Silence detection: if `state.last_update_tick` hasn't advanced within
    SILENCE_TICKS, self.no_data flips True and the tx layers skip
    transmitting. The encoder polls at 20 Hz so it can observe silence
    (an event-blocked encoder never would).

    Latched fields hold the last known value so the tx loop always has
    something to send, mirroring how a real sensor keeps re-advertising
    its last event until a new one arrives.
    """

    def __init__(self, data_src: Bike) -> None:
        self.data_src = data_src

        self._crank_bridge = RateEventBridge()
        self._wheel_bridge = RateEventBridge()

        # Latched outputs in protocol-friendly units
        self.power = 0.0
        self.cadence = 0.0  # rpm
        self.speed = 0.0    # m/s
        self.crank_revs = 0
        self.crank_event_tick = 0
        self.wheel_revs = 0
        self.wheel_event_tick = 0

        # ANT+ power page accumulators
        self.power_event_count = 0
        self.cum_power = 0

        self.no_data = True

    async def loop(self):
        while True:
            await asyncio.sleep(0.05)

            now_tick = get_tick_now()
            state = self.data_src.state

            # Silence detection: if the source hasn't stamped fresh data
            # within SILENCE_TICKS, mark no_data and skip derivation.
            if state.last_update_tick == 0 or (now_tick - state.last_update_tick) > SILENCE_TICKS:
                self.no_data = True
                continue
            self.no_data = False

            # Crank channel: events take priority, else rate.
            if state.has_crank_event:
                self._crank_bridge.feed_event(state.crank_revs, state.crank_event_tick, now_tick)
            elif not math.isnan(state.cadence):
                self._crank_bridge.feed_rate(state.cadence / 60, now_tick)
            self.crank_revs, self.crank_event_tick = self._crank_bridge.get_event()
            self.cadence = self._crank_bridge.get_rate_rps() * 60

            # Wheel channel: events > speed > power-derived speed.
            if state.has_wheel_event:
                self._wheel_bridge.feed_event(state.wheel_revs, state.wheel_event_tick, now_tick)
            else:
                speed_mps = state.speed
                if math.isnan(speed_mps) and not math.isnan(state.power):
                    speed_mps = power_to_speed(state.power)
                if not math.isnan(speed_mps):
                    self._wheel_bridge.feed_rate(speed_mps / WHEEL_CIRCUMFERENCE, now_tick)
            self.wheel_revs, self.wheel_event_tick = self._wheel_bridge.get_event()
            self.speed = self._wheel_bridge.get_rate_rps() * WHEEL_CIRCUMFERENCE

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
