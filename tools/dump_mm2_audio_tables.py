#!/usr/bin/env python3
"""Dump MM2 in-exe audio-related tables from data/code hunks.

This extracts known table offsets anchored by A4=$7FFE from
EXTRACTED/hunks/mm2_data_00.bin and fixed code offsets from EXTRACTED/mm2.
"""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_HUNK = ROOT / "EXTRACTED" / "hunks" / "mm2_data_00.bin"
CODE_HUNK = ROOT / "EXTRACTED" / "hunks" / "mm2_code_00.bin"
OUT_JSON = ROOT / "EXTRACTED" / "tmp_mm2_audio_tables.json"
OUT_TXT = ROOT / "EXTRACTED" / "tmp_mm2_audio_tables.txt"

A4_BASE = 0x7FFE


def be_u16(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 2], "big")


def be_u32(buf: bytes, off: int) -> int:
    return int.from_bytes(buf[off : off + 4], "big")


def fmt_hex_seq(vals: list[int], width: int = 2) -> str:
    return " ".join(f"{v:0{width}X}" for v in vals)


def c_string(buf: bytes, off: int, max_len: int = 256) -> str:
    out = bytearray()
    i = off
    end = min(len(buf), off + max_len)
    while i < end:
        b = buf[i]
        if b == 0:
            break
        out.append(b)
        i += 1
    return out.decode("ascii", errors="replace")


