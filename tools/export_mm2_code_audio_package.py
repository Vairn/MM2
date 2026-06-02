#!/usr/bin/env python3
"""Export a consolidated code-audio package from MM2 hunks."""

from __future__ import annotations

import json
from pathlib import Path

from mm2_code_audio_codec import load_data_hunk_file


ROOT = Path(__file__).resolve().parents[1]
DATA_HUNK = ROOT / "EXTRACTED" / "hunks" / "mm2_data_00.bin"
THUNK_MAP = ROOT / "EXTRACTED" / "tmp_mm2_thunk_map.txt"
DISPATCH_MAP = ROOT / "EXTRACTED" / "tmp_mm2_audio_dispatch.txt"
CODE_SEQS = ROOT / "EXTRACTED" / "tmp_mm2_code_sequences.txt"
OUT_JSON = ROOT / "EXTRACTED" / "mm2_code_audio_package.json"


def main() -> None:
    t = load_data_hunk_file(DATA_HUNK)
    payload = {
        "source": {
            "data_hunk": str(DATA_HUNK),
            "thunk_map": str(THUNK_MAP),
            "dispatch_map": str(DISPATCH_MAP),
            "code_sequences": str(CODE_SEQS),
        },
        "tables": {
            "seq_73de_u8_10": t.seq_73de,
            "seq_73d5_u8_10": t.seq_73d5,
            "seq_73a8_u16_16": t.seq_73a8,
            "seq_7388_u16_16": t.seq_7388,
            "seq_734a_u16_16": t.seq_734a,
            "seq_7338_u16_16": t.seq_7338,
            "seq_7326_u16_9": t.seq_7326,
            "seq_730c_u32_8": t.seq_730c,
        },
    }
    OUT_JSON.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"Wrote {OUT_JSON}")


if __name__ == "__main__":
    main()

