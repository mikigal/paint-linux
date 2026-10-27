#!/usr/bin/env python3
"""Cross-application clipboard against a real Wayland compositor (a headless KWin).

  * an image another client puts on the clipboard (wl-copy) pastes into Paint for Linux;
  * an image copied in Paint for Linux can be read by other clients (wl-paste) as PNG.

Run from the repository root after building the dev preset:  tests/integration/clipboard_kwin.py
"""

from __future__ import annotations

import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from paintctl import PaintClient  # noqa: E402

SESSION = ROOT / "tools/headless-session.sh"
COLOR = (12, 34, 156)


def session_environment() -> dict[str, str]:
    env = dict(os.environ)
    for line in subprocess.run([SESSION, "env"], check=True, capture_output=True, text=True).stdout.splitlines():
        command, _, assignment = line.partition(" ")
        if command == "export":
            key, _, value = assignment.partition("=")
            env[key] = shlex.split(value)[0] if value else ""
        elif command == "unset":
            env.pop(assignment, None)
    return env


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"ok    {message}")


def main() -> int:
    subprocess.run([SESSION, "start", "--platform", "kwin"], check=True, stdout=subprocess.DEVNULL)
    try:
        env = session_environment()
        with tempfile.TemporaryDirectory() as tmp, PaintClient() as client:
            source = Path(tmp) / "source.png"
            Image.new("RGB", (20, 10), COLOR).save(source)

            # Another application copies an image.
            with open(source, "rb") as data:
                # wl-copy keeps serving in the background until the session ends; keep it quiet.
                subprocess.run(["wl-copy", "--type", "image/png"], stdin=data, env=env, check=True, timeout=10,
                               stderr=subprocess.DEVNULL)
            client.call("wait", ms=300)
            enabled = client.call("property", target="action.edit.paste", name="enabled")["value"]
            check(enabled, "Paste is enabled for an image copied in another application")

            client.call("invoke", action="action.edit.paste")
            client.call("wait", ms=300)
            state = client.call("state")["app"]
            check(state["floating"] and state["selection"] == [0, 0, 20, 10], "the image is pasted as a selection")
            pixel = client.call("call", target="mainWindow", method="documentPixel", args=[5, 5])["value"]
            expected = "#ff{:02x}{:02x}{:02x}".format(*COLOR)
            check(pixel == expected, f"pasted pixels match ({pixel})")

            # Paint for Linux copies the selection; other applications read it.
            client.call("invoke", action="action.edit.copy")
            client.call("wait", ms=300)
            types = subprocess.run(["wl-paste", "--list-types"], env=env, capture_output=True, text=True,
                                   timeout=10).stdout.split()
            check("image/png" in types, f"the copy is offered as image/png ({', '.join(types[:4])}…)")
            copied = Path(tmp) / "copied.png"
            with open(copied, "wb") as out:
                subprocess.run(["wl-paste", "--type", "image/png"], stdout=out, env=env, check=True, timeout=10)
            image = Image.open(copied).convert("RGB")
            check(image.size == (20, 10) and image.getpixel((5, 5)) == COLOR,
                  "another application gets the same pixels back")
        return 0
    except AssertionError as error:
        print(f"FAIL  {error}")
        return 1
    finally:
        subprocess.run([SESSION, "stop"], check=False)


if __name__ == "__main__":
    sys.exit(main())
