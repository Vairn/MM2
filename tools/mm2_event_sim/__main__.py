#!/usr/bin/env python3
"""MM2 event.dat interactive simulator.

Split TUI: map grid | event log | A4 memory inspector.

  python -m tools.mm2_event_sim --location 0
  python tools/mm2_event_sim/__main__.py 1

Requires: pip install textual
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))


def main() -> None:
    parser = argparse.ArgumentParser(description="MM2 event.dat TUI simulator")
    parser.add_argument("--location", "-l", type=int, default=0, help="event.dat location 0..70")
    parser.add_argument("--x", type=int, default=8)
    parser.add_argument("--y", type=int, default=8)
    parser.add_argument("--gold", type=int, default=1000)
    parser.add_argument("--era", type=int, default=0)
    args = parser.parse_args()

    try:
        from mm2_event_sim.tui import run_tui
    except ImportError:
        from tools.mm2_event_sim.tui import run_tui

    run_tui(args.location, args.x, args.y, args.gold, args.era)


if __name__ == "__main__":
    main()
