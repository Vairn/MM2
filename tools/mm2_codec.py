#!/usr/bin/env python3
"""MM2 codec tool for items.dat and str.dat.

Usage:
  python tools/mm2_codec.py items-decode <items.dat> <items.csv>
  python tools/mm2_codec.py items-encode <items.csv> <items.dat>
  python tools/mm2_codec.py str-decode <str.dat> <text.txt>
  python tools/mm2_codec.py str-encode <text.txt> <str.dat>
"""

from __future__ import annotations

import csv
import struct
import sys
from pathlib import Path

ITEM_RECORD_SIZE = 20
ITEM_NAME_SIZE = 12

ITEM_HEADER = [
    "index",
    "name",
    "separator",
    "forbidden_classes",
    "bonus_type",
    "bonus_amount",
    "effect_type",
    "effect_amount",
    "damage",
    "pad",
    "gold",
]


def _usage() -> int:
    print(__doc__.strip())
    return 1


def items_decode(in_path: Path, out_path: Path) -> None:
    data = in_path.read_bytes()
    if len(data) % ITEM_RECORD_SIZE != 0:
        raise ValueError(
            f"items size {len(data)} is not a multiple of {ITEM_RECORD_SIZE}"
        )

    rows = []
    record_count = len(data) // ITEM_RECORD_SIZE
    for idx in range(record_count):
        off = idx * ITEM_RECORD_SIZE
        rec = data[off : off + ITEM_RECORD_SIZE]
        name = rec[:ITEM_NAME_SIZE].decode("ascii", errors="strict").rstrip(" ")
        sep = rec[12]
        forbidden_classes = rec[13]  # set bit = class CANNOT use
        bonus = rec[14]
        effect = rec[15]
        damage = rec[16]
        pad = rec[17]
        gold = struct.unpack("<H", rec[18:20])[0]
        rows.append(
            {
                "index": idx,
                "name": name,
                "separator": sep,
                "forbidden_classes": forbidden_classes,
                "bonus_type": (bonus >> 4) & 0x0F,
                "bonus_amount": bonus & 0x0F,
                "effect_type": (effect >> 4) & 0x0F,
                "effect_amount": effect & 0x0F,
                "damage": damage,
                "pad": pad,
                "gold": gold,
            }
        )

    with out_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=ITEM_HEADER)
        writer.writeheader()
        writer.writerows(rows)


def _u8(row: dict[str, str], key: str) -> int:
    v = int(row[key], 0)
    if not (0 <= v <= 0xFF):
        raise ValueError(f"{key} out of u8 range: {v}")
    return v


def _u16(row: dict[str, str], key: str) -> int:
    v = int(row[key], 0)
    if not (0 <= v <= 0xFFFF):
        raise ValueError(f"{key} out of u16 range: {v}")
    return v


def items_encode(in_path: Path, out_path: Path) -> None:
    out = bytearray()
    with in_path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        missing = [k for k in ITEM_HEADER if k not in (reader.fieldnames or [])]
        if missing:
            raise ValueError(f"missing CSV columns: {', '.join(missing)}")

        expected_index = 0
        for row in reader:
            idx = int(row["index"], 0)
            if idx != expected_index:
                raise ValueError(
                    f"row index mismatch: expected {expected_index}, got {idx}"
                )
            expected_index += 1

            name = row["name"]
            name_bytes = name.encode("ascii", errors="strict")
            if len(name_bytes) > ITEM_NAME_SIZE:
                raise ValueError(f"name too long ({len(name_bytes)}): {name!r}")
            name_field = name_bytes.ljust(ITEM_NAME_SIZE, b" ")

            sep = _u8(row, "separator")
            forbidden_classes = _u8(row, "forbidden_classes")
            bonus_type = _u8(row, "bonus_type")
            bonus_amount = _u8(row, "bonus_amount")
            effect_type = _u8(row, "effect_type")
            effect_amount = _u8(row, "effect_amount")
            if bonus_type > 0x0F or bonus_amount > 0x0F:
                raise ValueError("bonus_type/bonus_amount must be 0..15")
            if effect_type > 0x0F or effect_amount > 0x0F:
                raise ValueError("effect_type/effect_amount must be 0..15")

            damage = _u8(row, "damage")
            pad = _u8(row, "pad")
            gold = _u16(row, "gold")

            bonus = (bonus_type << 4) | bonus_amount
            effect = (effect_type << 4) | effect_amount

            rec = bytearray()
            rec.extend(name_field)
            rec.append(sep)
            rec.append(forbidden_classes)
            rec.append(bonus)
            rec.append(effect)
            rec.append(damage)
            rec.append(pad)
            rec.extend(struct.pack("<H", gold))
            if len(rec) != ITEM_RECORD_SIZE:
                raise AssertionError("bad record size")
            out.extend(rec)

    out_path.write_bytes(out)


def str_decode(in_path: Path, out_path: Path) -> None:
    src = in_path.read_bytes()
    decoded = bytearray()
    for b in src:
        if b == 0x01:
            decoded.append(0x0A)
        else:
            decoded.append((b + 0x1C) & 0xFF)
    out_path.write_bytes(decoded)


def str_encode(in_path: Path, out_path: Path) -> None:
    src = in_path.read_bytes()
    encoded = bytearray()
    for b in src:
        if b == 0x0A:
            encoded.append(0x01)
        elif b == 0x0D:
            # Ignore CR from CRLF input; LF is mapped above.
            continue
        else:
            encoded.append((b - 0x1C) & 0xFF)
    out_path.write_bytes(encoded)


def main(argv: list[str]) -> int:
    if len(argv) != 4:
        return _usage()

    cmd, src_s, dst_s = argv[1], argv[2], argv[3]
    src = Path(src_s)
    dst = Path(dst_s)

    if cmd == "items-decode":
        items_decode(src, dst)
    elif cmd == "items-encode":
        items_encode(src, dst)
    elif cmd == "str-decode":
        str_decode(src, dst)
    elif cmd == "str-encode":
        str_encode(src, dst)
    else:
        return _usage()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
