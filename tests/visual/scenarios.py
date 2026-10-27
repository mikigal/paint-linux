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


def click(target: str, **extra: Any) -> Step:
    return ("click", {"target": target, **extra})


def invoke(action: str) -> Step:
    return ("invoke", {"action": f"action.{action}"})


def draw(path: list[list[int]], steps: int = 10) -> Step:
    # A polyline drag on the canvas; segments are interpolated so strokes look hand-drawn.
    points: list[list[int]] = [path[0]]
    for start, end in zip(path, path[1:]):
        for i in range(1, steps + 1):
            points.append([start[0] + (end[0] - start[0]) * i // steps, start[1] + (end[1] - start[1]) * i // steps])
    return ("drag", {"relativeTo": "canvasView.viewport", "path": points})


def slider(name: str, value: int) -> Step:
    return ("property", {"target": f"sidePanel.{name}.slider", "name": "value", "value": value})


# Large split buttons open their menu from the chevron in their lower part.
CHEVRON = {"at": [22, 53]}

# A small picture to select from: blue background with a thick red V.
PICTURE: list[Step] = [
    ("invoke", {"action": "action.tool.fill"}), ("click", {"target": "ribbon.palette.7"}),
    ("click", {"relativeTo": "canvasView.viewport", "x": 900, "y": 500}),
    ("invoke", {"action": "action.tool.brush"}), ("click", {"target": "ribbon.palette.3"}),
    ("property", {"target": "sidePanel.size.slider", "name": "value", "value": 25}),
]
PICTURE.append(draw([[200, 150], [350, 300], [500, 150]], steps=20))
# Moves the pointer off the canvas so hover effects do not depend on timing.
AWAY: Step = ("mouse", {"action": "move", "x": 1200, "y": 700})

SCENARIOS: list[Scenario] = [
    # Whole window
    Scenario("main-window-light"),
    Scenario("main-window-dark", theme="dark"),
    Scenario("main-window-paint-size-light", size=(1442, 910)),
    Scenario("narrow-950-light", size=(950, 700)),
    Scenario("narrow-800-dark", theme="dark", size=(800, 700)),
    Scenario("narrow-minimum-light", size=(600, 600)),
    # Menu bar
    Scenario("menu-file-light", steps=[click("menu.file")]),
    Scenario("menu-edit-dark", theme="dark", steps=[click("menu.edit")]),
    Scenario("menu-image-light", steps=[click("menu.image")]),
    Scenario("menu-view-light", steps=[click("menu.view")]),
    Scenario("menu-help-dark", theme="dark", steps=[click("menu.help")]),
    # Ribbon drop-downs
    Scenario("dropdown-select-light", steps=[click("ribbon.select", **CHEVRON)]),
    Scenario("dropdown-rotate-light", steps=[click("ribbon.rotate")]),
    Scenario("dropdown-flip-dark", theme="dark", steps=[click("ribbon.flip")]),
    Scenario("dropdown-brushes-light", steps=[click("ribbon.brushes", **CHEVRON)]),
    Scenario("dropdown-brushes-dark", theme="dark", steps=[click("ribbon.brushes", **CHEVRON)]),
    Scenario("dropdown-outline-light", steps=[invoke("shape.heart"), click("ribbon.outline")]),
    Scenario("dropdown-shape-fill-dark", theme="dark", steps=[invoke("shape.oval"), click("ribbon.shapeFill")]),
    Scenario("dropdown-shapes-compact-light", size=(1000, 700), steps=[click("ribbon.shapesCompact")]),
    Scenario("dropdown-colors-compact-light", size=(700, 600), steps=[click("ribbon.colorsCompact")]),
    Scenario("dropdown-toolbar-options-light", steps=[click("ribbon.toolbarOptions")]),
    # Tool states
    Scenario("tool-shape-selected-light", steps=[invoke("shape.fivePointStar")]),
    Scenario("tool-fill-tolerance-light", steps=[invoke("tool.fill")]),
    Scenario("tool-text-no-panel-dark", theme="dark", steps=[invoke("tool.text")]),
    Scenario("colors-secondary-active-light", steps=[click("ribbon.color2"), click("ribbon.palette.7")]),
    # Panels and modes
    Scenario("layers-panel-light", steps=[invoke("view.layersPanel")]),
    Scenario("layers-panel-dark", theme="dark", steps=[invoke("view.layersPanel")]),
    Scenario("autohide-strip-light", steps=[invoke("view.toolbarAutoHide")]),
    # The pointer is moved off the dropped-down ribbon so no hover effect depends on timing.
    Scenario("autohide-open-light", steps=[invoke("view.toolbarAutoHide"), click("ribbon.showToolbar"),
                                           ("mouse", {"action": "move", "x": 640, "y": 600})]),
    Scenario("context-menu-light",
             steps=[invoke("tool.rectangleSelect"), ("click", {"x": 400, "y": 400, "button": "right"})]),
    Scenario("zoom-200-light", steps=[invoke("view.zoomIn")]),
    Scenario("zoom-fit-dark", theme="dark", steps=[invoke("view.zoomFit")]),
    # Drawing (paths are in canvas viewport coordinates)
    Scenario("draw-pencil-light", steps=[draw([[150, 150], [300, 300], [450, 150], [600, 300], [750, 150]])]),
    Scenario("draw-brush-opacity-light", steps=[
        click("ribbon.palette.3"), invoke("tool.brush"), slider("size", 18), slider("opacity", 50),
        draw([[150, 200], [700, 380], [150, 380], [700, 200]], steps=40),
    ]),
    Scenario("draw-fill-dark", theme="dark", steps=[
        slider("size", 3), draw([[200, 150], [500, 150], [500, 400], [200, 400], [200, 150]]),
        invoke("tool.fill"), click("ribbon.palette.7"), ("click", {"relativeTo": "canvasView.viewport", "x": 350,
                                                                   "y": 280}),
    ]),
    Scenario("draw-eraser-outline-light", steps=[
        invoke("tool.fill"), click("ribbon.palette.8"), ("click", {"x": 600, "y": 400}),
        invoke("tool.eraser"), slider("size", 40), draw([[300, 250], [800, 450]], steps=30),
        ("mouse", {"action": "move", "x": 700, "y": 300}),
    ]),
    Scenario("draw-zoom-400-light", steps=[
        draw([[625, 305], [640, 311], [652, 324]]), invoke("view.zoomIn"), invoke("view.zoomIn"),
        invoke("view.zoomIn"),
    ]),
    # Selections (on a picture: a red V on blue)
    Scenario("select-rectangle-light", steps=[*PICTURE, invoke("tool.rectangleSelect"),
                                              ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120],
                                                        "to": [520, 320]}), AWAY]),
    Scenario("select-moved-light", steps=[*PICTURE, invoke("tool.rectangleSelect"),
                                          ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120],
                                                    "to": [520, 320]}),
                                          ("drag", {"relativeTo": "canvasView.viewport", "from": [350, 220],
                                                    "to": [650, 300]}), AWAY]),
    Scenario("select-rotated-dark", theme="dark", steps=[
        *PICTURE, invoke("tool.rectangleSelect"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120], "to": [520, 320]}),
        # The rotate handle sits 24 px above the middle of the top edge.
        ("drag", {"relativeTo": "canvasView.viewport", "from": [350, 96], "to": [560, 150], "modifiers": "shift"}),
        AWAY]),
    Scenario("select-freeform-light", steps=[*PICTURE, invoke("tool.freeformSelect"),
                                             draw([[200, 150], [450, 110], [560, 300], [300, 340], [200, 150]])]),
    Scenario("context-menu-selection-light", steps=[
        *PICTURE, invoke("tool.rectangleSelect"),
        ("drag", {"relativeTo": "canvasView.viewport", "from": [180, 120], "to": [520, 320]}),
        ("click", {"relativeTo": "canvasView.viewport", "x": 350, "y": 220, "button": "right"})]),
    Scenario("drop-image-light", steps=[
        ("drop", {"relativeTo": "canvasView.viewport", "x": 300, "y": 200,
                  "imagePath": "tests/golden/document/composite-two-layers.png"}), AWAY]),
    # Dialogs
    Scenario("dialog-about-light", steps=[invoke("help.about")], target="active"),
    Scenario("dialog-save-prompt-light", steps=[draw([[100, 100], [200, 200]]), invoke("file.new")],
             target="active"),
]
