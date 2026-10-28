#!/usr/bin/env python3
"""The desktop around Paint for Linux, on a private D-Bus session:

  * a file:// URL on the command line (as "Open with" passes it) opens the image;
  * "Set as desktop background ▸ Fill" hands a screen-sized picture to the XDG desktop portal
    (org.freedesktop.portal.Wallpaper), played here by a fake portal so the real desktop's
    wallpaper is never touched.

Run from the repository root after building the dev preset:  tests/integration/desktop_portal.py
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

SESSION = ROOT / "tools/headless-session.sh"
SCREEN = (900, 600)
RED = (200, 20, 30)
BLUE = (20, 40, 210)


def serve_fake_portal(output: str) -> None:
    """Plays org.freedesktop.portal.Desktop: records what SetWallpaperFile receives, then answers."""
    import dbus
    import dbus.service
    from dbus.mainloop.glib import DBusGMainLoop
    from gi.repository import GLib

    DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()

    class Request(dbus.service.Object):
        @dbus.service.signal("org.freedesktop.portal.Request", signature="ua{sv}")
        def Response(self, response, results):  # noqa: N802 (D-Bus name)
            pass

    class Portal(dbus.service.Object):
        requests: list[Request] = []

        @dbus.service.method("org.freedesktop.portal.Wallpaper", in_signature="sha{sv}", out_signature="o",
                             sender_keyword="sender")
        def SetWallpaperFile(self, parent, fd, options, sender=None):  # noqa: N802 (D-Bus name)
            token = str(options.get("handle_token", "request"))
            path = f"/org/freedesktop/portal/desktop/request/{sender[1:].replace('.', '_')}/{token}"
            with os.fdopen(fd.take(), "rb") as picture:
                Path(output).write_bytes(picture.read())
            details = {"parent": str(parent), "options": {key: str(value) for key, value in options.items()}}
            Path(output + ".json").write_text(json.dumps(details))
            request = Request(bus, path)
            self.requests.append(request)

            def answer() -> bool:
                request.Response(dbus.UInt32(0), dbus.Dictionary({}, signature="sv"))
                return False

            GLib.timeout_add(50, answer)
            return dbus.ObjectPath(path)

    _name = dbus.service.BusName("org.freedesktop.portal.Desktop", bus)
    Portal(bus, "/org/freedesktop/portal/desktop")
    Path(output + ".ready").touch()
    GLib.MainLoop().run()


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)
    print(f"ok    {message}")


def wait_for(path: Path, seconds: float = 10) -> bool:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if path.exists():
            return True
        time.sleep(0.05)
    return False


def main() -> int:
    from PIL import Image

    from paintctl import PaintClient

    with tempfile.TemporaryDirectory() as tmp:
        bus = subprocess.Popen(["dbus-daemon", "--session", "--nofork", "--print-address=1"],
                               stdout=subprocess.PIPE, text=True)
        address = bus.stdout.readline().strip()
        env = dict(os.environ, DBUS_SESSION_BUS_ADDRESS=address)
        received = Path(tmp) / "received.png"
        portal = subprocess.Popen([sys.executable, "-I", __file__, "--serve", str(received)], env=env)
        try:
            check(wait_for(Path(str(received) + ".ready")), "the fake portal is on the private bus")
            # Left half red, right half blue: Fill covers the screen, cropping the sides.
            picture = Path(tmp) / "open with.png"
            image = Image.new("RGB", (300, 100), BLUE)
            image.paste(RED, (0, 0, 150, 100))
            image.save(picture)

            url = picture.as_uri()
            subprocess.run([SESSION, "start", "--platform", "offscreen", "--size", f"{SCREEN[0]}x{SCREEN[1]}",
                            "--", url], check=True, stdout=subprocess.DEVNULL, env=env)
            with PaintClient() as client:
                state = client.call("state")["app"]
                check(state["filePath"] == str(picture), f"a file:// URL argument opens the image ({url})")

                client.call("invoke", action="action.file.wallpaperFill")
                check(wait_for(received), "the portal receives a picture for the desktop")
                time.sleep(0.3)  # the answer arrives a moment later
                wallpaper = Image.open(received).convert("RGB")
                check(wallpaper.size == SCREEN, f"the picture has the screen's size ({wallpaper.size})")
                check(wallpaper.getpixel((10, SCREEN[1] // 2)) == RED and
                      wallpaper.getpixel((SCREEN[0] - 10, SCREEN[1] // 2)) == BLUE,
                      "Fill stretches the image over the screen")
                details = json.loads(Path(str(received) + ".json").read_text())
                check(details["options"].get("set-on") == "background", "only the desktop background is set")
                message = client.call("state")["app"]["statusMessage"]
                check("desktop background" in message, f"the status bar confirms it ({message!r})")
            return 0
        except AssertionError as error:
            print(f"FAIL  {error}")
            return 1
        finally:
            subprocess.run([SESSION, "stop"], check=False)
            portal.terminate()
            bus.terminate()
            portal.wait()
            bus.wait()


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--serve":
        serve_fake_portal(sys.argv[2])
    else:
        sys.exit(main())
