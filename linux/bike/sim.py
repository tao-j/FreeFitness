import asyncio, random

from . import *


class SimCrankPowerEncoder(Bike):
    def __init__(self) -> None:
        super().__init__()

        self.power = 0
        self.rev_inc = 0

    async def loop(self):
        interval = 0.1
        while True:
            await asyncio.sleep(interval)
            self.no_data = False
            self.rev_inc = interval * 1.1
            self.power = random.randint(120, 133)
            self.new_data.set()
            self.new_data.clear()
