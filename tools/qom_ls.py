#!/usr/bin/env python3
"""List QOM child nodes under a path, to find where a device actually lives.

Usage: qom_ls.py [/machine]
"""

import sys

from qmp_client import QmpClient, parse_socket_args, connect_from_args


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("path", nargs="?", default="/machine", help="QOM path")
    parse_socket_args(ap)
    args = ap.parse_args()

    qmp = connect_from_args(args)
    for item in qmp.command("qom-list", path=args.path):
        kind = item.get("type", "")
        marker = "dir" if kind.startswith("child<") else "   "
        print(f"{marker}  {item['name']:24s} {kind}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
