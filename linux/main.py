import asyncio
import sys
import argparse


def parse_args():
    parser = argparse.ArgumentParser(description="Bike power transmission utility")
    parser.add_argument(
        "--bike-id",
        type=int,
        default=0,
        help="Keiser bike ID (default: 0 for simulation)",
    )
    parser.add_argument(
        "--protocols",
        type=str,
        default="ant",
        help="Comma-separated list of protocols to use (ant,ble). Default: ant",
    )
    parser.add_argument(
        "--mock",
        action="store_true",
        help="Use simulated power data instead of real bike",
    )
    parser.add_argument(
        "--ant-driver",
        type=str,
        default="usb2",
        choices=["usb1", "usb2"],
        help="ANT driver type: usb1 (serial) or usb2 (direct USB). Default: usb2",
    )
    return parser.parse_args()


async def main(bike_id: int, protocols: list[str], mock: bool, ant_driver: str):
    tasks = []

    # Import and initialize bike source
    if mock:
        from bike.sim import SimCrankPowerEncoder

        src = SimCrankPowerEncoder()
    else:
        from bike.keiser import KeiserBike

        src = KeiserBike(bike_id=bike_id)
    tasks.append(src.loop())

    # Configure ANT+ transmission
    if "ant" in protocols:
        from tx.ant import ANTTx
        from tx.conv import ANTConv

        ant_tx = ANTTx(driver_type=ant_driver)
        ant_bike_data = ANTConv(src)
        tasks.extend([ant_bike_data.loop(), ant_tx.loop(bike_data=ant_bike_data)])

    # Configure BLE transmission
    if "ble" in protocols:
        from tx.ble import BLETx
        from tx.conv import BLEConv

        ble_tx = BLETx()
        await ble_tx.setup()
        ble_bike_data = BLEConv(src)
        tasks.extend([ble_bike_data.loop(), ble_tx.loop(bike_data=ble_bike_data)])

    try:
        async with asyncio.TaskGroup() as g:
            for task in tasks:
                g.create_task(task)

    except asyncio.exceptions.CancelledError:
        print("Cancelled by user")


if __name__ == "__main__":
    args = parse_args()
    protocols = [p.strip().lower() for p in args.protocols.split(",")]

    # Validate protocols
    valid_protocols = {"ant", "ble"}
    if not all(p in valid_protocols for p in protocols):
        print(
            f"Error: Invalid protocol(s). Valid options are: {', '.join(valid_protocols)}"
        )
        sys.exit(1)

    asyncio.run(main(args.bike_id, protocols, args.mock, args.ant_driver))
