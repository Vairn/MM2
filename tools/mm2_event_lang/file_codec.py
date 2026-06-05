"""Load/save event.dat via lifted AST."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from decode_event import encode_event_dat, load_event_records  # noqa: E402

from .ast import EventFile
from .decompile import decompile_file, decompile_location
from .dsl_emit import emit_file, emit_location
from .dsl_parser import parse_file, parse_file_path
from .encode import encode_file


def load_event_dat(path: Path) -> EventFile:
    return decompile_file(path)


def save_event_dat(path: Path, event: EventFile) -> None:
    data = encode_file(event)
    path.write_bytes(data)


def compile_file(dsl_path: Path, out_path: Path, original: Path | None = None) -> None:
    event = parse_file_path(dsl_path)
    if original and original.is_file():
        _, records = load_event_records(original.read_bytes())
        header = load_event_records(original.read_bytes())[0]
        event.header = header
        for i, loc in enumerate(event.locations):
            if loc.raw_blob is None and i < len(records):
                pass
    save_event_dat(out_path, event)


def compile_manifest(manifest_dir: Path, out_path: Path, original: Path) -> None:
    import yaml

    manifest = manifest_dir / "manifest.yaml"
    if manifest.is_file():
        data = yaml.safe_load(manifest.read_text(encoding="utf-8"))
        files = data.get("locations", [])
    else:
        files = sorted(manifest_dir.glob("loc_*.mm2evt"))

    orig_data = original.read_bytes()
    header, records = load_event_records(orig_data)
    out_records = list(records)

    for entry in files:
        if isinstance(entry, dict):
            path = manifest_dir / entry["file"]
            loc_id = entry["id"]
        else:
            path = Path(entry)
            loc_id = None
        loc = parse_file_path(path).locations[0]
        if loc_id is not None:
            loc.id = loc_id
        blob = __import__("mm2_event_lang.encode", fromlist=["encode_location"]).encode_location(loc)
        if loc_id is not None and loc_id < len(out_records):
            out_records[loc_id] = blob

    out_path.write_bytes(encode_event_dat(out_records, header))


def decompile_to_dir(event_path: Path, out_dir: Path) -> None:
    event = decompile_file(event_path)
    out_dir.mkdir(parents=True, exist_ok=True)
    for loc in event.locations:
        fname = f"loc_{loc.id:02d}.mm2evt"
        (out_dir / fname).write_text(emit_location(loc), encoding="utf-8", newline="\n")

    manifest_lines = ["locations:"]
    for loc in event.locations:
        manifest_lines.append(f"  - id: {loc.id}")
        manifest_lines.append(f"    file: loc_{loc.id:02d}.mm2evt")
    (out_dir / "manifest.yaml").write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")
