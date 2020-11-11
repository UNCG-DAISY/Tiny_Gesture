# -*- coding: utf-8 -*-
"""
Notifications
-------------
Example showing how to add notifications to a characteristic and handle the responses.
Updated on 2019-07-03 by hbldh <henrik.blidh@gmail.com>
"""

import logging
import asyncio
import platform

from bleak import BleakClient
from bleak import _logger as logger
import csv
import json

CHARACTERISTIC_UUID = "00002a18-0000-1000-8000-00805f9b34fb"  # <--- Change to the characteristic you want to enable notifications from.
FILENAME = "run_set_each_new42.csv"

def notification_handler(sender, data):
    """Simple notification handler which prints the data received."""
    s_data = "{" + bytes(data).decode("utf-8") + "}"
    data = json.loads(s_data)
    print(data)
    print(notification_handler.counter)
    fields = ['aX', 'aY', 'aZ', 'gX', 'gY', 'gZ']   
    # name of csv file  
    filename = FILENAME
    # writing to csv file  
    if notification_handler.counter < 1:
        fields = ['aX', 'aY', 'aZ', 'gX', 'gY', 'gZ']   
        # name of csv file  
        filename = FILENAME 
        # writing to csv file  
        with open(filename, 'w') as csvfile:  
            # creating a csv dict writer object  
            writer = csv.DictWriter(csvfile, fieldnames = fields)  
            # writing headers (field names)  
            writer.writeheader()  
            # writing data rows  
            # writer.writerow(data)
    notification_handler.counter += 1
    with open(filename, 'a') as csvfile:  
        # creating a csv dict writer object  
        writer = csv.DictWriter(csvfile, fieldnames = fields)  
        # writing data rows  
        writer.writerow(data)

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
        notification_handler.counter = 0
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