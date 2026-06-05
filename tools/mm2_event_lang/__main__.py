#!/usr/bin/env python3
"""CLI for MM2 event script DSL decompile/compile."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from mm2_event_lang.decompile import decompile_file, decompile_location  # noqa: E402
from mm2_event_lang.dsl_emit import emit_file, emit_location  # noqa: E402
from mm2_event_lang.dsl_parser import parse_file_path  # noqa: E402
from mm2_event_lang.encode import encode_file  # noqa: E402
from mm2_event_lang.file_codec import decompile_to_dir, load_event_dat, save_event_dat  # noqa: E402
from decode_event import load_event_records, round_trip_check  # noqa: E402


def cmd_decompile(args: argparse.Namespace) -> int:
    src = Path(args.input)
    if args.out_dir:
        decompile_to_dir(src, Path(args.out_dir))
        print(f"Wrote .mm2evt files to {args.out_dir}")
        return 0
    event = decompile_file(src)
    if args.location is not None:
        loc = event.locations[args.location]
        text = emit_location(loc)
    else:
        text = emit_file(event)
    if args.output:
        Path(args.output).write_text(text, encoding="utf-8", newline="\n")
        print(f"Wrote {args.output}")
    else:
        print(text)
    return 0


def cmd_compile(args: argparse.Namespace) -> int:
    src = Path(args.input)
    dst = Path(args.output)
    event = parse_file_path(src)
    orig = Path(args.original) if args.original else None
    if orig and orig.is_file():
        header, records = load_event_records(orig.read_bytes())
        event.header = header
        new_records = list(records)
        for loc in event.locations:
            from mm2_event_lang.encode import encode_location

            new_records[loc.id] = encode_location(loc)
        from decode_event import encode_event_dat

        dst.write_bytes(encode_event_dat(new_records, header))
    else:
        save_event_dat(dst, event)
    print(f"Wrote {dst} ({dst.stat().st_size} bytes)")
    return 0


def cmd_roundtrip(args: argparse.Namespace) -> int:
    src = Path(args.input)
    data = src.read_bytes()
    header, records = load_event_records(data)
    from mm2_event_lang.encode import encode_location

    ok = 0
    fail = 0
    for i, rec in enumerate(records):
        loc = decompile_location(rec, i)
        rebuilt = encode_location(loc)
        if rebuilt == rec:
            ok += 1
        else:
            fail += 1
            if args.verbose:
                print(f"loc {i}: mismatch orig={len(rec)} rebuilt={len(rebuilt)}")
    print(f"roundtrip: {ok}/{len(records)} byte-identical")
    if fail:
        return 1
    full = encode_file(decompile_file(src))
    if full == data:
        print("full file: OK")
    else:
        print("full file: MISMATCH")
        return 1
    return 0


def cmd_debug_bytecode(args: argparse.Namespace) -> int:
    from decode_event import decode_location, split_script_segments
    from decode_event import decompile_op, parse_segment_stream_nodes, try_load_default_items

    data = Path(args.input).read_bytes()
    _, records = load_event_records(data)
    loc_id = args.location or 0
    blob = records[loc_id]
    loc = decode_location(blob, loc_id)
    items = try_load_default_items()
    script = blob[loc["script_offset"] : loc["string_table_offset"]]
    segments = split_script_segments(script)
    evt = args.event or 0
    if evt >= len(segments):
        print("event not found")
        return 1
    seg = segments[evt]
    nodes = parse_segment_stream_nodes(seg)
    for idx, n in enumerate(nodes):
        op = int(n["op"])
        args_b = [int(x) for x in n["args"]]
        print(f"{idx:02d}: {decompile_op(op, args_b, loc['strings'], items)}")
    return 0


def cmd_patch_loc(args: argparse.Namespace) -> int:
    """Replace one location record from .mm2evt into event.dat."""
    from mm2_event_lang.encode import encode_location

    orig = Path(args.original)
    dsl = Path(args.input)
    dst = Path(args.output) if args.output else orig
    loc_id = args.location
    header, records = load_event_records(orig.read_bytes())
    loc = parse_file_path(dsl).locations[0]
    loc.id = loc_id
    loc.modified = True
    records[loc_id] = encode_location(loc)
    from decode_event import encode_event_dat

    dst.write_bytes(encode_event_dat(records, header))
    print(f"Patched location {loc_id} -> {dst}")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description="MM2 event script DSL")
    sub = p.add_subparsers(dest="cmd", required=True)

    d = sub.add_parser("decompile", help="event.dat -> .mm2evt")
    d.add_argument("input", nargs="?", default=str(ROOT / "event.dat"))
    d.add_argument("-o", "--out-dir", help="output directory for per-location files")
    d.add_argument("--output", help="write single-location or full decompile to file")
    d.add_argument("-l", "--location", type=int, help="single location id")
    d.set_defaults(func=cmd_decompile)

    c = sub.add_parser("compile", help=".mm2evt -> event.dat")
    c.add_argument("input")
    c.add_argument("output")
    c.add_argument("--original", help="original event.dat for header/merge")
    c.set_defaults(func=cmd_compile)

    r = sub.add_parser("roundtrip", help="test byte-identical roundtrip")
    r.add_argument("input", nargs="?", default=str(ROOT / "event.dat"))
    r.add_argument("-v", "--verbose", action="store_true")
    r.set_defaults(func=cmd_roundtrip)

    b = sub.add_parser("debug-bytecode", help="low-level opcode listing")
    b.add_argument("input", nargs="?", default=str(ROOT / "event.dat"))
    b.add_argument("-l", "--location", type=int, default=0)
    b.add_argument("-e", "--event", type=int, default=0)
    b.set_defaults(func=cmd_debug_bytecode)

    p = sub.add_parser("patch-loc", help="merge one .mm2evt location into event.dat")
    p.add_argument("-l", "--location", type=int, required=True)
    p.add_argument("input", help=".mm2evt file")
    p.add_argument("--original", required=True, help="source event.dat")
    p.add_argument("-o", "--output", help="output event.dat (default: overwrite original)")
    p.set_defaults(func=cmd_patch_loc)

    args = p.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
