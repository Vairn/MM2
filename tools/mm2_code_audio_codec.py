#!/usr/bin/env python3
"""Codec for code-driven MM2 audio tables in mm2_data_00.bin."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import List


A4_BASE = 0x7FFE


def a4_off(neg_disp: int) -> int:
    return A4_BASE - neg_disp


def be_u16(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 2], "big")


def be_u32(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 4], "big")


def be_store_u16(buf: bytearray, off: int, val: int) -> None:
    buf[off : off + 2] = int(val & 0xFFFF).to_bytes(2, "big")


def be_store_u32(buf: bytearray, off: int, val: int) -> None:
    buf[off : off + 4] = int(val & 0xFFFFFFFF).to_bytes(4, "big")


@dataclass
class Mm2CodeAudioTables:
    seq_73de: List[int]
    seq_73d5: List[int]
    seq_73a8: List[int]
    seq_7388: List[int]
    seq_734a: List[int]
    seq_7338: List[int]
    seq_7326: List[int]
    seq_730c: List[int]


def decode_from_data_hunk(data_hunk: bytes) -> Mm2CodeAudioTables:
    if len(data_hunk) < a4_off(0x730C) + 8 * 4:
        raise ValueError("data hunk too small for code-audio tables")

    return Mm2CodeAudioTables(
        seq_73de=list(data_hunk[a4_off(0x73DE) : a4_off(0x73DE) + 10]),
        seq_73d5=list(data_hunk[a4_off(0x73D5) : a4_off(0x73D5) + 10]),
        seq_73a8=[be_u16(data_hunk, a4_off(0x73A8) + i * 2) for i in range(16)],
        seq_7388=[be_u16(data_hunk, a4_off(0x7388) + i * 2) for i in range(16)],
        seq_734a=[be_u16(data_hunk, a4_off(0x734A) + i * 2) for i in range(16)],
        seq_7338=[be_u16(data_hunk, a4_off(0x7338) + i * 2) for i in range(16)],
        seq_7326=[be_u16(data_hunk, a4_off(0x7326) + i * 2) for i in range(9)],
        seq_730c=[be_u32(data_hunk, a4_off(0x730C) + i * 4) for i in range(8)],
    )


def encode_into_data_hunk(tables: Mm2CodeAudioTables, data_hunk: bytes) -> bytes:
    out = bytearray(data_hunk)
    if len(out) < a4_off(0x730C) + 8 * 4:
        raise ValueError("data hunk too small for code-audio tables")

    out[a4_off(0x73DE) : a4_off(0x73DE) + 10] = bytes(tables.seq_73de[:10])
    out[a4_off(0x73D5) : a4_off(0x73D5) + 10] = bytes(tables.seq_73d5[:10])

    for i in range(16):
        be_store_u16(out, a4_off(0x73A8) + i * 2, tables.seq_73a8[i])
        be_store_u16(out, a4_off(0x7388) + i * 2, tables.seq_7388[i])
        be_store_u16(out, a4_off(0x734A) + i * 2, tables.seq_734a[i])
        be_store_u16(out, a4_off(0x7338) + i * 2, tables.seq_7338[i])
    for i in range(9):
        be_store_u16(out, a4_off(0x7326) + i * 2, tables.seq_7326[i])
    for i in range(8):
        be_store_u32(out, a4_off(0x730C) + i * 4, tables.seq_730c[i])

    return bytes(out)


def load_data_hunk_file(path: str | Path) -> Mm2CodeAudioTables:
    p = Path(path)
    return decode_from_data_hunk(p.read_bytes())


def save_data_hunk_file(path: str | Path, tables: Mm2CodeAudioTables) -> None:
    p = Path(path)
    p.write_bytes(encode_into_data_hunk(tables, p.read_bytes()))

