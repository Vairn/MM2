#!/usr/bin/env python3
"""Tests for pc_dat_lzw.py.

Two tiers:
  - Synthetic fixture tests (always run): build an in-memory LZW blob with
    `compress_pc_dat_flat` and check `decompress_pc_dat` round-trips it, and
    that plain (already-decompressed) data is never mistaken for compressed.
  - Real-GOG tests (skipped if no local install is found): decompress the
    actual GOG ATTRIB.DAT/MONSTERS.DAT/MAP.DAT/EVENTSI+EVENTSO.DAT/ITEMS.DAT
    and diff against the repo-root Amiga plain files.
"""
from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from pc_dat_lzw import (  # noqa: E402
    compress_pc_dat_flat,
    decompress_event_dat_pair,
    decompress_map_dat,
    decompress_pc_dat,
    is_pc_event_dat_half,
    is_pc_lzw_compressed,
    is_pc_map_dat,
)

GOG_DEFAULT = Path(r"C:\Program Files (x86)\GOG Galaxy\Games\Might and Magic 2")


def _find_gog() -> Path | None:
    if GOG_DEFAULT.is_dir() and (GOG_DEFAULT / "ATTRIB.DAT").is_file():
        return GOG_DEFAULT
    return None


class TestSyntheticFixture(unittest.TestCase):
    def test_flat_roundtrip(self) -> None:
        plaintext = (b"ATTRIB-LIKE-FIXTURE-DATA-" * 50)[:1200]
        packed = compress_pc_dat_flat(plaintext)
        self.assertTrue(is_pc_lzw_compressed(packed))
        self.assertEqual(decompress_pc_dat(packed), plaintext)

    def test_flat_roundtrip_tiny(self) -> None:
        # Zero-length input is intentionally excluded: dest_size == 0 is
        # treated as "not a compressed container" by design (real dat/gfx
        # blobs are never empty), so it isn't a meaningful round-trip case.
        for plaintext in (b"\x00", b"AB", b"X" * 3):
            packed = compress_pc_dat_flat(plaintext)
            self.assertEqual(decompress_pc_dat(packed), plaintext)

    def test_plain_data_not_flagged_compressed(self) -> None:
        # A plausible-looking but non-LZW blob shouldn't false-positive: pick
        # bytes whose leading u32 is huge (mirrors real Amiga .dat headers).
        plain = struct.pack("<I", 0xDEADBEEF) + b"\x01\x02\x03" * 100
        self.assertFalse(is_pc_lzw_compressed(plain))
        self.assertEqual(decompress_pc_dat(plain), plain)

    def test_amiga_plain_dat_files_not_flagged(self) -> None:
        for name in ("attrib.dat", "monsters.dat", "items.dat", "map.dat",
                      "event.dat", "roster.dat", "spells.dat", "str.dat"):
            path = ROOT / name
            if not path.is_file():
                continue
            data = path.read_bytes()
            with self.subTest(name=name):
                self.assertFalse(is_pc_lzw_compressed(data))
                self.assertFalse(is_pc_map_dat(data))
                self.assertFalse(is_pc_event_dat_half(data))
                self.assertEqual(decompress_pc_dat(data), data)


class TestRealGogInstall(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.gog = _find_gog()
        if cls.gog is None:
            raise unittest.SkipTest("No local GOG install found (optional real-data test)")
        cls.root = ROOT

    def test_attrib_byte_identical(self) -> None:
        amiga = (self.root / "attrib.dat").read_bytes()
        pc = (self.gog / "ATTRIB.DAT").read_bytes()
        self.assertTrue(is_pc_lzw_compressed(pc))
        self.assertEqual(decompress_pc_dat(pc), amiga)

    def test_monsters_byte_identical(self) -> None:
        amiga = (self.root / "monsters.dat").read_bytes()
        pc = (self.gog / "MONSTERS.DAT").read_bytes()
        self.assertTrue(is_pc_lzw_compressed(pc))
        self.assertEqual(decompress_pc_dat(pc), amiga)

    def test_items_already_plain(self) -> None:
        amiga = (self.root / "items.dat").read_bytes()
        pc = (self.gog / "ITEMS.DAT").read_bytes()
        self.assertFalse(is_pc_lzw_compressed(pc))
        self.assertEqual(pc, amiga)

    def test_map_reassembles_byte_identical(self) -> None:
        amiga = (self.root / "map.dat").read_bytes()
        pc = (self.gog / "MAP.DAT").read_bytes()
        self.assertTrue(is_pc_map_dat(pc))
        self.assertEqual(decompress_map_dat(pc), amiga)

    def test_event_merge_close_to_amiga(self) -> None:
        amiga = (self.root / "event.dat").read_bytes()
        indoor = (self.gog / "EVENTSI.DAT").read_bytes()
        outdoor = (self.gog / "EVENTSO.DAT").read_bytes()
        merged = decompress_event_dat_pair(indoor, outdoor)
        self.assertEqual(len(merged), len(amiga))
        diffs = sum(1 for a, b in zip(amiga, merged) if a != b)
        # 18 known single-byte platform content differences (see module
        # docstring); a large diff count would indicate a real decode bug.
        self.assertEqual(diffs, 18)


if __name__ == "__main__":
    unittest.main()
