import asyncio

class Bike:
    def __init__(self) -> None:
        self.new_data = asyncio.Event()
        self.new_data.clear()
        self.no_data = True
