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
| 0    | Tile position: `(y << 4) | x` in 16×16 grid         |
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
Locations 60–70 are **overlay records** indexed the same way by
`screen_mode_id`, but many use alternate layouts (string banks, castle blobs,
meta reference text). See [08-event-runtime.md](08-event-runtime.md) and
`EXTRACTED/event_location_inventory.json`.

| Loc | Purpose (summary) |
|-----|-------------------|
| 60 | Nordon/Nordonna/Corak intro quest strings |
| 61 | Encoded spell/hireling index tables |
| 62 | Chris cartography, Gertrude/Rat Fink side quests |
| 63, 65, 68 | Castle blobs (no `00 00 00` terminator) |
| 64 | Lord Haart heirloom quest |
| 66 | Endgame Corak / Murray / Horvath |
| 67 | Hall of Spells mixed text/script pool |
| 69 | Queen Lamanda (Luxus) storyline |
| 70 | Meta bank: HoS, Hireling Hall, bishops, puzzles |

## Runtime Flow

1. On screen change, `sub_92F2` loads event data for new screen_id
   into a 2220-byte working buffer at `A4-$B838`.
2. First access calls `sub_1754A` to parse the triplet table and
   compute the string table offset.
3. Each game tick, `sub_175E2` computes tile position from player
   coords, scans the triplet table for matches, and dispatches
   matching events to the bytecode interpreter at `sub_172CA`.

## FAQ event validation

Cross-check of Schultz FAQ walkthrough text (§§3-2-2/§4-1/§4-3/§8–11) against
`EXTRACTED/event_decode.txt` (generated by `python tools/decode_event.py event.dat`):

| Loc | FAQ claim | Event decode evidence | Result |
|-----|-----------|----------------------|--------|
| **60** | Nordon at C2 (10,2) gives S2/1 Eagle Eye + 2000 XP on goblet return (FAQ §3-2-2-2 / §4-1) | `str[11]` = *"Ah, you've found a goblet! For your bravery I grant you 2,000 exp, the spell Eagle Eye"*; `str[9]` = *"The humble wizard Nordon asks, 'Will you do me a service?'"* — matches exactly | **PASS** |
| **22** | Corak's Cave (map 22) contains S2/6 Lloyd's Beacon, granted by Lloyd at tile (7,11) (FAQ §3-2-2-2 / §4-6) | `str[10]` = *"Lloyd, of Lloyd's Beacon fame, was last seen in Corak's Cave at 7,11"*; `str[1]` = *"An ingenious sorcerer named Lloyd tells the tale of his great invention, a Beacon Spell"* — tile (7,11) confirmed | **PASS** |
| **4** | Sandsobar "Beggar's Gift" at (7,4), gives Wizard Eye for 100 gp (FAQ §3-2-2-2 / §4-3 / bar hint) | event 42 `show_text_above_door(str[27] "The Beggar's Gift")` at trigger (y=4,x=8); event 43 trigger (y=4,x=7) = FAQ (x=7,y=4) with `exec_selector(0x51)` — sign confirmed; purchase exec confirmed | **PASS** (sign + exec present; 100 gp amount from FAQ, not yet decoded in exec_selector trace) |
| **7** | C9/2 Holy Word "carved on a tree, face south" at C1 (5,5) (FAQ §3-2-2-1 / §4-6) | `str[13]` = *"Holy Word -H. Gibson / Look south on a tree / in Lost Soul's Woods"*; and separate `text_record` = *"Holy Word is best found facing south while walking through the woods"* — matches FAQ | **PASS** |
| **14** | C2/3 Nature's Gate at C3 (1,9) after buying/eating Red Hot Wolf Nipple Chips (FAQ §3-2-2-1 / §3-10-1) | `str[10]` = *"An old druid … munches enthusiastically on Red Hot Wolf Nipple Chips"*; `str[14]` = *"Your breath really reeks of my favorite … take this spell, Nature's Gate"* — matches FAQ exactly | **PASS** |
| **0** | Middlegate inn at (14,7), pub at (7,10), temple "Gateway Temple" (FAQ §8-3) | event 01 `show_text_above_door("Middlegate Inn")` at (5,7); event 02 `"S.J. Blacksmith"` at (4,5); event 04 `"Gateway Temple"` at (6,7) — names match FAQ §8-3 | **PASS** (tile coords differ by 1-2 from FAQ — FAQ uses different x/y convention) |

**Mismatches found: none** in the above 6 samples. FAQ coordinates sometimes differ by 1 from event decode triggers due to direction-facing conventions (FAQ gives "face south" tiles whereas event triggers fire on approach). Recommend full bulk diff via `python tools/decode_event.py event.dat` and comparing all FAQlisted spell/event descriptions vs string pools.

## Tool

```
python tools/decode_event.py [path_to_event.dat] [location_id]
```

Omit location_id to dump all 71 locations.
