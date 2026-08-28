#!/usr/bin/env python3
"""Verify the keypad GPIO wiring in the running machine.

qdev out-GPIOs are QOM link properties, so the board's wiring is directly
observable: /machine/soc/b "pin-out[6]" should point at a keypad "col" input,
and /machine/keypad "row[0]" should point at a GPIOB "pin-in" input.
"""

import sys

from qmp_client import QmpClient, parse_socket_args, connect_from_args


def get(q, path, prop):
    try:
        r = q.command("qom-get", path=path, property=prop)
        return r
    except SystemExit:
        return "ERR"


def main():
    qmp = connect_from_args(type('Args', (), {'host': None, 'port': None, 'socket': None})())

    print("== /machine children ==")
    for it in qmp.command("qom-list", path="/machine"):
        print(f"   {it['name']:20s} {it.get('type','')}")

    print("\n== GPIOB column outputs (pins 6..3 = cols 1..4) ==")
    for c in range(1, 5):
        pin = 6 - (c - 1)
        print(f"   pin-out[{pin}] -> {get(qmp, '/machine/soc/b', f'pin-out[{pin}]')}")

    print("\n== keypad row outputs (rows 0..3 -> pins 15..12) ==")
    for r in range(4):
        print(f"   row[{r}] -> {get(qmp, '/machine/keypad', f'row[{r}]')}")

    print("\n== keypad col input objects ==")
    for it in qmp.command("qom-list", path="/machine/keypad"):
        if it["name"].startswith(("col[", "row[")):
            print(f"   {it['name']:12s} {it.get('type','')}")

    print("\n== GPIOB pin-in objects for the row pins ==")
    for it in qmp.command("qom-list", path="/machine/soc/b"):
        if it["name"].startswith("pin-in["):
            n = int(it["name"][7:-1])
            if n >= 12:
                print(f"   {it['name']:12s} {it.get('type','')}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
