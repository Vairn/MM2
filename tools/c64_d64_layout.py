"""Shared D64 sector layout helpers for C64 MM2 RE tools."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Standard 1541 35-track layout
SPT = [21] * 17 + [19] * 7 + [18] * 6 + [17] * 5

INTERLEAVE: dict[range, list[int]] = {
    range(1, 18): [0, 11, 2, 9, 4, 7, 6, 1, 10, 3, 8, 5, 13, 14, 15, 16, 17, 18, 19, 20, 12],
    range(18, 25): [0, 6, 12, 18, 4, 10, 16, 2, 8, 14, 1, 7, 13, 3, 9, 15, 5, 11, 17],
    range(25, 31): [0, 6, 12, 18, 3, 9, 15, 1, 7, 13, 2, 8, 14, 4, 10, 16, 5, 11],
    range(31, 36): [0, 6, 12, 3, 9, 15, 1, 7, 13, 2, 8, 14, 4, 10, 16, 5, 11],
}

# Minimal NMOS 6502 opcode set (same as disasm_6502.py)
KNOWN_OPS = frozenset(
    [
        0x00, 0x01, 0x05, 0x06, 0x08, 0x09, 0x0A, 0x0D, 0x10, 0x18, 0x20, 0x21, 0x24, 0x25,
        0x29, 0x2C, 0x30, 0x38, 0x4A, 0x4C, 0x4D, 0x50, 0x60, 0x65, 0x68, 0x69, 0x6C, 0x6D,
        0x70, 0x84, 0x85, 0x86, 0x88, 0x8A, 0x8C, 0x8D, 0x8E, 0x90, 0x91, 0x94, 0x95, 0x99,
        0x9A, 0xA0, 0xA2, 0xA4, 0xA5, 0xA6, 0xA8, 0xA9, 0xAA, 0xAC, 0xAD, 0xAE, 0xB0, 0xB1,
        0xB5, 0xB9, 0xBA, 0xBD, 0xC0, 0xC5, 0xC6, 0xC8, 0xC9, 0xCA, 0xCD, 0xCE, 0xD0, 0xD8,
        0xE0, 0xE5, 0xE6, 0xE8, 0xEA, 0xEE, 0xF0,
    ]
)

LOADER_ORG = 0x0400  # T18S1 IEC routine static disasm base (confirmed)

IEC_PATTERNS = {
    "sta_dd00": bytes([0x8D, 0x00, 0xDD]),
    "lda_dd00": bytes([0xAD, 0x00, 0xDD]),
    "eor_dd00": bytes([0x4D, 0x00, 0xDD]),
    "stx_dd00": bytes([0x8E, 0x00, 0xDD]),
}


def interleave_table(track: int) -> list[int]:
    for rng, table in INTERLEAVE.items():
        if track in rng:
            return table
    raise ValueError(f"invalid track {track}")


def track_base(track: int) -> int:
    return sum(SPT[i - 1] * 256 for i in range(1, track))


def sector_offset(track: int, sector: int) -> int:
    il = interleave_table(track)[: SPT[track - 1]]
    if sector >= len(il):
        raise ValueError(f"sector {sector} out of range for track {track} ({len(il)} sectors)")
    return track_base(track) + il[sector] * 256


def read_sector(data: bytes, track: int, sector: int) -> bytes:
    off = sector_offset(track, sector)
    return data[off : off + 256]


def sector_payload(data: bytes, track: int, sector: int) -> bytes:
    return read_sector(data, track, sector)[2:]


def offset_to_track_sector(data_len: int, offset: int) -> tuple[int, int] | None:
    """Map flat D64 byte offset to (track, sector) if it falls in sector payload."""
    for track in range(1, 36):
        for sector in range(SPT[track - 1]):
            off = sector_offset(track, sector)
            if off + 2 <= offset < off + 256:
                return track, sector
    return None


def classify_payload(payload: bytes) -> str:
    """Heuristic sector class: empty, code, text, data."""
    if not payload or all(b == 0 for b in payload):
        return "empty"
    ascii_ratio = sum(32 <= b < 127 for b in payload) / len(payload)
    op_ratio = sum(b in KNOWN_OPS for b in payload) / len(payload)
    iec_hits = sum(payload.count(p) for p in IEC_PATTERNS.values())
    if iec_hits >= 2 and op_ratio >= 0.35:
        return "iec_code"
    if op_ratio >= 0.45 and ascii_ratio < 0.25:
        return "code"
    if ascii_ratio >= 0.55:
        return "text"
    if ascii_ratio >= 0.35 and op_ratio < 0.30:
        return "text_mixed"
    return "data"
