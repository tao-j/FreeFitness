import asyncio
import random

from . import Bike
from bridge import get_tick_now


class SimulatedCrankPowerSource(Bike):
    """Simulates a hardware hall-effect crank sensor.

    Emits one discrete event per revolution (crank_revs + crank_event_tick) —
    exactly what a real hall-effect interrupt would produce, nothing more.
    The encoder's RateEventBridge derives cadence from Δcount / Δtick.
    Power is sampled independently at each revolution, mirroring a
    crank-based power meter that reports per pedal stroke.
    """

    def __init__(self) -> None:
        super().__init__()
        self.state.has_crank_event = True

    async def loop(self):
        target_rpm = 66.0
        period_s = 60.0 / target_rpm
        while True:
            await asyncio.sleep(period_s)
            now_tick = get_tick_now()
            self.state.crank_revs += 1
            self.state.crank_event_tick = now_tick
            self.state.power = float(random.randint(120, 133))
            self.state.last_update_tick = now_tick
