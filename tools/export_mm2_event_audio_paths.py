#!/usr/bin/env python3
"""Export per-event MM2 audio call paths from capstone disassembly."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List


ROOT = Path(__file__).resolve().parents[1]
CAPSTONE = ROOT / "EXTRACTED" / "mm2.capstone.asm"
THUNK_MAP_TXT = ROOT / "EXTRACTED" / "tmp_mm2_thunk_map.txt"
OUT_DIR = ROOT / "EXTRACTED" / "audio-events"
OUT_ALL = ROOT / "EXTRACTED" / "mm2_event_audio_paths.json"

LINE_RE = re.compile(
    r"^(?P<addr>[0-9a-fA-F]{6})\s+(?P<opbytes>[0-9a-fA-F]+)\s+(?P<asm>.+)$"
)
JSR_A4_RE = re.compile(r"jsr\s+-\$(?P<disp>[0-9a-fA-F]{4})\(a4\)", re.IGNORECASE)
THUNK_LINE_RE = re.compile(
    r"^\s*-\s+-\$(?P<disp>[0-9A-Fa-f]{4})\(A4\).+target=(?P<target>0x[0-9A-Fa-f]+)"
)
IMM_PUSH_RE = re.compile(r"move\.w\s+#\$(?P<imm>[0-9a-fA-F]+),\s*-\(a7\)", re.IGNORECASE)
CLEAR_PUSH_RE = re.compile(r"clr\.w\s+-\(a7\)", re.IGNORECASE)
CMPI_W_IMM_RE = re.compile(r"cmpi\.w\s+#\$(?P<imm>[0-9a-fA-F]+),", re.IGNORECASE)
BLT_B_RE = re.compile(r"blt\.b\s+\$(?P<target>[0-9a-fA-F]+)", re.IGNORECASE)

SLOT_TARGETS = {
    "7C62": "0x218EA",
    "7C44": "0x21D80",
    "7BAE": "0x22C24",
    "7BA8": "0x22D68",
    "7B9C": "0x22EAA",
}


def is_audio_slot_disp(disp_hex: str) -> bool:
    disp = int(disp_hex, 16)
    if 0x7B00 <= disp <= 0x7CFF:
        return True
    # Party death/defeat entry uses -$7E96 in this build.
    return disp == 0x7E96


@dataclass
class Row:
    addr: int
    opbytes: str
    asm: str


def parse_rows() -> List[Row]:
    rows: List[Row] = []
    for ln in CAPSTONE.read_text(encoding="utf-8", errors="replace").splitlines():
        m = LINE_RE.match(ln)
        if not m:
            continue
        rows.append(
            Row(
                addr=int(m.group("addr"), 16),
                opbytes=m.group("opbytes"),
                asm=m.group("asm").strip(),
            )
        )
    return rows


def load_slot_targets() -> Dict[str, str]:
    out = {k: v for k, v in SLOT_TARGETS.items()}
    if not THUNK_MAP_TXT.exists():
        return out
    for ln in THUNK_MAP_TXT.read_text(encoding="utf-8", errors="replace").splitlines():
        m = THUNK_LINE_RE.match(ln)
        if not m:
            continue
        out[m.group("disp").upper()] = m.group("target")
    return out


def capture_context(rows: List[Row], pos: int, back: int = 24) -> List[str]:
    out: List[str] = []
    start = max(0, pos - back)
    for i in range(pos - 1, start - 1, -1):
        asm = rows[i].asm.lower()
        if asm.startswith("jsr ") or asm.startswith("bsr "):
            break
        if "-(a7)" in asm or "pea." in asm:
            out.append(f"0x{rows[i].addr:06X} {rows[i].asm}")
        elif out:
            # Stop when we have started collecting and hit a non-push line.
            break
    out.reverse()
    return out


def decode_literal_stack_args(context_pushes: List[str]) -> List[object]:
    args: List[object] = []
    for row in context_pushes:
        asm = row.split(" ", 1)[1] if " " in row else row
        m_imm = IMM_PUSH_RE.search(asm)
        if m_imm:
            args.append(int(m_imm.group("imm"), 16))
            continue
        if CLEAR_PUSH_RE.search(asm):
            args.append(0)
            continue
        args.append(f"expr:{asm}")
    return args


def detect_runtime_loop_repeats(rows: List[Row], call_pos: int) -> int | None:
    """
    Detect simple counted loops around a call:
      addq ... counter
      cmpi.w #$NN, counter
      blt.b $back
    If the backward branch covers the call, return NN as estimated loop repeats.
    """
    call_addr = rows[call_pos].addr
    window = rows[call_pos : min(call_pos + 12, len(rows))]
    cmp_limit: int | None = None
    for r in window:
        m = CMPI_W_IMM_RE.search(r.asm)
        if m:
            cmp_limit = int(m.group("imm"), 16)
        b = BLT_B_RE.search(r.asm)
        if b:
            target = int(b.group("target"), 16)
            if target <= call_addr and cmp_limit is not None:
                return cmp_limit
    return None


def extract_calls_in_ranges(
    rows: List[Row], ranges: List[tuple[int, int]], slot_targets: Dict[str, str]
) -> List[Dict[str, object]]:
    out: List[Dict[str, object]] = []
    for i, r in enumerate(rows):
        in_range = any(lo <= r.addr <= hi for lo, hi in ranges)
        if not in_range:
            continue
        m = JSR_A4_RE.search(r.asm)
        if not m:
            continue
        disp = m.group("disp").upper()
        if not is_audio_slot_disp(disp):
            continue
        context_pushes = capture_context(rows, i, back=24)
        runtime_repeats = detect_runtime_loop_repeats(rows, i)
        out.append(
            {
                "address": f"0x{r.addr:06X}",
                "call": r.asm,
                "slot": f"-$${disp}(A4)".replace("$$", "$"),
                "resolved_target": slot_targets.get(disp, "unknown"),
                "context_pushes": context_pushes,
                "literal_stack_args": decode_literal_stack_args(context_pushes),
                "runtime_loop_repeats_estimate": runtime_repeats,
            }
        )
    return out


def main() -> None:
    rows = parse_rows()
    slot_targets = load_slot_targets()

    events: Dict[str, Dict[str, object]] = {
        "title": {
            "ranges": [(0x000FC8, 0x001080), (0x001FF0, 0x002040)],
            "notes": "Blocking title song + title loop step + title chirp.",
        },
        "combat_enter": {
            "ranges": [(0x012E30, 0x012EF0), (0x0037E0, 0x003820)],
            "notes": "Encounter entry stings and immediate combat cue path.",
        },
        "combat_victory": {
            "ranges": [(0x012430, 0x0129FF)],
            "notes": "Victory handler and repeated jingle note calls.",
        },
        "party_death_or_defeat": {
            "ranges": [(0x009F20, 0x00A010), (0x011650, 0x0116D0), (0x007DCC, 0x007E60)],
            "notes": "Party death/defeat entrypoints and tone-scaling path.",
        },
    }

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    bundle: Dict[str, object] = {
        "source": str(CAPSTONE),
        "slot_targets": {f"-$${k}(A4)".replace("$$", "$"): v for k, v in slot_targets.items()},
        "events": {},
    }

    for name, meta in events.items():
        ranges = meta["ranges"]  # type: ignore[index]
        calls = extract_calls_in_ranges(rows, ranges, slot_targets)
        payload = {
            "event": name,
            "notes": meta["notes"],
            "ranges": [f"0x{lo:06X}-0x{hi:06X}" for lo, hi in ranges],  # type: ignore[arg-type]
            "calls": calls,
        }
        (OUT_DIR / f"{name}.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
        bundle["events"][name] = payload  # type: ignore[index]

    OUT_ALL.write_text(json.dumps(bundle, indent=2), encoding="utf-8")
    print(f"Wrote {OUT_ALL}")
    print(f"Wrote {OUT_DIR}")


if __name__ == "__main__":
    main()

