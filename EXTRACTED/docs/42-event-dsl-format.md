# MM2 Event Script DSL (`.mm2evt`)

Readable script format for authoring `event.dat` tile events. Implemented in
[`tools/mm2_event_lang/`](../../tools/mm2_event_lang/).

## CLI

From the `tools/` directory:

```powershell
# Decompile one location to readable script
python -m mm2_event_lang decompile -l 34 ../event.dat

# Export all 71 locations
python -m mm2_event_lang decompile -o ../events ../event.dat

# Write single location to file
python -m mm2_event_lang decompile -l 53 --output loc_53.mm2evt ../event.dat

# Merge edited location back into event.dat
python -m mm2_event_lang patch-loc -l 53 loc_53.mm2evt --original ../event.dat

# Byte-identical round-trip test (71/71)
python -m mm2_event_lang roundtrip ../event.dat

# Low-level opcode listing (debug only)
python -m mm2_event_lang debug-bytecode -l 53 -e 4 ../event.dat
```

## File layout

```
events/
  manifest.yaml
  loc_00.mm2evt
  ...
  loc_70.mm2evt
```

## Syntax overview

```mm2evt
location 34
record standard

strings:
  plaque_intro:
    """A plaque left by the Jurors of Mount
       Farview reads: ..."""

on tile (0, 7) facing ns -> juror_plaque  @event 12

script juror_plaque:  @event 12
  say plaque_intro
  wait space
  quest farview_reward
```

### Triggers

| Syntax | Condition byte |
|--------|----------------|
| `always` | `0x10` |
| `enter` | `0x80` |
| `from north` | `0x20` |
| `facing ns` | `0xA0` |
| `any_direction` | `0xF0` |
| `when 0xNN` | raw |

### Statements

| Readable | Opcode(s) |
|----------|-----------|
| `say sN` | OP_01–06 — inline `# "..."` comment shows string-bank text |
| `service_title sN mode=M` | OP_0B — same inline string preview |
| `wait space` / `wait key` | OP_07/08 |
| `ask yes_no` | OP_09/0A |
| `if class_field 0xNN mask=0xMM:` | OP_2D + branch |
| `if gold >= N:` | OP_24 + branch |
| `if answer == "46":` | OP_30 + branch (decoded from script bytes) |
| `if has_item 0xNN probe=M:` | OP_28 + branch |
| `@apply_party_masked count=… set=… and=… or=…` | OP_18 |
| `selector 0xNN` | OP_0E dispatch byte; explicit asm cases get `# handler` comment |
| `go_to screen N pos 0xNN` | OP_0C |
| `fight monsters ...` | OP_12 |
| `abort` / `end` | OP_29 / OP_0F |

Decompile is **mechanical**: string refs are `s0`, `s1`, …; scripts are
`event_00`, …; OP_0E is `selector 0xNN` (with `# open_tavern_food` etc. when the
byte hits a dedicated handler @ asm `0x160C2`, not default-range bins); OP_18 is
raw `@apply_party_masked` bytes. No quest-flag or class names are inferred.
Optional hand-author sugar
(`set quest_flag …`, `class Knight`, `shop tavern`) still parses when editing.

Legacy alias: `say_service` parses as `service_title`.

Structured `if`/`else` is compiled to `skip_tokens` internally. Human-facing
decompile never shows skip counts.

### Castle blobs

Locations 63, 65, 68–70 use `record castle_blob` with a `raw_record:` hex block
for byte-exact round-trip.

## Architecture

```
event.dat → parse_bytecode → cfg_lift → semantic_tables → dsl_emit → .mm2evt
.mm2evt → dsl_parser → lower → encode → event.dat
```

Unmodified locations preserve the original record bytes on decompile; re-encode
uses the lifted AST only when `modified=True` (parsed from DSL).

## C API stub

[`EXTRACTED/decomp/mm2_event_ast.h`](../decomp/mm2_event_ast.h) mirrors the Python
AST. JSON codec is stubbed in `mm2_event_ast_codec.c`.

## Editor

MM2ED Events section: **Export .mm2evt** / **Import .mm2evt** buttons, plus
**Block view** toggle for readable script blocks in the node graph.

## Tests

```powershell
cd tools
python test_event_lang_roundtrip.py
```

Golden checks: loc 34 (Farview juror), loc 53 (Sorcerer door puzzle).
