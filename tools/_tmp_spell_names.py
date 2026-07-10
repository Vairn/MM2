#!/usr/bin/env python3
"""Map flat spell index -> stub by finding name strings before handlers."""
from __future__ import annotations

import struct
from pathlib import Path
import importlib.util

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p", HERE / "_trace_spell_picker.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
mem = m.build_full_code_image()

# Find C-strings in spell handler region and nearest following link.w
region = mem[0xB000:0xC800]
base = 0xB000
i = 0
while i < len(region) - 4:
    if 0x20 <= region[i] < 0x7F and region[i] not in (0x4E,):
        j = i
        while j < len(region) and 0x20 <= region[j] < 0x7F:
            j += 1
        if j - i >= 4 and (j >= len(region) or region[j] == 0):
            s = region[i:j].decode("ascii")
            # find next 4E55 (link) after string
            k = j
            while k + 2 < len(region) and k < j + 64:
                if region[k] == 0x4E and region[k + 1] == 0x55:
                    stub = base + k
                    print(f"{stub:#06x}  {s!r}")
                    break
                k += 1
            i = j + 1
            continue
    i += 1
