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
| `service_title sN mode=M` | OP_0B (sign/title window — not OP_01 dialog) |
| `wait space` / `wait key` | OP_07/08 |
| `ask yes_no` | OP_09/0A |
| `if class Knight:` | OP_2D + branch |
| `if gold >= N:` | OP_24 + branch |
| `if answer == "46":` | OP_30 + branch |
| `set quest_complete` | OP_18 masked `$75` |
| `selector 0xNN` | OP_0E (raw dispatch byte) |
| `go_to screen N pos 0xNN` | OP_0C |
| `fight monsters ...` | OP_12 |
| `abort` / `end` | OP_29 / OP_0F |

Decompile is **mechanical**: string refs are `s0`, `s1`, … (index into this
location's string bank); scripts are `event_00`, `event_01`, …; OP_0E is always
`selector 0xNN`. Read the `strings:` block for actual text. Optional hand-author
sugar `shop tavern` still parses to OP_0E `0x01`.

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
