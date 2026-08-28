#!/usr/bin/env python3
"""Press keys on the emulated radio through its QMP socket.

The keypad model exposes a "press" property: writing a key name holds that key,
writing an empty string releases it. The firmware debounces over several 10 ms
polls, so a press has to be held for a while to register -- see HOLD_MS.

Usage:
    key.py MENU              # one short press
    key.py MENU UP UP EXIT   # a sequence
    key.py --long F          # long press
    key.py --list            # show key names
"""

import argparse
import sys
import time

from qmp_client import QmpClient, parse_socket_args, connect_from_args

# App/app/app.c debounces in 10 ms timeslices driven by the SysTick interrupt:
#   key_debounce_10ms     =  2  -> 20 ms to register a press
#   key_repeat_delay_10ms = 40  -> 400 ms counts as a key *held*
HOLD_MS = 200          # ~20 ticks: past debounce, well short of the 40-tick hold
LONG_HOLD_MS = 900     # ~90 ticks: comfortably past the hold threshold
GAP_MS = 400           # let the release be debounced before the next press

KEYS = [
    "MENU", "UP", "DOWN", "EXIT", "F", "STAR",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "SIDE1", "SIDE2",
]


def press(qmp, key, hold_ms):
    qmp.press_key(key)
    time.sleep(hold_ms / 1000)
    qmp.press_key("")
    time.sleep(GAP_MS / 1000)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("keys", nargs="*", help="key names to press in order")
    ap.add_argument("--long", action="store_true", help="hold each key longer")
    ap.add_argument("--hold", type=int, help="hold time in ms, overrides --long")
    ap.add_argument("--list", action="store_true", help="list key names and exit")
    parse_socket_args(ap)
    args = ap.parse_args()

    if args.list:
        print(" ".join(KEYS))
        return 0
    if not args.keys:
        ap.error("no keys given (try --list)")

    unknown = [k for k in args.keys if k.upper() not in KEYS]
    if unknown:
        raise SystemExit(f"unknown key(s): {', '.join(unknown)}\nKnown: {' '.join(KEYS)}")

    hold = args.hold if args.hold else (LONG_HOLD_MS if args.long else HOLD_MS)
    qmp = connect_from_args(args)

    for key in args.keys:
        press(qmp, key.upper(), hold)
        print(f"pressed {key.upper()} ({hold} ms)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