def main() -> None:
    data = DATA_HUNK.read_bytes()
    code = CODE_HUNK.read_bytes()

    def a4_off(neg_disp: int) -> int:
        return A4_BASE - neg_disp

    # Tables referenced by audio call paths.
    off_762e = a4_off(0x762E)
    off_75ee = a4_off(0x75EE)
    off_73cc = a4_off(0x73CC)
    off_73c4 = a4_off(0x73C4)
    off_79de = a4_off(0x79DE)
    off_79ca = a4_off(0x79CA)
    off_796a = a4_off(0x796A)
    off_73de = a4_off(0x73DE)
    off_73d5 = a4_off(0x73D5)
    off_73a8 = a4_off(0x73A8)
    off_7388 = a4_off(0x7388)
    off_734a = a4_off(0x734A)
    off_7338 = a4_off(0x7338)
    off_7326 = a4_off(0x7326)
    off_730c = a4_off(0x730C)

    note_lut_762e = list(data[off_762e : off_762e + 0x40])  # 64 bytes
    step_mask_75ee = [be_u16(data, off_75ee + i * 2) for i in range(16)]
    flagmask_73cc = list(data[off_73cc : off_73cc + 8])
    script_ptr_73c4 = [be_u32(data, off_73c4 + i * 4) for i in range(7)]

    # These are runtime-populated by audio_init from master.32, but we also dump
    # current init defaults present in data hunk.
    pitch_a_79de = [be_u16(data, off_79de + i * 2) for i in range(10)]
    pitch_b_79ca = [be_u16(data, off_79ca + i * 2) for i in range(10)]
    note_map_796a = [be_u16(data, off_796a + i * 2) for i in range(8)]
    seq_73de_u8_10 = list(data[off_73de : off_73de + 10])
    seq_73d5_u8_10 = list(data[off_73d5 : off_73d5 + 10])
    seq_73a8_u16_16 = [be_u16(data, off_73a8 + i * 2) for i in range(16)]
    seq_7388_u16_16 = [be_u16(data, off_7388 + i * 2) for i in range(16)]
    seq_734a_u16_16 = [be_u16(data, off_734a + i * 2) for i in range(16)]
    seq_7338_u16_16 = [be_u16(data, off_7338 + i * 2) for i in range(16)]
    seq_7326_u16_9 = [be_u16(data, off_7326 + i * 2) for i in range(9)]
    seq_730c_u32_8 = [be_u32(data, off_730c + i * 4) for i in range(8)]

    # In-code dispatch offsets (F5BC block) and jump block around 0x173C4.
    f5bc_words = [be_u16(code, 0xF5BC + i * 2) for i in range(8)]
    jump_block_173c4 = list(code[0x173C4 : 0x173C4 + 0x80])
    script_ptr_73c4_strings = {f"0x{p:04X}": c_string(code, p) for p in script_ptr_73c4}
    title_loop_1ff6 = list(code[0x1FF6 : 0x2016])
    death_handler_7dcc = list(code[0x7DCC : 0x7F5B])
    victory_block_12430 = list(code[0x12430 : 0x12561])

    master_exists = any(
        p.exists()
        for p in (
            ROOT / "master.32",
            ROOT / "MM2BOOT" / "master.32",
            ROOT / "EXTRACTED" / "master.32",
        )
    )

    # A4 thunk slots used by audio calls in the disassembly.
    thunk_slots: dict[str, dict[str, object]] = {}
    thunk_disps = {
        "play_note_index": 0x7C62,
        "play_tone": 0x7C44,
        "play_song_step": 0x7BA8,
        "play_song": 0x7B9C,
        "note_core_or_dispatch": 0x7BAE,
        "aux_core_7ACA": 0x7ACA,
        "aux_core_7AE2": 0x7AE2,
    }
    for name, disp in thunk_disps.items():
        off = a4_off(disp)
        raw = list(data[off : off + 16])
        info: dict[str, object] = {
            "disp": f"-${disp:04X}(A4)",
            "offset": off,
            "raw_u8_16": raw,
        }
        if len(raw) >= 6:
            op = (raw[0] << 8) | raw[1]
            if op in (0x4EF9, 0x4EB9):  # JMP/JSR absolute long
                tgt = be_u32(data, off + 2)
                info["stub_op"] = f"0x{op:04X}"
                info["stub_target"] = tgt
                if 0 <= tgt < len(code):
                    info["target_code_u8_16"] = list(code[tgt : tgt + 16])
            else:
                info["stub_op"] = f"0x{op:04X}"
        thunk_slots[name] = info

    payload = {
        "a4_base": A4_BASE,
        "data_hunk": str(DATA_HUNK),
        "code_hunk": str(CODE_HUNK),
        "tables": {
            "note_lut_762e_u8_64": note_lut_762e,
            "step_mask_75ee_u16_16": step_mask_75ee,
            "flagmask_73cc_u8_8": flagmask_73cc,
            "script_ptr_73c4_u32_7": script_ptr_73c4,
            "pitch_a_79de_u16_10_default": pitch_a_79de,
            "pitch_b_79ca_u16_10_default": pitch_b_79ca,
            "note_map_796a_u16_8_default": note_map_796a,
            "seq_73de_u8_10": seq_73de_u8_10,
            "seq_73d5_u8_10": seq_73d5_u8_10,
            "seq_73a8_u16_16": seq_73a8_u16_16,
            "seq_7388_u16_16": seq_7388_u16_16,
            "seq_734a_u16_16": seq_734a_u16_16,
            "seq_7338_u16_16": seq_7338_u16_16,
            "seq_7326_u16_9": seq_7326_u16_9,
            "seq_730c_u32_8": seq_730c_u32_8,
            "dispatch_f5bc_u16_8": f5bc_words,
            "jump_block_173c4_u8_128": jump_block_173c4,
            "script_ptr_73c4_strings": script_ptr_73c4_strings,
            "title_loop_1ff6_u8": title_loop_1ff6,
            "death_handler_7dcc_u8": death_handler_7dcc,
            "victory_block_12430_u8": victory_block_12430,
            "thunk_slots": thunk_slots,
        },
        "workspace_master_32_present": master_exists,
        "notes": [
            "pitch_a_79de/pitch_b_79ca/note_map_796a are runtime-overwritten by audio_init from master.32",
            "script_ptr_73c4 values point to code offsets used by scripted playback paths",
            "note_lut_762e is the byte lookup used by song-step note decode (byte>>2 path)",
        ],
    }

    OUT_JSON.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    lines: list[str] = []
    lines.append(f"A4 base: 0x{A4_BASE:04X}")
    lines.append(f"Data hunk: {DATA_HUNK}")
    lines.append(f"Code hunk: {CODE_HUNK}")
    lines.append("")
    lines.append("[note_lut_762e_u8_64]")
    lines.append(fmt_hex_seq(note_lut_762e, 2))
    lines.append("")
    lines.append("[step_mask_75ee_u16_16]")
    lines.append(fmt_hex_seq(step_mask_75ee, 4))
    lines.append("")
    lines.append("[flagmask_73cc_u8_8]")
    lines.append(fmt_hex_seq(flagmask_73cc, 2))
    lines.append("")
    lines.append("[script_ptr_73c4_u32_7]")
    lines.append(fmt_hex_seq(script_ptr_73c4, 8))
    lines.append("")
    lines.append("[script_ptr_73c4_strings]")
    for p in script_ptr_73c4:
        lines.append(f"0x{p:04X}: {script_ptr_73c4_strings[f'0x{p:04X}']}")
    lines.append("")
    lines.append("[pitch_a_79de_u16_10_default]")
    lines.append(fmt_hex_seq(pitch_a_79de, 4))
    lines.append("")
    lines.append("[pitch_b_79ca_u16_10_default]")
    lines.append(fmt_hex_seq(pitch_b_79ca, 4))
    lines.append("")
    lines.append("[note_map_796a_u16_8_default]")
    lines.append(fmt_hex_seq(note_map_796a, 4))
    lines.append("")
    lines.append("[seq_73de_u8_10]")
    lines.append(fmt_hex_seq(seq_73de_u8_10, 2))
    lines.append("")
    lines.append("[seq_73d5_u8_10]")
    lines.append(fmt_hex_seq(seq_73d5_u8_10, 2))
    lines.append("")
    lines.append("[seq_73a8_u16_16]")
    lines.append(fmt_hex_seq(seq_73a8_u16_16, 4))
    lines.append("")
    lines.append("[seq_7388_u16_16]")
    lines.append(fmt_hex_seq(seq_7388_u16_16, 4))
    lines.append("")
    lines.append("[seq_734a_u16_16]")
    lines.append(fmt_hex_seq(seq_734a_u16_16, 4))
    lines.append("")
    lines.append("[seq_7338_u16_16]")
    lines.append(fmt_hex_seq(seq_7338_u16_16, 4))
    lines.append("")
    lines.append("[seq_7326_u16_9]")
    lines.append(fmt_hex_seq(seq_7326_u16_9, 4))
    lines.append("")
    lines.append("[seq_730c_u32_8]")
    lines.append(fmt_hex_seq(seq_730c_u32_8, 8))
    lines.append("")
    lines.append("[dispatch_f5bc_u16_8]")
    lines.append(fmt_hex_seq(f5bc_words, 4))
    lines.append("")
    lines.append("[jump_block_173c4_u8_128]")
    lines.append(fmt_hex_seq(jump_block_173c4, 2))
    lines.append("")
    lines.append("[title_loop_1ff6_u8]")
    lines.append(fmt_hex_seq(title_loop_1ff6, 2))
    lines.append("")
    lines.append("[death_handler_7dcc_u8]")
    lines.append(fmt_hex_seq(death_handler_7dcc, 2))
    lines.append("")
    lines.append("[victory_block_12430_u8]")
    lines.append(fmt_hex_seq(victory_block_12430, 2))
    lines.append("")
    lines.append("[thunk_slots]")
    for k, v in thunk_slots.items():
        lines.append(
            f"{k}: {v['disp']} off=0x{int(v['offset']):04X} op={v.get('stub_op', 'n/a')}"
        )
        lines.append(f"  raw: {fmt_hex_seq(list(v['raw_u8_16']), 2)}")
        if "stub_target" in v:
            lines.append(f"  target: 0x{int(v['stub_target']):04X}")
            tgt = v.get("target_code_u8_16")
            if isinstance(tgt, list):
                lines.append(f"  target_code: {fmt_hex_seq(tgt, 2)}")
    lines.append("")
    lines.append("Notes:")
    lines.append(f"- workspace master.32 present: {master_exists}")
    lines.append("- pitch_* and note_map defaults are overwritten by audio_init from master.32.")
    lines.append("- script_ptr_73c4 entries are code offsets used by scripted song/text playback.")
    lines.append("- note_lut_762e is the song-step note lookup table used by byte>>2 decode.")
    OUT_TXT.write_text("\n".join(lines), encoding="utf-8")

    print(f"Wrote {OUT_JSON}")
    print(f"Wrote {OUT_TXT}")


if __name__ == "__main__":
    main()
