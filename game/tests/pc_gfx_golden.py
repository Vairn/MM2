#!/usr/bin/env python3
"""Compare mm2_pc_gfx_codec RGBA against decode_pc_gfx.py for golden cases."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def load_dump_rgba(repo_root: Path, path: Path, role: str, cga_palette: int = 1) -> bytes:
    proc = subprocess.run(
        [
            sys.executable,
            str(repo_root / "tools" / "decode_pc_gfx.py"),
            str(path),
            "--frame",
            "0",
            "--cga-palette",
            str(cga_palette),
            "--role",
            role,
            "--dump-rgba",
        ],
        capture_output=True,
        check=True,
    )
    data = proc.stdout
    line, body = data.split(b"\n", 1)
    if not line.startswith(b"OK "):
        raise RuntimeError(f"bad python header: {line!r}")
    return body


def load_c_rgba(dump_exe: Path, path: Path, role: str, cga_palette: int = 1) -> bytes:
    proc = subprocess.run(
        [str(dump_exe), str(path), role, "0", str(cga_palette)],
        capture_output=True,
        check=True,
    )
    data = proc.stdout
    line, body = data.split(b"\n", 1)
    if not line.startswith(b"OK "):
        raise RuntimeError(f"bad c header: {line!r}")
    return body


def compare_case(repo_root: Path, dump_exe: Path, path: Path, role: str, cga_palette: int = 1) -> None:
    py = load_dump_rgba(repo_root, path, role, cga_palette)
    c = load_c_rgba(dump_exe, path, role, cga_palette)
    if py != c:
        mismatches = sum(1 for a, b in zip(py, c) if a != b)
        mismatches += abs(len(py) - len(c))
        raise AssertionError(f"{path.name}: {mismatches} byte mismatches (py={len(py)} c={len(c)})")
    print(f"OK {path.name} frame0 {len(py) // 4} pixels")


def main() -> int:
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <pc_data_dir> <build_dir>", file=sys.stderr)
        return 2
    pc_dir = Path(sys.argv[1])
    build_dir = Path(sys.argv[2])
    repo_root = build_dir.parent.parent
    dump_exe = build_dir / ("pc_gfx_rgba_dump.exe" if sys.platform == "win32" else "pc_gfx_rgba_dump")
    if not dump_exe.is_file():
        print(f"FAIL: missing {dump_exe}", file=sys.stderr)
        return 1

    cases = [
        (pc_dir / "TOWN.4", "wall"),
        (pc_dir / "TOWN.16", "wall"),
        (pc_dir / "SKY.4", "sky"),
    ]
    fails = 0
    for path, role in cases:
        if not path.is_file():
            print(f"FAIL: missing {path}", file=sys.stderr)
            fails += 1
            continue
        try:
            compare_case(repo_root, dump_exe, path, role)
        except Exception as exc:
            print(f"FAIL: {exc}", file=sys.stderr)
            fails += 1
    if fails:
        print(f"pc_gfx_golden: {fails} case(s) failed", file=sys.stderr)
        return 1
    print("pc_gfx_golden: all cases passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
