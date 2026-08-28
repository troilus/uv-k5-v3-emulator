#!/usr/bin/env python3
"""Read a QOM property from the running emulator.

Usage: qom_get.py <path> <property>
       qom_get.py /machine/keypad press
"""

import json
import sys

from qmp_client import QmpClient, parse_socket_args, connect_from_args


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("path", help="QOM path")
    ap.add_argument("property", help="property name")
    parse_socket_args(ap)
    args = ap.parse_args()

    qmp = connect_from_args(args)
    value = qmp.command("qom-get", path=args.path, property=args.property)
    print(json.dumps(value))
    return 0


if __name__ == "__main__":
    sys.exit(main())
