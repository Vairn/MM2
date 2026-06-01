#!/usr/bin/env python3
"""Export compact map.dat + attrib.dat JSON for the wiki maze walker."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

from mm2_gfx_export import (  # noqa: E402
    area_name,
    load_attrib,
    load_map_screens,
    resolve_data_dir,
)


def export_map_walker(out_path: Path, data_dir: Path | None = None) -> dict:
    data = resolve_data_dir(data_dir)
    attrib = load_attrib(data)
    screens = load_map_screens(data)

    payload: dict = {"version": 1, "screens": []}
    for sid in range(60):
        visual, collision = screens[sid]
        a = attrib[sid]
        entry_x = a[0x0E] & 0x0F
        entry_y = (a[0x0E] >> 4) & 0x0F
        payload["screens"].append(
            {
                "id": sid,
                "name": area_name(sid, attrib),
                "outdoor": a[0x04] != 0,
                "entry": [entry_x, entry_y],
                "neighbors": [a[0x05], a[0x06], a[0x07], a[0x08]],
                "visual": list(visual),
                "collision": list(collision),
            }
        )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, separators=(",", ":")), encoding="utf-8")
    print(f"  maze walker: {out_path} ({len(payload['screens'])} screens)")
    return payload


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Export maps.json for the maze walker")
    ap.add_argument(
        "-o",
        "--out",
        type=Path,
        default=ROOT / "wiki" / "maze-walker" / "maps.json",
    )
    ap.add_argument("--data", type=Path, default=None)
    args = ap.parse_args()
    try:
        export_map_walker(args.out, args.data)
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
