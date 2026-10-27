"""Visual scenarios: each one starts a fresh app, runs automation steps and takes one screenshot.

Steps are (command, arguments) pairs for the automation server (see `paintctl.py help`).
Screenshots are compared against tests/visual/baselines/<name>.png by run.py.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

Step = tuple[str, dict[str, Any]]


@dataclass(frozen=True)
class Scenario:
    name: str
    theme: str = "light"
    size: tuple[int, int] = (1280, 800)
    steps: list[Step] = field(default_factory=list)
    target: str | None = None  # screenshot target; defaults to the main window with popups


SCENARIOS: list[Scenario] = [
    Scenario("main-window-light"),
    Scenario("main-window-dark", theme="dark"),
    Scenario("menu-file-light", steps=[("click", {"target": "menu.file"})]),
    Scenario("menu-help-dark", theme="dark", steps=[("click", {"target": "menu.help"})]),
    Scenario("dialog-about-light", steps=[("invoke", {"action": "action.help.about"})], target="active"),
]
