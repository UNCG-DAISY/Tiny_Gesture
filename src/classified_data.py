# -*- coding: utf-8 -*-
"""
classified_data
-------------
Get classified gesture data(gesture, probability, elapsed time) through BLE from arduino board by connecting to the CHARACTERISTIC_UUID.
"""
import logging
import asyncio
import platform

from bleak import BleakClient
from bleak import _logger as logger


CHARACTERISTIC_UUID = "00002a17-0000-1000-8000-00805f9b34fb"

def notification_handler(sender, data):
    """Simple notification handler which prints the data received."""
    data = bytes(data).decode("utf-8").split(",")
    print("\033[1m" + '\033[92m' + data[0]  + "\033[0m" + "\033[0m")
    print("\033[1m" + '\033[92m' + data[1]  + "\033[0m" + "\033[0m")
    print("\033[1m" + '\033[92m' + data[2]  + "\033[0m" + "\033[0m")
    print("\n")


async def run(address, loop, debug=False):
    if debug:
        import sys

        loop.set_debug(True)
        l = logging.getLogger("asyncio")
        l.setLevel(logging.DEBUG)
        h = logging.StreamHandler(sys.stdout)
        h.setLevel(logging.DEBUG)
        l.addHandler(h)
        logger.addHandler(h)

    async with BleakClient(address, loop=loop) as client:
        x = await client.is_connected()
        logger.info("Connected: {0}".format(x))

        await client.start_notify(CHARACTERISTIC_UUID, notification_handler)
        await asyncio.sleep(50000000, loop=loop)
        await client.stop_notify(CHARACTERISTIC_UUID)

if __name__ == "__main__":
    import os

    os.environ["PYTHONASYNCIODEBUG"] = str(1)
    address = (
        "d4:61:9d:09:ff:94"
        if platform.system() != "Darwin"
        else "226A7E50-443F-4175-BDE3-6BA587C98CA3"
    )
    loop = asyncio.get_event_loop()
    loop.run_until_complete(run(address, loop, True))