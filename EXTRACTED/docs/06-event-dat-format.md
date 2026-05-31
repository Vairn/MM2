# event.dat Format — Might & Magic II (Amiga)

Decoded from 68k assembly at routines `0x92F2` (loader), `0x1754A` (init/parse),
`0x175E2` (tile lookup), and `0x172CA` (opcode interpreter).

## File Layout

```
+0x000  ┌──────────────────────────────────────┐
        │  Header: 71 entries × 6 bytes = 426  │
+0x1AA  ├──────────────────────────────────────┤
        │  Location 0 data (variable length)   │
        ├──────────────────────────────────────┤
        │  Location 1 data ...                 │
        ├──────────────────────────────────────┤
        │  ...                                 │
        ├──────────────────────────────────────┤
        │  Location 70 data                    │
+175C7  └──────────────────────────────────────┘  (EOF = 95687 bytes)
```

### Header Entry (6 bytes, big-endian)

| Offset | Size | Field           | Notes                              |
|--------|------|-----------------|------------------------------------|
| +0     | 4    | data_offset     | Absolute byte offset from file start |
| +4     | 2    | data_length     | Clamped to 0x8AC (2220) at runtime |

All 71 entries are contiguous (entry[n].offset + entry[n].length == entry[n+1].offset).

## Per-Location Record

```
+0x00  ┌─────────────────────────────────────────┐
       │  Tile Event Table (3-byte triplets)      │
       │  ...                                     │
       │  00 00 00  (terminator)                  │
       ├─────────────────────────────────────────┤
       │  String offset word (2 bytes, LE)        │
       ├─────────────────────────────────────────┤
       │  Event script bytecodes                  │
       │  ...                                     │
       ├─────────────────────────────────────────┤
       │  String table (0xFF-terminated strings)  │
       └─────────────────────────────────────────┘
```

### 1. Tile Event Table

3-byte entries until `00 00 00`:

| Byte | Meaning                                              |
|------|------------------------------------------------------|
| 0    | Tile position: `(x << 4) | y` in 16×16 grid         |
| 1    | Event handler ID (dispatched to script interpreter)  |
| 2    | Condition flags (AND-masked with facing/context byte)|

The engine scans all triplets each time the party steps on a tile.
If `position` matches and `(flags & context) != 0`, the event fires.

**Known condition flag values (towns/outdoor):**

| Value | Meaning              |
|-------|----------------------|
| 0x10  | Always fires         |
| 0x20  | Directional (N?)     |
| 0x40  | Special/interact     |
| 0x80  | Enter/step-on        |
| 0xC0  | Enter + Special      |
| 0xF0  | Any direction        |

### 2. String Offset Word

2 bytes, little-endian. Relative offset from the word's own position to
the start of the string table:

```
string_table_offset = pos_of_word + word_value
```

### 3. Event Script Bytecodes

Opcode-based interpreter with ~51 opcodes (jump table at `0x17494`).
Each handler reads its own arguments sequentially from the byte stream.
Terminated by `0xFF`.

The `A4-$86AA` register tracks the current parse position. Event handler
IDs from the triplet table cause the interpreter to execute from the
script start until the matching handler fires.

### 4. String Table

Strings are 0xFF-terminated (not null-terminated). The `@` character
(0x40) represents a line break in the game's text renderer.

Indexed positionally — event scripts reference strings by sequential order.

## Relationship to map.dat

| File      | Buffer      | Size    | Screens | Purpose              |
|-----------|-------------|---------|---------|----------------------|
| map.dat   | A4-$EEF4   | 30720   | 60      | Tile/terrain visuals |
| event.dat | A4-$EEF8   | 95687   | 71      | Interactions/scripts |

Both are indexed by `screen_id`. The map uses fixed 512-byte blocks
(2 × 256-byte pages per screen); events use variable-length records.
Locations 60–70 are extra areas (quest zones, special dungeons) that
may share map screens or use extended addressing.

## Runtime Flow

1. On screen change, `sub_92F2` loads event data for new screen_id
   into a 2220-byte working buffer at `A4-$B838`.
2. First access calls `sub_1754A` to parse the triplet table and
   compute the string table offset.
3. Each game tick, `sub_175E2` computes tile position from player
   coords, scans the triplet table for matches, and dispatches
   matching events to the bytecode interpreter at `sub_172CA`.

## Tool

```
python tools/decode_event.py [path_to_event.dat] [location_id]
```

Omit location_id to dump all 71 locations.
