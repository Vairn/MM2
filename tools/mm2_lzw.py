#!/usr/bin/env python3
"""Canonical MM2 PC (DOS) LZW decompressor — traced to MM2.EXE @0x2A42.

Shared by `decode_pc_gfx.py` (the `.4`/`.16` graphics codec) and
`pc_dat_lzw.py` (the GOG `.DAT` file codec) — both wrap the exact same
9->12 bit LZW bitstream, so the implementation lives in one place.
"""
from __future__ import annotations

_LZW_MASK = {9: 0x1FF, 10: 0x3FF, 11: 0x7FF, 12: 0xFFF}


def lzw_decompress(source: bytes, dest_size: int) -> bytes:
    """MM2.EXE LZW decompressor (0x2A42): 9->12 bit codes, early-change
    dictionary growth, clear code 0x100, stop code 0x101."""
    dict_root = [0] * 4096
    dict_code = [0] * 4096
    bits_read = 0
    out = bytearray()

    def read_code(width: int) -> int | None:
        nonlocal bits_read
        base = bits_read // 8
        if base >= len(source):
            return None
        b0 = source[base]
        b1 = source[base + 1] if base + 1 < len(source) else 0
        b2 = source[base + 2] if base + 2 < len(source) else 0
        word = b0 | (b1 << 8) | (b2 << 16)
        word >>= bits_read % 8
        code = word & _LZW_MASK[width]
        bits_read += width
        return code

    def expand(code: int) -> list[int]:
        stack: list[int] = []
        while code > 0xFF:
            stack.append(dict_root[code])
            code = dict_code[code]
        stack.append(code)
        stack.reverse()
        return stack

    code_width = 9
    dict_limit = 0x200
    dict_next = 0x102
    prev: int | None = None

    while len(out) < dest_size:
        code = read_code(code_width)
        if code is None:
            break
        if code == 0x100:
            code_width = 9
            dict_limit = 0x200
            dict_next = 0x102
            code = read_code(code_width)
            if code is None:
                break
            out.append(code & 0xFF)
            prev = code
            continue
        if code == 0x101:
            break
        if prev is None:
            break
        # ASM 0x2C38-0x2C40: the dictionary link-walk ends at the ROOT char,
        # i.e. the *first* character of the string, which is expand()[0].
        if code < dict_next:
            chars = expand(code)
            first = chars[0]
        else:
            chars = expand(prev)
            first = chars[0]
            chars = chars + [first]
        out.extend(chars)
        if dict_next < 4096:
            dict_root[dict_next] = first
            dict_code[dict_next] = prev
            dict_next += 1
            if dict_next >= dict_limit and code_width < 12:
                code_width += 1
                dict_limit <<= 1
        prev = code

    return bytes(out[:dest_size])
