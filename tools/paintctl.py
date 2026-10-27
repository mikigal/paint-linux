#!/usr/bin/env python3
"""Command-line client and Python API for the Paint for Linux automation server.

The application must be built with PAINT_AUTOMATION=ON (the `dev` preset does this) and started
with --automation-socket (tools/headless-session.sh takes care of both).

Examples:
    paintctl.py ping
    paintctl.py tree
    paintctl.py click target=menu.file
    paintctl.py drag relativeTo=workspace from=[10,10] to=[200,120] modifiers=shift
    paintctl.py key keys=Ctrl+Z
    paintctl.py screenshot path=/tmp/shot.png
    paintctl.py drop target=workspace urls='["/path/to/image.png"]'
    paintctl.py --batch steps.txt      # one command per line, same syntax (or raw JSON)

Arguments are key=value pairs; values are parsed as JSON when possible (numbers, booleans,
lists, objects) and fall back to plain strings.
"""

from __future__ import annotations

import argparse
import json
import os
import shlex
import socket
import sys
from pathlib import Path
from typing import Any

DEFAULT_STATE_DIR = Path(os.environ.get("XDG_RUNTIME_DIR", "/tmp")) / "paint-for-linux-test"


def default_socket_path() -> str:
    return os.environ.get("PAINT_AUTOMATION_SOCKET", str(DEFAULT_STATE_DIR / "automation.sock"))


class AutomationError(RuntimeError):
    pass


class PaintClient:
    """Synchronous client: every call sends one request line and waits for one reply line."""

    def __init__(self, socket_path: str | None = None, timeout: float = 30.0):
        self.socket_path = socket_path or default_socket_path()
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(timeout)
        try:
            self._sock.connect(self.socket_path)
        except OSError as error:
            raise AutomationError(f"cannot connect to {self.socket_path}: {error}") from error
        self._buffer = b""

    def close(self) -> None:
        self._sock.close()

    def __enter__(self) -> "PaintClient":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def request(self, payload: dict[str, Any]) -> dict[str, Any]:
        self._sock.sendall(json.dumps(payload).encode() + b"\n")
        while b"\n" not in self._buffer:
            chunk = self._sock.recv(65536)
            if not chunk:
                raise AutomationError("connection closed by the application")
            self._buffer += chunk
        line, self._buffer = self._buffer.split(b"\n", 1)
        return json.loads(line)

    def call(self, cmd: str, **args: Any) -> dict[str, Any]:
        """Runs a command and raises AutomationError if the application reports a failure."""
        reply = self.request({"cmd": cmd, **args})
        if not reply.get("ok"):
            raise AutomationError(f"{cmd}: {reply.get('error', 'unknown error')}")
        return reply


def parse_value(text: str) -> Any:
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


def parse_command(tokens: list[str]) -> dict[str, Any]:
    if not tokens:
        raise ValueError("empty command")
    if tokens[0].startswith("{"):
        return json.loads(" ".join(tokens))
    payload: dict[str, Any] = {"cmd": tokens[0]}
    for token in tokens[1:]:
        key, sep, value = token.partition("=")
        if not sep:
            raise ValueError(f"expected key=value, got '{token}'")
        payload[key] = parse_value(value)
    return payload


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--socket", default=None, help="automation socket path")
    parser.add_argument("--batch", help="file with one command per line ('-' for stdin)")
    parser.add_argument("--compact", action="store_true", help="print replies on one line")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    options = parser.parse_args()

    if options.batch:
        source = sys.stdin if options.batch == "-" else open(options.batch, encoding="utf-8")
        lines = [line.strip() for line in source if line.strip() and not line.lstrip().startswith("#")]
        commands = [parse_command(shlex.split(line)) for line in lines]
    else:
        commands = [parse_command(options.command)]

    status = 0
    with PaintClient(options.socket) as client:
        for payload in commands:
            reply = client.request(payload)
            print(json.dumps(reply, ensure_ascii=False, indent=None if options.compact else 2))
            if not reply.get("ok"):
                status = 1
                break
    return status


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AutomationError, ValueError) as error:
        print(f"paintctl: {error}", file=sys.stderr)
        sys.exit(2)
