#!/usr/bin/env python3
"""Harvest symbol names from EXTRACTED/docs and merge into mm2_symbols.yaml."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from mm2_symbols import (  # noqa: E402
    DEFAULT_SYMBOLS,
    build_bootstrap,
    export_json,
    harvest_from_docs,
    harvest_ira_func_labels,
    load_symbols,
    merge_symbols,
    prune_symbols,
    save_symbols,
)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--merge",
        action="store_true",
        help="Merge harvest into existing yaml (keep existing entries on conflict)",
    )
    ap.add_argument(
        "--overwrite",
        action="store_true",
        help="Replace yaml with harvest + bootstrap (existing manual edits lost)",
    )
    ap.add_argument("--json", type=Path, help="Also write JSON export to this path")
    ap.add_argument("-o", "--output", type=Path, default=DEFAULT_SYMBOLS)
    args = ap.parse_args()

    harvested = harvest_from_docs()
    ira = harvest_ira_func_labels()
    bootstrap = build_bootstrap()

    if args.overwrite:
        merged = merge_symbols(bootstrap, harvested, ira, prefer="new")
    elif args.merge or args.output.exists():
        base = load_symbols(args.output) if args.output.exists() else bootstrap
        merged = merge_symbols(base, bootstrap, harvested, ira, prefer="base")
        merged = prune_symbols(merged)
    else:
        merged = merge_symbols(bootstrap, harvested, ira, prefer="new")

    merged = prune_symbols(merged)
    save_symbols(merged, args.output)
    nf = len(merged["functions"])
    na = len(merged["a4_fields"])
    nt = len(merged["a4_thunks"])
    print(f"Wrote {args.output}")
    print(f"  functions:  {nf}")
    print(f"  a4_fields:  {na}")
    print(f"  a4_thunks:  {nt}")

    if args.json:
        export_json(merged, args.json)
        print(f"  json:       {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
