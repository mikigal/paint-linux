#!/usr/bin/env python3
"""Runs the visual scenarios headlessly and compares screenshots with approved baselines.

    tests/visual/run.py               capture and compare (exit status 1 on any difference)
    tests/visual/run.py --update      capture and accept the results as the new baselines
    tests/visual/run.py --only menu   run scenarios whose name contains "menu"
    tests/visual/run.py --list        list scenarios

Screenshots land in tests/visual/output/ (with <name>.diff.png for mismatches); baselines live
in tests/visual/baselines/. Every scenario runs in a fresh app on the offscreen platform with the
deterministic KDE environment from tests/env, so results do not depend on the desktop settings.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from paintctl import PaintClient  # noqa: E402
from scenarios import SCENARIOS, Scenario  # noqa: E402

OUTPUT_DIR = ROOT / "tests/visual/output"
BASELINE_DIR = ROOT / "tests/visual/baselines"
SESSION = ROOT / "tools/headless-session.sh"
CHANNEL_TOLERANCE = 2


def capture(scenario: Scenario, build_dir: Path) -> Path:
    width, height = scenario.size
    subprocess.run(
        [SESSION, "start", "--platform", "offscreen", "--theme", scenario.theme,
         "--size", f"{width + 200}x{height + 200}", "--build", str(build_dir)],
        check=True, stdout=subprocess.DEVNULL)
    output = OUTPUT_DIR / f"{scenario.name}.png"
    try:
        with PaintClient() as client:
            client.call("resize", width=width, height=height)
            for command, args in scenario.steps:
                client.call(command, **args)
            shot_args = {"path": str(output)}
            if scenario.target:
                shot_args["target"] = scenario.target
            client.call("screenshot", **shot_args)
    finally:
        subprocess.run([SESSION, "stop"], check=False)
    return output


def compare(actual_path: Path, baseline_path: Path) -> str | None:
    """Returns None when the images match, otherwise a description of the difference."""
    actual = Image.open(actual_path).convert("RGBA")
    baseline = Image.open(baseline_path).convert("RGBA")
    if actual.size != baseline.size:
        return f"size {actual.size} differs from baseline {baseline.size}"
    delta = ImageChops.difference(actual, baseline)
    mask = Image.eval(delta, lambda v: 255 if v > CHANNEL_TOLERANCE else 0)
    channels = mask.split()
    combined = channels[0]
    for channel in channels[1:]:
        combined = ImageChops.lighter(combined, channel)
    different = combined.width * combined.height - combined.histogram()[0]
    if not different:
        return None
    diff = actual.convert("L").point(lambda v: 200 + v // 5).convert("RGB")
    diff.paste((255, 0, 0), mask=combined)
    diff.save(actual_path.with_suffix(".diff.png"))
    return f"{different} pixels differ (see {actual_path.with_suffix('.diff.png')})"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--update", action="store_true", help="accept screenshots as new baselines")
    parser.add_argument("--only", default="", help="substring filter on scenario names")
    parser.add_argument("--list", action="store_true", help="list scenarios and exit")
    parser.add_argument("--build", type=Path, default=ROOT / "build/dev", help="build directory")
    options = parser.parse_args()

    scenarios = [s for s in SCENARIOS if options.only in s.name]
    if options.list:
        for scenario in scenarios:
            print(scenario.name)
        return 0

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    BASELINE_DIR.mkdir(parents=True, exist_ok=True)
    failures = 0
    for scenario in scenarios:
        actual = capture(scenario, options.build)
        baseline = BASELINE_DIR / actual.name
        if options.update:
            shutil.copyfile(actual, baseline)
            print(f"updated  {scenario.name}")
            continue
        if not baseline.exists():
            print(f"MISSING  {scenario.name}: no baseline (review {actual}, then run with --update)")
            failures += 1
            continue
        problem = compare(actual, baseline)
        if problem:
            print(f"DIFFERS  {scenario.name}: {problem}")
            failures += 1
        else:
            print(f"ok       {scenario.name}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
