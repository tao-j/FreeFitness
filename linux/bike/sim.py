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
        # Slow random walks around realistic cruising values. Step sizes are
        # small so cadence and power drift smoothly across pedal strokes
        # rather than jumping — closer to how a human actually rides.
        rpm = 70.0
        power = 150.0
        while True:
            rpm = max(50.0, min(95.0, rpm + random.uniform(-0.6, 0.6)))
            power = max(80.0, min(240.0, power + random.uniform(-4.0, 4.0)))
            await asyncio.sleep(60.0 / rpm)
            now_tick = get_tick_now()
            self.state.crank_revs += 1
            self.state.crank_event_tick = now_tick
            self.state.power = power
            self.state.last_update_tick = now_tick
