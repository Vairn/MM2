#!/usr/bin/env python3
"""Round-trip and golden tests for mm2_event_lang."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from decode_event import encode_event_dat, load_event_records  # noqa: E402
from mm2_event_lang.decompile import decompile_file, decompile_location  # noqa: E402
from mm2_event_lang.dsl_emit import emit_location  # noqa: E402
from mm2_event_lang.dsl_parser import parse_location  # noqa: E402
from mm2_event_lang.encode import encode_file, encode_location  # noqa: E402

EVENT_DAT = ROOT / "event.dat"


class TestEventLangRoundtrip(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not EVENT_DAT.is_file():
            raise unittest.SkipTest("event.dat not found")

    def test_all_records_byte_identical(self) -> None:
        data = EVENT_DAT.read_bytes()
        header, records = load_event_records(data)
        for i, rec in enumerate(records):
            loc = decompile_location(rec, i)
            rebuilt = encode_location(loc)
            self.assertEqual(rec, rebuilt, f"location {i}")

    def test_full_file_roundtrip(self) -> None:
        data = EVENT_DAT.read_bytes()
        event = decompile_file(EVENT_DAT)
        self.assertEqual(encode_file(event), data)

    def test_loc34_juror_readable(self) -> None:
        _, records = load_event_records(EVENT_DAT.read_bytes())
        loc = decompile_location(records[34], 34)
        text = emit_location(loc)
        self.assertIn("selector 0x97", text)
        self.assertIn("facing ns", text)
        self.assertNotIn("skip_tokens", text)

    def test_loc53_access_code_if_else(self) -> None:
        _, records = load_event_records(EVENT_DAT.read_bytes())
        loc = decompile_location(records[53], 53)
        text = emit_location(loc)
        self.assertIn('if answer == "46":', text)
        self.assertIn("@apply_party_masked count=0x00 set=0x75 and=0xFB or=0x04", text)
        self.assertIn('# "Correct!"', text)
        self.assertIn('# "Incorrect!"', text)
        self.assertNotIn("quest_flag", text)
        self.assertNotIn("skip_tokens", text)

    def test_loc0_fountain_store_var8(self) -> None:
        _, records = load_event_records(EVENT_DAT.read_bytes())
        loc = decompile_location(records[0], 0)
        sc = next(s for s in loc.scripts if s.event_id == 42)
        import mm2_event_lang.dsl_emit as emit_mod

        lines = []
        for j, stmt in enumerate(sc.body):
            nxt = sc.body[j + 1] if j + 1 < len(sc.body) else None
            lines.extend(emit_mod._format_stmt(stmt, 1, nxt))
        self.assertIn("if yes_no:", "\n".join(lines))
        self.assertIn("store_var8 eagle_eye value=0x32", "\n".join(lines))
        self.assertIn("store_var8 wizard_eye value=0x32", "\n".join(lines))
        self.assertNotIn("@op 0x1A", "\n".join(lines))

    def test_loc0_op0b_op0e_mechanical(self) -> None:
        _, records = load_event_records(EVENT_DAT.read_bytes())
        loc = decompile_location(records[0], 0)
        text = emit_location(loc)
        sc = next(s for s in loc.scripts if s.event_id == 19)
        import mm2_event_lang.dsl_emit as emit_mod

        lines = []
        for j, stmt in enumerate(sc.body):
            nxt = sc.body[j + 1] if j + 1 < len(sc.body) else None
            lines.extend(emit_mod._format_stmt(stmt, 1, nxt))
        self.assertEqual(
            lines,
            ["  service_title s20 mode=0", "  selector 0x0D  # enroll_mages_guild"],
        )
        self.assertIn('s20: "Fool, you have no farthing to flick!"', text)
        self.assertNotIn("say_service", text)
        self.assertNotIn("shop ", text)

    def test_dsl_parse_compile_preserves_modified(self) -> None:
        _, records = load_event_records(EVENT_DAT.read_bytes())
        loc = decompile_location(records[0], 0)
        dsl = emit_location(loc)
        parsed = parse_location(dsl)
        self.assertTrue(parsed.modified)
        self.assertEqual(parsed.id, 0)

    def test_decompile_to_dir(self) -> None:
        from mm2_event_lang.file_codec import decompile_to_dir

        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            decompile_to_dir(EVENT_DAT, out)
            files = list(out.glob("loc_*.mm2evt"))
            self.assertEqual(len(files), 71)
            self.assertTrue((out / "manifest.yaml").is_file())


if __name__ == "__main__":
    unittest.main()
