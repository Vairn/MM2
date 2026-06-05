#!/usr/bin/env python3
"""Build montage PNGs using the C++ Gfx decoder (same as MM2ED).

MM2ED and this tool share editor/src/core/Gfx.cpp. Python decode in
scifi_town_common.py is used for encoding but previews must go through C++
to match what you see when you Open the .32 in the editor.
"""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    raise SystemExit("pip install pillow")

ROOT = Path(__file__).resolve().parents[1]
GFX_DUMP = ROOT / "editor" / "tests" / "gfx_dump.exe"


def ensure_gfx_dump() -> Path:
    if GFX_DUMP.exists():
        return GFX_DUMP
    src = ROOT / "editor" / "tests" / "gfx_dump.cpp"
    core = ROOT / "editor" / "src" / "core"
    cmd = [
        "g++", "-std=c++17", f"-I{core.parent}",
        str(src), str(core / "Gfx.cpp"), str(core / "ByteIO.cpp"),
        "-o", str(GFX_DUMP),
    ]
    subprocess.run(cmd, check=True)
    return GFX_DUMP


def export_rgba(sheet: Path, out_dir: Path) -> list[Image.Image]:
    out_dir.mkdir(parents=True, exist_ok=True)
    dump = ensure_gfx_dump()
    subprocess.run([str(dump), "--export", str(out_dir), str(sheet.resolve())], check=True)
    frames: list[Image.Image] = []
    for rgba in sorted(out_dir.glob("f*.rgba")):
        raw = rgba.read_bytes()
        w, h = struct.unpack("<II", raw[:8])
        data = raw[8:]
        im = Image.frombytes("RGBA", (w, h), data)
        frames.append(im)
    return frames


def checker(w, h, cell=8):
    bg = Image.new("RGBA", (w, h), (40, 40, 48, 255))
    d = ImageDraw.Draw(bg)
    for y in range(0, h, cell):
        for x in range(0, w, cell):
            if ((x // cell) + (y // cell)) & 1:
                d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(56, 56, 66, 255))
    return bg


def montage_rgba(frames: list[Image.Image], cols: int, scale: int, title: str) -> Image.Image:
    pad, lh = 6, 12
    cw = max(im.width for im in frames) * scale
    ch = max(im.height for im in frames) * scale
    cw2, ch2 = cw + pad * 2, ch + pad * 2 + lh
    rows = (len(frames) + cols - 1) // cols
    cv = Image.new("RGBA", (cols * cw2, rows * ch2 + 18), (24, 24, 30, 255))
    d = ImageDraw.Draw(cv)
    d.text((6, 4), title, fill=(220, 220, 230, 255))
    for i, im in enumerate(frames):
        cx, cy = (i % cols) * cw2, (i // cols) * ch2 + 18
        sf = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
        tile = checker(cw, ch)
        tile.alpha_composite(sf, ((cw - sf.width) // 2, (ch - sf.height) // 2))
        cv.alpha_composite(tile, (cx + pad, cy + pad + lh))
        d.text((cx + pad, cy + 1), f"{i}", fill=(180, 200, 255, 255))
    return cv


def montage_from_sheet(sheet: Path, out_png: Path, cols: int = 8, scale: int = 3,
                       title: str | None = None) -> Path:
    tmp = out_png.parent / f"_cpp_export_{sheet.stem}"
    frames = export_rgba(sheet, tmp)
    title = title or f"{sheet.name} (MM2ED / Gfx.cpp decode)"
    img = montage_rgba(frames, cols, scale, title)
    out_png.parent.mkdir(parents=True, exist_ok=True)
    img.save(out_png)
    return out_png


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: python tools/cpp_gfx_preview.py <file.32> <out.png>", file=sys.stderr)
        raise SystemExit(1)
    p = montage_from_sheet(Path(sys.argv[1]), Path(sys.argv[2]))
    print(f"wrote {p}")
