"""
Shared QMP client for the UV-K5 V3 emulator.

Supports both Unix domain sockets (Linux) and TCP sockets (Windows).
All Python tools should import QmpClient from here instead of reimplementing
the protocol.
"""

import json
import os
import socket
import sys

# Default connection: TCP on localhost:4444 (matches the built-in GUI).
# Override with environment variables or --socket argument.
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 4444
KEYPAD_PATH = "/machine/keypad"


class QmpClient:
    """Minimal QMP client: connect, negotiate, send commands."""

    def __init__(self, host=None, port=None, unix_path=None):
        if unix_path:
            # Unix domain socket (Linux/macOS)
            self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                self.sock.connect(unix_path)
            except (FileNotFoundError, ConnectionRefusedError) as exc:
                raise SystemExit(
                    f"cannot reach the emulator at {unix_path}: {exc}\n"
                    "Start the emulator first."
                ) from exc
        else:
            # TCP socket (Windows cross-platform)
            host = host or os.environ.get("UVK5_QMP_HOST", DEFAULT_HOST)
            port = port or int(os.environ.get("UVK5_QMP_PORT", DEFAULT_PORT))
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                self.sock.connect((host, port))
            except (ConnectionRefusedError, OSError) as exc:
                raise SystemExit(
                    f"cannot reach the emulator at {host}:{port}: {exc}\n"
                    "Start the emulator first (uvk5.exe or run.sh)."
                ) from exc

        self.buf = b""
        self._read_json()  # greeting
        self.command("qmp_capabilities")

    def _read_json(self):
        while b"\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise SystemExit("emulator closed the QMP connection")
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return json.loads(line)

    def command(self, name, **args):
        payload = {"execute": name}
        if args:
            payload["arguments"] = args
        self.sock.sendall(json.dumps(payload).encode() + b"\n")
        while True:
            msg = self._read_json()
            if "error" in msg:
                raise SystemExit(
                    f"QMP error: {msg['error'].get('desc', msg['error'])}"
                )
            if "return" in msg:
                return msg["return"]

    def press_key(self, key):
        """Hold the named key.  Pass empty string to release."""
        self.command("qom-set", path=KEYPAD_PATH, property="press", value=key)

    def get_pressed_key(self):
        """Read back which key is currently held (empty string = none)."""
        return self.command("qom-get", path=KEYPAD_PATH, property="press")


def parse_socket_args(parser):
    """Add --host, --port, --socket arguments to an argparse parser."""
    parser.add_argument("--host", default=None,
                        help="QMP host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=None,
                        help="QMP TCP port (default: 4444)")
    parser.add_argument("--socket", default=None,
                        help="Unix domain socket path (Linux/macOS only)")


def connect_from_args(args):
    """Create a QmpClient from parsed arguments."""
    if args.socket:
        return QmpClient(unix_path=args.socket)
    return QmpClient(host=args.host, port=args.port)
