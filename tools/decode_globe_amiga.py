#!/usr/bin/env python3
"""
Decode/encode the Amiga XOR-obfuscated title blob used by MM2 startup code.

ASM anchor:
  - XOR loop: EXTRACTED/mm2.capstone.asm @ 0x2613E (and duplicate @ 0x263A8)
  - XOR length: 0x0BE1 bytes
  - Key length: 5 bytes, fetched from A4-$7675..-$7671
  - NUL string table counts walked after decode: 14,18,7,15,17,10,23

This tool intentionally keeps operations simple and reversible:
  - load/save raw blob bytes
  - decode (XOR first N bytes)
  - encode (XOR first N bytes back)
  - parse the seven NUL-terminated string tables
"""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import List

XOR_LEN_DEFAULT = 0x0BE1
KEY_LEN = 5
TABLE_COUNTS = [14, 18, 7, 15, 17, 10, 23]


@dataclass
class ParsedTables:
    xor_len: int
    key_hex: str
    table_counts: List[int]
    tables: List[List[str]]
    parse_end_offset: int


def load_blob(path: Path) -> bytes:
    return path.read_bytes()


def save_blob(path: Path, blob: bytes) -> None:
    path.write_bytes(blob)


def parse_key_hex(key_hex: str) -> bytes:
    key = bytes.fromhex(key_hex)
    if len(key) != KEY_LEN:
        raise ValueError(f"key must be {KEY_LEN} bytes ({KEY_LEN * 2} hex chars)")
    return key


def xor_prefix(data: bytes, key: bytes, xor_len: int) -> bytes:
    n = min(len(data), xor_len)
    out = bytearray(data)
    for i in range(n):
        out[i] ^= key[i % KEY_LEN]
    return bytes(out)


def decode_blob(obfuscated: bytes, key: bytes, xor_len: int = XOR_LEN_DEFAULT) -> bytes:
    return xor_prefix(obfuscated, key, xor_len)


def encode_blob(decoded: bytes, key: bytes, xor_len: int = XOR_LEN_DEFAULT) -> bytes:
    # XOR is symmetric.
    return xor_prefix(decoded, key, xor_len)


def parse_string_tables(decoded: bytes) -> ParsedTables:
    tables: List[List[str]] = []
    off = 0

    for count in TABLE_COUNTS:
        table: List[str] = []
        for _ in range(count):
            end = decoded.find(b"\x00", off)
            if end < 0:
                raise ValueError("unterminated string while parsing tables")
            raw = decoded[off:end]
            table.append(raw.decode("latin-1"))
            off = end + 1
        tables.append(table)

    return ParsedTables(
        xor_len=XOR_LEN_DEFAULT,
        key_hex="",
        table_counts=list(TABLE_COUNTS),
        tables=tables,
        parse_end_offset=off,
    )


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Decode/encode Amiga MM2 XOR title blob (ASM 0x2613E)."
    )
    ap.add_argument("input", help="Input blob path")
    ap.add_argument(
        "--key",
        default="f129ab5a07",
        help="5-byte XOR key in hex (default: f129ab5a07)",
    )
    ap.add_argument(
        "--xor-len",
        type=lambda v: int(v, 0),
        default=XOR_LEN_DEFAULT,
        help="XOR prefix length (default: 0x0BE1)",
    )
    ap.add_argument(
        "--decoded-out",
        help="Write decoded bytes to this path",
    )
    ap.add_argument(
        "--encoded-out",
        help="Write re-encoded bytes to this path (decode then encode round-trip)",
    )
    ap.add_argument(
        "--json-out",
        help="Write parsed table JSON to this path",
    )
    ap.add_argument(
        "--no-parse",
        action="store_true",
        help="Skip string-table parsing",
    )
    args = ap.parse_args()

    in_path = Path(args.input)
    key = parse_key_hex(args.key)

    raw = load_blob(in_path)
    decoded = decode_blob(raw, key=key, xor_len=args.xor_len)

    if args.decoded_out:
        save_blob(Path(args.decoded_out), decoded)

    parsed = None
    if not args.no_parse:
        parsed = parse_string_tables(decoded)
        parsed.key_hex = key.hex()
        parsed.xor_len = args.xor_len

    if args.encoded_out:
        reenc = encode_blob(decoded, key=key, xor_len=args.xor_len)
        save_blob(Path(args.encoded_out), reenc)

    print(f"input={in_path} bytes={len(raw)} xor_len=0x{args.xor_len:X} key={key.hex()}")
    if parsed is not None:
        print(
            "parsed_tables="
            + ",".join(str(x) for x in parsed.table_counts)
            + f" parse_end=0x{parsed.parse_end_offset:X}"
        )
        for i, tbl in enumerate(parsed.tables):
            preview = ", ".join(repr(s) for s in tbl[:3])
            if len(tbl) > 3:
                preview += ", ..."
            print(f"  table{i} count={len(tbl)} [{preview}]")

        if args.json_out:
            Path(args.json_out).write_text(
                json.dumps(asdict(parsed), indent=2),
                encoding="utf-8",
            )


if __name__ == "__main__":
    main()
