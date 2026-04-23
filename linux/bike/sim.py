import asyncio
import random

from . import *
from clock import now_tick
from tx import CountGenerator


class SimulatedCrankPowerSource(Bike):
    """Simulates a hardware crank encoder.

    Uses CountGenerator internally to integrate the cadence rate and emit
    discrete events (state.crank_revs + state.crank_event_tick), the same
    way a real hall-sensor would. The protocol encoder passes them through
    without interpolation.
    """

    def __init__(self) -> None:
        super().__init__()
        self._crank_gen = CountGenerator()
        self.state.has_crank_event = True

    async def loop(self):
        interval = 0.1
        while True:
            await asyncio.sleep(interval)
            now = now_tick()

            self._crank_gen.inc_count(interval * 1.1, now)
            self.state.crank_revs, self.state.crank_event_tick = self._crank_gen.get_event()
            self.state.power = float(random.randint(120, 133))
            self.state.last_update_tick = now

