#!/usr/bin/env python3
"""Export deterministic MM2 audio intermediate JSON for MOD conversion scaffolding."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any, Dict, List


ROOT = Path(__file__).resolve().parents[1]
CAPSTONE_ASM = ROOT / "EXTRACTED" / "mm2.capstone.asm"
IRA_ASM = ROOT / "EXTRACTED" / "mm2.asm"
OUT_JSON = ROOT / "EXTRACTED" / "mm2_mod_audio_intermediate.json"

CAP_LINE_RE = re.compile(
    r"^(?P<addr>[0-9A-Fa-f]{6})\s+(?P<opbytes>[0-9A-Fa-f]+)\s+(?P<asm>.+)$"
)
IRA_LINE_RE = re.compile(
    r"^(?P<instr>.+?)\s*;\s*(?P<addr>[0-9A-Fa-f]{5}):\s*(?P<opbytes>[0-9A-Fa-f]+)\s*$"
)


def parse_capstone_lines() -> Dict[int, Dict[str, str]]:
    out: Dict[int, Dict[str, str]] = {}
    for line in CAPSTONE_ASM.read_text(encoding="utf-8", errors="replace").splitlines():
        m = CAP_LINE_RE.match(line)
        if not m:
            continue
        addr = int(m.group("addr"), 16)
        out[addr] = {
            "asm": m.group("asm").strip(),
            "opbytes": m.group("opbytes"),
            "address": f"0x{addr:06X}",
        }
    return out


def parse_ira_lines() -> Dict[int, Dict[str, str]]:
    out: Dict[int, Dict[str, str]] = {}
    for line in IRA_ASM.read_text(encoding="utf-8", errors="replace").splitlines():
        m = IRA_LINE_RE.match(line)
        if not m:
            continue
        addr = int(m.group("addr"), 16)
        out[addr] = {
            "instruction": re.sub(r"\s+", " ", m.group("instr").strip()),
            "opbytes": m.group("opbytes"),
            "address": f"0x{addr:05X}",
        }
    return out


def require_cap(
    cap: Dict[int, Dict[str, str]], addr: int, contains: str
) -> Dict[str, str]:
    row = cap.get(addr)
    if row is None:
        raise ValueError(f"Missing capstone address 0x{addr:06X}")
    if contains.lower() not in row["asm"].lower():
        raise ValueError(
            f"Capstone mismatch at 0x{addr:06X}: expected '{contains}', got '{row['asm']}'"
        )
    return row


def require_ira(
    ira: Dict[int, Dict[str, str]], addr: int, contains: str
) -> Dict[str, str]:
    row = ira.get(addr)
    if row is None:
        raise ValueError(f"Missing IRA address 0x{addr:05X}")
    if contains.lower() not in row["instruction"].lower():
        raise ValueError(
            f"IRA mismatch at 0x{addr:05X}: expected '{contains}', got '{row['instruction']}'"
        )
    return row


def source_ref(record: Dict[str, str], key: str = "asm") -> Dict[str, str]:
    text = record[key] if key in record else record["instruction"]
    return {"address": record["address"], "instruction": text}


IMM_PUSH_RE = re.compile(r"move\.w\s+#\$(?P<imm>[0-9a-fA-F]+),\s*-\(a7\)", re.IGNORECASE)
REG_PUSH_RE = re.compile(r"move\.w\s+(?P<reg>d[0-7]),\s*-\(a7\)", re.IGNORECASE)


def mk_param(
    name: str,
    kind: str,
    source: Dict[str, str],
    value: int | None = None,
    value_expr: str | None = None,
    push_source: Dict[str, str] | None = None,
) -> Dict[str, Any]:
    payload: Dict[str, Any] = {
        "name": name,
        "kind": kind,
        "source": source,
    }
    if value is not None:
        payload["value"] = value
    if value_expr is not None:
        payload["value_expr"] = value_expr
    if push_source is not None:
        payload["push_source"] = push_source
    return payload


def parameter_provenance(parameters: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []
    for p in parameters:
        addrs = [p["source"]["address"]]
        if "push_source" in p:
            addrs.append(p["push_source"]["address"])
        out.append(
            {
                "parameter": p["name"],
                "kind": p["kind"],
                "source_addresses": addrs,
            }
        )
    return out


def mk_target_fields(
    static_target: str, observed_behavior_target: str | None = None
) -> Dict[str, str]:
    observed = observed_behavior_target or static_target
    return {
        "static_target": static_target,
        "observed_behavior_target": observed,
    }


def family_target_maps(events: List[Dict[str, Any]]) -> Dict[str, Dict[str, List[str]]]:
    static_map: Dict[str, set[str]] = {}
    observed_map: Dict[str, set[str]] = {}
    for event in events:
        slot = event.get("slot")
        if not isinstance(slot, str):
            continue
        static_target = str(event.get("static_target", event.get("resolved_target", "unknown")))
        observed_target = str(
            event.get("observed_behavior_target", event.get("resolved_target", "unknown"))
        )
        static_map.setdefault(slot, set()).add(static_target)
        observed_map.setdefault(slot, set()).add(observed_target)
    return {
        "static_target": {slot: sorted(values) for slot, values in sorted(static_map.items())},
        "observed_behavior_target": {
            slot: sorted(values) for slot, values in sorted(observed_map.items())
        },
    }


def collect_short_note_calls(cap: Dict[int, Dict[str, str]]) -> List[Dict[str, Any]]:
    rows = sorted((addr, rec["asm"]) for addr, rec in cap.items())
    out: List[Dict[str, Any]] = []
    for i, (addr, asm) in enumerate(rows):
        if "jsr       -$7c62(a4)" not in asm.lower():
            continue
        prev_addr, prev_asm = rows[i - 1] if i > 0 else (None, "")
        arg_kind = "unknown"
        immediate: int | None = None
        arg_expr: str | None = None
        if prev_addr is not None:
            imm_match = IMM_PUSH_RE.search(prev_asm)
            if imm_match:
                immediate = int(imm_match.group("imm"), 16)
                arg_kind = "immediate_u16"
            else:
                reg_match = REG_PUSH_RE.search(prev_asm)
                if reg_match:
                    arg_kind = "register_u16"
                    arg_expr = reg_match.group("reg").lower()
                elif "clr.w     -(a7)" in prev_asm.lower():
                    arg_kind = "immediate_u16"
                    immediate = 0
                else:
                    arg_expr = prev_asm
        out.append(
            {
                "call_address": f"0x{addr:06X}",
                "call_instruction": asm,
                "arg_kind": arg_kind,
                "arg_immediate": immediate,
                "arg_expr": arg_expr,
                "arg_source": {
                    "address": f"0x{prev_addr:06X}" if prev_addr is not None else "unknown",
                    "instruction": prev_asm,
                },
            }
        )
    return out


def build_payload(cap: Dict[int, Dict[str, str]], ira: Dict[int, Dict[str, str]]) -> Dict[str, object]:
    # Title song start/step/poll evidence.
    cap_1010 = require_cap(cap, 0x001010, "move.w    #$47, -(a7)")
    cap_1014 = require_cap(cap, 0x001014, "move.w    #$12d, -(a7)")
    cap_1018 = require_cap(cap, 0x001018, "move.w    #$8, -(a7)")
    cap_101c = require_cap(cap, 0x00101C, "move.w    #$e0, -(a7)")
    cap_1020 = require_cap(cap, 0x001020, "clr.w     -(a7)")
    cap_1022 = require_cap(cap, 0x001022, "move.w    #$1, -(a7)")
    cap_1026 = require_cap(cap, 0x001026, "clr.w     -(a7)")
    cap_1028 = require_cap(cap, 0x001028, "jsr       -$7b9c(a4)")
    cap_1034 = require_cap(cap, 0x001034, "clr.w     -(a7)")
    cap_1036 = require_cap(cap, 0x001036, "jsr       -$7c3e(a4)")
    cap_1058 = require_cap(cap, 0x001058, "move.w    #$1, -(a7)")
    cap_105c = require_cap(cap, 0x00105C, "jsr       -$7c3e(a4)")
    cap_1ff6 = require_cap(cap, 0x001FF6, "move.w    #$12d, -(a7)")
    cap_1ffe = require_cap(cap, 0x001FFE, "move.w    #$e8, -(a7)")
    cap_2002 = require_cap(cap, 0x002002, "clr.w     -(a7)")
    cap_2004 = require_cap(cap, 0x002004, "jsr       -$7ba8(a4)")
    cap_200e = require_cap(cap, 0x00200E, "cmpi.w    #$48")
    cap_2014 = require_cap(cap, 0x002014, "blt.b     $1ff6")

    ira_202c = require_ira(ira, 0x0202C, "JSR -31656(A4)")
    ira_2036 = require_ira(ira, 0x02036, "CMPI.W #$0048,-36(A5)")
    ira_203c = require_ira(ira, 0x0203C, "BLT.S LAB_201E")

    # Death entry + backend/tone dispatch evidence.
    cap_9f22 = require_cap(cap, 0x009F22, "jsr       -$7e96(a4)")
    cap_11660 = require_cap(cap, 0x011660, "jsr       -$7e96(a4)")
    cap_7e22 = require_cap(cap, 0x007E22, "move.w    #$33, -(a7)")
    cap_7e26 = require_cap(cap, 0x007E26, "move.w    #$31, -(a7)")
    cap_7e2a = require_cap(cap, 0x007E2A, "jsr       -$7f68(a4)")
    cap_7e90 = require_cap(cap, 0x007E90, "cmpi.w    #$31, -$2(a5)")
    cap_7ecc = require_cap(cap, 0x007ECC, "jsr       -$7bd8(a4)")
    cap_7ed2 = require_cap(cap, 0x007ED2, "cmpi.w    #$32, -$2(a5)")
    cap_7f14 = require_cap(cap, 0x007F14, "cmpi.w    #$33, -$2(a5)")
    cap_7f0e = require_cap(cap, 0x007F0E, "jsr       -$7bde(a4)")
    cap_7f54 = require_cap(cap, 0x007F54, "jsr       -$7bde(a4)")
    cap_7ebc = require_cap(cap, 0x007EBC, "move.w    #$20, -(a7)")
    cap_7ec0 = require_cap(cap, 0x007EC0, "move.w    #$1, -(a7)")
    cap_7ec8 = require_cap(cap, 0x007EC8, "move.l    $66(a0), -(a7)")
    cap_7efe = require_cap(cap, 0x007EFE, "move.w    #$20, -(a7)")
    cap_7f02 = require_cap(cap, 0x007F02, "move.w    #$1, -(a7)")
    cap_7f0a = require_cap(cap, 0x007F0A, "move.w    $5c(a0), -(a7)")
    cap_7f40 = require_cap(cap, 0x007F40, "move.w    #$20, -(a7)")
    cap_7f44 = require_cap(cap, 0x007F44, "move.w    #$1, -(a7)")
    cap_7f4e = require_cap(cap, 0x007F4E, "move.b    $25(a0), d0")
    cap_7f52 = require_cap(cap, 0x007F52, "move.w    d0, -(a7)")

    ira_7e70 = require_ira(ira, 0x07E70, "CMPI.W #$0031,-2(A5)")
    ira_7e86 = require_ira(ira, 0x07E86, "CMPI.W #$0032,-2(A5)")
    ira_7e9c = require_ira(ira, 0x07E9C, "CMPI.W #$0033,-2(A5)")

    short_note_calls = collect_short_note_calls(cap)
    walk_beep_calls = [c for c in short_note_calls if c.get("arg_immediate") == 0x2D]
    immediate_values: Dict[int, int] = {}
    for call in short_note_calls:
        imm = call.get("arg_immediate")
        if imm is not None:
            immediate_values[int(imm)] = immediate_values.get(int(imm), 0) + 1

    events: List[Dict[str, Any]] = []

    title_start_params = [
        mk_param("channel", "immediate_u16", source_ref(cap_1010), value=0x47),
        mk_param("song_id", "immediate_u16", source_ref(cap_1014), value=0x12D),
        mk_param("arg2", "immediate_u16", source_ref(cap_1018), value=0x08),
        mk_param("volume_or_tempo", "immediate_u16", source_ref(cap_101c), value=0xE0),
        mk_param("arg4", "immediate_u16", source_ref(cap_1020), value=0),
        mk_param("gate", "immediate_u16", source_ref(cap_1022), value=1),
        mk_param("arg6", "immediate_u16", source_ref(cap_1026), value=0),
    ]
    events.append(
        {
            "event_id": "title.song.start_blocking",
            "family_id": "title.song.start_step_poll",
            "domain": "title",
            "confidence": "proven",
            "source_address": "0x001028",
            "source_addresses": ["0x001010", "0x001014", "0x001028"],
            "call_type": "song_start",
            "call": cap_1028["asm"],
            "slot": "-$7B9C(A4)",
            **mk_target_fields("0x22EAA"),
            "resolved_target": "0x22EAA",
            "parameters": title_start_params,
            "parameter_provenance": parameter_provenance(title_start_params),
            "evidence": {"capstone": [source_ref(cap_1028)], "ira": []},
        }
    )

    title_poll0_params = [mk_param("poll_mode", "immediate_u16", source_ref(cap_1034), value=0)]
    events.append(
        {
            "event_id": "title.song.poll_mode0",
            "family_id": "title.song.start_step_poll",
            "domain": "title",
            "confidence": "likely",
            "source_address": "0x001036",
            "source_addresses": ["0x001034", "0x001036"],
            "call_type": "song_poll",
            "call": cap_1036["asm"],
            "slot": "-$7C3E(A4)",
            **mk_target_fields("0x21EAE"),
            "resolved_target": "0x21EAE",
            "parameters": title_poll0_params,
            "parameter_provenance": parameter_provenance(title_poll0_params),
            "notes": "Poll semantics inferred from title flow and argument toggling.",
            "evidence": {"capstone": [source_ref(cap_1034), source_ref(cap_1036)], "ira": []},
        }
    )

    title_poll1_params = [mk_param("poll_mode", "immediate_u16", source_ref(cap_1058), value=1)]
    events.append(
        {
            "event_id": "title.song.poll_mode1",
            "family_id": "title.song.start_step_poll",
            "domain": "title",
            "confidence": "likely",
            "source_address": "0x00105C",
            "source_addresses": ["0x001058", "0x00105C"],
            "call_type": "song_poll",
            "call": cap_105c["asm"],
            "slot": "-$7C3E(A4)",
            **mk_target_fields("0x21EAE"),
            "resolved_target": "0x21EAE",
            "parameters": title_poll1_params,
            "parameter_provenance": parameter_provenance(title_poll1_params),
            "notes": "Poll semantics inferred from title flow and argument toggling.",
            "evidence": {"capstone": [source_ref(cap_1058), source_ref(cap_105c)], "ira": []},
        }
    )

    title_step_params = [
        mk_param("song_id", "immediate_u16", source_ref(cap_1ff6), value=0x12D),
        mk_param("step_index", "expression", source_ref(cap[0x001FFA]), value_expr="-$24(A5)"),
        mk_param("tempo", "immediate_u16", source_ref(cap_1ffe), value=0xE8),
        mk_param("flags", "immediate_u16", source_ref(cap_2002), value=0),
    ]
    events.append(
        {
            "event_id": "title.loop.song_step",
            "family_id": "title.song.start_step_poll",
            "domain": "title",
            "confidence": "proven",
            "source_address": "0x002004",
            "source_addresses": ["0x001FF6", "0x002004", "0x00200E", "0x002014"],
            "call_type": "song_step",
            "call": cap_2004["asm"],
            "slot": "-$7BA8(A4)",
            **mk_target_fields("0x22D68"),
            "resolved_target": "0x22D68",
            "parameters": title_step_params,
            "parameter_provenance": parameter_provenance(title_step_params),
            "loop_control": {
                "cmp_limit": {
                    "value": 0x48,
                    "source": source_ref(cap_200e),
                },
                "back_branch": source_ref(cap_2014),
            },
            "evidence": {
                "capstone": [
                    source_ref(cap_1ff6),
                    source_ref(cap_2004),
                    source_ref(cap_200e),
                    source_ref(cap_2014),
                ],
                "ira": [
                    source_ref(ira_202c, key="instruction"),
                    source_ref(ira_2036, key="instruction"),
                    source_ref(ira_203c, key="instruction"),
                ],
            },
        }
    )

    events.extend(
        [
            {
                "event_id": "death.entry.party_wipe",
                "family_id": "death.tone.dispatch",
                "domain": "death",
                "confidence": "likely",
                "source_address": "0x009F22",
                "source_addresses": ["0x009F22"],
                "call_type": "backend_entry",
                "call": cap_9f22["asm"],
                "slot": "-$7E96(A4)",
                **mk_target_fields("0x063EE", observed_behavior_target="0x007DCC"),
                "resolved_target": "0x063EE",
                "parameters": [],
                "parameter_provenance": [],
                "notes": "Thunk map static target differs from observed death backend entry routine.",
                "evidence": {"capstone": [source_ref(cap_9f22)], "ira": []},
            },
            {
                "event_id": "death.entry.combat_defeat",
                "family_id": "death.tone.dispatch",
                "domain": "death",
                "confidence": "likely",
                "source_address": "0x011660",
                "source_addresses": ["0x011660"],
                "call_type": "backend_entry",
                "call": cap_11660["asm"],
                "slot": "-$7E96(A4)",
                **mk_target_fields("0x063EE", observed_behavior_target="0x007DCC"),
                "resolved_target": "0x063EE",
                "parameters": [],
                "parameter_provenance": [],
                "notes": "Thunk map static target differs from observed death backend entry routine.",
                "evidence": {"capstone": [source_ref(cap_11660)], "ira": []},
            },
        ]
    )

    death_picker_params = [
        mk_param("max_type", "immediate_u16", source_ref(cap_7e22), value=0x33),
        mk_param("min_type", "immediate_u16", source_ref(cap_7e26), value=0x31),
    ]
    events.append(
        {
            "event_id": "death.dispatch.pick_type_31_33",
            "family_id": "death.tone.dispatch",
            "domain": "death",
            "confidence": "proven",
            "source_address": "0x007E2A",
            "source_addresses": ["0x007E22", "0x007E26", "0x007E2A"],
            "call_type": "dispatch_select",
            "call": cap_7e2a["asm"],
            "slot": "-$7F68(A4)",
            **mk_target_fields("0x042AA"),
            "resolved_target": "0x042AA",
            "parameters": death_picker_params,
            "parameter_provenance": parameter_provenance(death_picker_params),
            "evidence": {"capstone": [source_ref(cap_7e22), source_ref(cap_7e2a)], "ira": []},
        }
    )

    death_type31_params = [
        mk_param("channel_or_mode", "immediate_u16", source_ref(cap_7ebc), value=0x20),
        mk_param("gate", "immediate_u16", source_ref(cap_7ec0), value=0x1),
        mk_param(
            "period_or_pitch_long",
            "memory_u32",
            source_ref(cap_7ec8),
            value_expr="$66(A0)",
        ),
    ]
    events.append(
        {
            "event_id": "death.dispatch.type31_call_7BD8",
            "family_id": "death.tone.dispatch",
            "domain": "death",
            "confidence": "likely",
            "source_address": "0x007ECC",
            "source_addresses": ["0x007E90", "0x007ECC"],
            "call_type": "play_tone_like",
            "call": cap_7ecc["asm"],
            "slot": "-$7BD8(A4)",
            **mk_target_fields("0x2249C"),
            "resolved_target": "0x2249C",
            "parameters": death_type31_params,
            "parameter_provenance": parameter_provenance(death_type31_params),
            "notes": "Likely same tone-dispatch family as -$7BDE(A4), but semantics not fully normalized.",
            "evidence": {
                "capstone": [source_ref(cap_7e90), source_ref(cap_7ecc)],
                "ira": [source_ref(ira_7e70, key="instruction")],
            },
        }
    )

    death_type32_params = [
        mk_param("channel_or_mode", "immediate_u16", source_ref(cap_7efe), value=0x20),
        mk_param("gate", "immediate_u16", source_ref(cap_7f02), value=0x1),
        mk_param(
            "period_or_pitch_word",
            "memory_u16",
            source_ref(cap_7f0a),
            value_expr="$5C(A0)",
        ),
    ]
    events.append(
        {
            "event_id": "death.tone.type32_from_age_word",
            "family_id": "death.tone.dispatch",
            "domain": "death",
            "confidence": "proven",
            "source_address": "0x007F0E",
            "source_addresses": ["0x007ED2", "0x007F0E"],
            "call_type": "play_tone",
            "call": cap_7f0e["asm"],
            "slot": "-$7BDE(A4)",
            **mk_target_fields("0x22480"),
            "resolved_target": "0x22480",
            "parameters": death_type32_params,
            "parameter_provenance": parameter_provenance(death_type32_params),
            "evidence": {
                "capstone": [source_ref(cap_7ed2), source_ref(cap_7f0e)],
                "ira": [source_ref(ira_7e86, key="instruction")],
            },
        }
    )

    death_type33_params = [
        mk_param("channel_or_mode", "immediate_u16", source_ref(cap_7f40), value=0x20),
        mk_param("gate", "immediate_u16", source_ref(cap_7f44), value=0x1),
        mk_param(
            "period_or_pitch_word",
            "register_u16",
            source_ref(cap_7f4e),
            value_expr="d0 <- $25(A0)",
            push_source=source_ref(cap_7f52),
        ),
    ]
    events.append(
        {
            "event_id": "death.tone.type33_from_level_byte",
            "family_id": "death.tone.dispatch",
            "domain": "death",
            "confidence": "proven",
            "source_address": "0x007F54",
            "source_addresses": ["0x007F14", "0x007F54"],
            "call_type": "play_tone",
            "call": cap_7f54["asm"],
            "slot": "-$7BDE(A4)",
            **mk_target_fields("0x22480"),
            "resolved_target": "0x22480",
            "parameters": death_type33_params,
            "parameter_provenance": parameter_provenance(death_type33_params),
            "evidence": {
                "capstone": [source_ref(cap_7f14), source_ref(cap_7f54)],
                "ira": [source_ref(ira_7e9c, key="instruction")],
            },
        }
    )

    for idx, call in enumerate(walk_beep_calls):
        param = [
            {
                "name": "note_index",
                "kind": "immediate_u16",
                "value": call["arg_immediate"],
                "source": call["arg_source"],
            }
        ]
        events.append(
            {
                "event_id": f"walk.beep.note_0x2D.callsite_{idx+1}",
                "family_id": "walk.beep.family",
                "domain": "walk",
                "confidence": "likely",
                "source_address": call["call_address"],
                "source_addresses": [call["arg_source"]["address"], call["call_address"]],
                "call_type": "play_note",
                "call": call["call_instruction"],
                "slot": "-$7C62(A4)",
                **mk_target_fields("0x218EA"),
                "resolved_target": "0x218EA",
                "parameters": param,
                "parameter_provenance": parameter_provenance(param),
                "notes": "Immediate note 0x2D callsite currently used as deterministic walk-beep candidate.",
                "evidence": {
                    "capstone": [call["arg_source"], {"address": call["call_address"], "instruction": call["call_instruction"]}],
                    "ira": [],
                },
            }
        )

    note_bucket_params = [
        {
            "name": "total_short_note_calls",
            "kind": "derived_u32",
            "value": len(short_note_calls),
            "source": {"address": "scan:mm2.capstone.asm", "instruction": "all jsr -$7c62(a4) callsites"},
        },
        {
            "name": "immediate_argument_cardinality",
            "kind": "derived_u32",
            "value": len(immediate_values),
            "source": {"address": "scan:mm2.capstone.asm", "instruction": "preceding move.w #$imm,-(a7) patterns"},
        },
    ]
    events.append(
        {
            "event_id": "short_note.sfx.metadata_buckets",
            "family_id": "short_note.sfx.metadata",
            "domain": "shared",
            "confidence": "likely",
            "source_address": "scan:mm2.capstone.asm",
            "source_addresses": [c["call_address"] for c in short_note_calls],
            "call_type": "metadata_bucket",
            "call": "scan jsr -$7c62(a4)",
            "slot": "-$7C62(A4)",
            **mk_target_fields("0x218EA"),
            "resolved_target": "0x218EA",
            "parameters": note_bucket_params,
            "parameter_provenance": parameter_provenance(note_bucket_params),
            "buckets": {
                "arg_kinds": {
                    "immediate_u16": sum(1 for c in short_note_calls if c["arg_kind"] == "immediate_u16"),
                    "register_u16": sum(1 for c in short_note_calls if c["arg_kind"] == "register_u16"),
                    "unknown": sum(1 for c in short_note_calls if c["arg_kind"] == "unknown"),
                },
                "immediate_values": [
                    {"value": value, "count": count}
                    for value, count in sorted(immediate_values.items(), key=lambda item: (item[0], item[1]))
                ],
                "walk_beep_note_0x2d_callsites": [c["call_address"] for c in walk_beep_calls],
            },
            "evidence": {"capstone": [], "ira": []},
        }
    )

    families = [
        {
            "family_id": "title.song.start_step_poll",
            "domain": "title",
            "confidence": "proven",
            "description": "Title-song start, loop step, and polling call family.",
            "source_addresses": ["0x001028", "0x002004", "0x001036", "0x00105C"],
            "event_ids": [
                "title.song.start_blocking",
                "title.song.poll_mode0",
                "title.song.poll_mode1",
                "title.loop.song_step",
            ],
        },
        {
            "family_id": "death.tone.dispatch",
            "domain": "death",
            "confidence": "likely",
            "description": "Death/defeat tone dispatch with type picker and per-type branches.",
            "source_addresses": ["0x009F22", "0x011660", "0x007E2A", "0x007ECC", "0x007F0E", "0x007F54"],
            "event_ids": [
                "death.entry.party_wipe",
                "death.entry.combat_defeat",
                "death.dispatch.pick_type_31_33",
                "death.dispatch.type31_call_7BD8",
                "death.tone.type32_from_age_word",
                "death.tone.type33_from_level_byte",
            ],
        },
        {
            "family_id": "walk.beep.family",
            "domain": "walk",
            "confidence": "likely",
            "description": "Deterministic callsites that push immediate note 0x2D into short-note helper.",
            "source_addresses": [c["call_address"] for c in walk_beep_calls],
            "event_ids": [e["event_id"] for e in events if e["family_id"] == "walk.beep.family"],
        },
        {
            "family_id": "short_note.sfx.metadata",
            "domain": "shared",
            "confidence": "likely",
            "description": "Global metadata buckets for short-note helper usage/provenance.",
            "source_addresses": [c["call_address"] for c in short_note_calls],
            "event_ids": ["short_note.sfx.metadata_buckets"],
        },
    ]

    events_by_family = {
        family["family_id"]: [event for event in events if event["family_id"] == family["family_id"]]
        for family in families
    }
    for family in families:
        family_targets = family_target_maps(events_by_family.get(family["family_id"], []))
        family["static_target"] = family_targets["static_target"]
        family["observed_behavior_target"] = family_targets["observed_behavior_target"]

    return {
        "schema_version": 3,
        "description": "MM2 audio intermediate events for deterministic MOD-oriented extraction.",
        "deterministic": True,
        "confidence_levels": ["proven", "likely", "uncertain"],
        "sources": {
            "capstone": str(CAPSTONE_ASM.relative_to(ROOT)),
            "ira": str(IRA_ASM.relative_to(ROOT)),
        },
        "counts": {
            "families": len(families),
            "events": len(events),
            "short_note_callsites": len(short_note_calls),
            "walk_beep_immediate_0x2d_callsites": len(walk_beep_calls),
        },
        "event_families": families,
        "events": events,
        "notes": [
            "Confidence tags are assigned per family and per event.",
            "Dual-view target fields separate static thunk-map targets from observed runtime behavior targets.",
            "Parameters include explicit provenance and source addresses where available.",
            "Walk-beep and short-note buckets are deterministic callsite scans without behavioral guessing.",
        ],
    }


def main() -> None:
    cap = parse_capstone_lines()
    ira = parse_ira_lines()
    payload = build_payload(cap, ira)
    OUT_JSON.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(f"Wrote {OUT_JSON}")
    print(f"Families: {len(payload['event_families'])}")
    print(f"Events: {len(payload['events'])}")


if __name__ == "__main__":
    main()
