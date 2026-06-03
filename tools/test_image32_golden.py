#!/usr/bin/env python3
"""Golden test: mm2_image32_codec.c vs render_view_refs.load_frame()."""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from render_view_refs import load_frame  # noqa: E402


def find_harness() -> Path | None:
    for candidate in (
        ROOT / "game" / "build" / "image32_golden.exe",
        ROOT / "game" / "build" / "image32_golden",
    ):
        if candidate.is_file():
            return candidate
    return None


def decode_c(harness: Path, path: Path, frame: int) -> bytes:
    proc = subprocess.run(
        [str(harness), str(path), str(frame)],
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode("utf-8", errors="replace") or "C decode failed")
    lines = proc.stdout.split(b"\n", 1)
    header = lines[0].decode("ascii")
    if not header.startswith("OK "):
        raise RuntimeError(f"unexpected harness output: {header!r}")
    return lines[1] if len(lines) > 1 else b""


def pil_rgba(path: Path, frame: int) -> bytes:
    im = load_frame(path.name, frame, path.parent)
    return im.tobytes()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def compare_file(data_dir: Path, sheet: str, frame: int, harness: Path, out_dir: Path) -> bool:
    path = data_dir / sheet
    if not path.is_file():
        print(f"SKIP missing {path}")
        return True

    py_bytes = pil_rgba(path, frame)
    c_bytes = decode_c(harness, path, frame)

    py_hash = sha256(py_bytes)
    c_hash = sha256(c_bytes)

    ok = py_bytes == c_bytes
    status = "PASS" if ok else "FAIL"
    print(f"{status} {sheet} frame {frame}: python={py_hash[:16]}… c={c_hash[:16]}… ({len(py_bytes)} bytes)")

    if not ok:
        out_dir.mkdir(parents=True, exist_ok=True)
        stem = f"{path.stem}_f{frame}"
        (out_dir / f"{stem}_python.rgba").write_bytes(py_bytes)
        (out_dir / f"{stem}_c.rgba").write_bytes(c_bytes)
        # Also write PNGs for visual diff
        try:
            from PIL import Image

            w, h = load_frame(path.name, frame, path.parent).size
            Image.frombytes("RGBA", (w, h), py_bytes).save(out_dir / f"{stem}_python.png")
            if len(c_bytes) == len(py_bytes):
                Image.frombytes("RGBA", (w, h), c_bytes).save(out_dir / f"{stem}_c.png")
        except ImportError:
            pass

    return ok


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data-dir", type=Path, default=ROOT, help="MM2 data directory")
    ap.add_argument(
        "--harness",
        type=Path,
        default=None,
        help="image32_golden binary (default: game/build/image32_golden*)",
    )
    ap.add_argument("--out", type=Path, default=ROOT / "game" / "test_out")
    args = ap.parse_args()

    harness = args.harness or find_harness()
    if harness is None:
        print("error: build image32_golden first (cmake --build game/build)", file=sys.stderr)
        return 2

    cases = [
        ("nwcp.32", 0),
        ("town.32", 0),
        ("castle.32", 0),
    ]

    all_ok = True
    for sheet, frame in cases:
        if not compare_file(args.data_dir, sheet, frame, harness, args.out):
            all_ok = False

    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
