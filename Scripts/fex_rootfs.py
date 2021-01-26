#!/bin/env python3
# client.py
import os
import requests
import wget
import sys
import json

fex_dist = "http://dist.fex-emu.org/info.json"

def get_info():
    return json.loads(requests.get(fex_dist).text)

def available():
    items = get_info()
    for item in items:
        print(f"name: {item['name']}")
        print(f"hash: {item['sha256']}")
        print(f"desc:\n\t{item['desc']}\n")

def installed():
    print("no rootfses installed")

def fetch():
    if len(sys.argv) != 3:
        print("Invalid arguments")
        exit(-1)
    rootfs = sys.argv[2]
    items = get_info()
    item = next(filter(lambda x: x['name'] == rootfs, items), None)
    if not item:
        print(f"{rootfs} is not available")
        exit(-2)
    print(f"fetching {rootfs}")
    wget.download(item['url'])

if __name__ == "__main__":
    if len(sys.argv) <= 1:
        command = "help"
    else:
        command = sys.argv[1]
    commands = { "installed": installed, "available": available, "fetch": fetch };

    if command in commands:
        commands[command]()
    else:
        print("fex rootfs helper\n")
        print("valid arguments: installed, available, fetch <image>\n\tlist: show installed fex rootfses\n\tavailable: show rootfses available for download\n\tfetch <name>: download and extract a rootfs")
