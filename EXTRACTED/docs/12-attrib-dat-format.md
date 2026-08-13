# `attrib.dat` Format

Per-screen map attribute table that parallels `map.dat` (tiles) and supplies
environment, world-adjacency, and roof data for each of the 60 map screens.

- File size: **3840 bytes** = `60 screens * 64 bytes`.
- Record size: **64 bytes** (`0x40`), indexed by `area * 64`.
- Endianness: the few multibyte fields are read byte-wise by the engine; the
  legacy Amiga build is big-endian. The 16-bit "complex id" (`0x15-0x16`) is
  stored big-endian here.

## Sources

- Legacy reverse code: `b3dmm2/mm2ed.bb` `load_map()` (reads `+0x03`, `+0x04`,
  `+0x15`, `+0x20..0x3F`) and `b3dmm2/main.bb` `loadmap()`.
- Disassembly: `EXTRACTED/mm2.capstone.asm` (env/roof use in the screen loader
  and map view).
- Data-driven analysis of all 60 records (adjacency table verified symmetric).

## Record Layout

| Offset | Size | Field | Status | Notes |
|--------|------|-------|--------|-------|
| `0x00` | 1 | `area_id` | confirmed | Always equals the record index (0..59). |
| `0x01` | 1 | `map_category` | confirmed | 1=town/surface-A, 2=cavern/surface-B, 3=dungeon, 4=castle; 5..8 used by elemental planes (41..44). |
| `0x02` | 1 | `tileset_id` | confirmed | Graphics/wall set. Constant per environment: town `0x21`, cavern `0x32`, dungeon `0x53`, castle `0x44`; `0x03/0x04/0x06` for surface, `0x00` for planes. |
| `0x03` | 1 | `env_type` | confirmed | `0x11`=town, `0x12`=cavern, `0x13`/`0x14`=castle, else outside/surface. (Used by `mm2ed.bb`.) |
| `0x04` | 1 | `surface_flag` | confirmed | `0`=interior. Nonzero ⇒ outside/overland; the value is a terrain/biome class (`0xAA`,`0xBB`,`0xCC`,`0x99`...). `mm2ed.bb` treats `>0` as "Outside Area". |
| `0x05` | 1 | `neighbor_0` (N) | confirmed | Adjacent screen id. |
| `0x06` | 1 | `neighbor_1` (E) | confirmed | Adjacent screen id. |
| `0x07` | 1 | `neighbor_2` (S) | confirmed | Adjacent screen id. |
| `0x08` | 1 | `neighbor_3` (W) | confirmed | Adjacent screen id. |
| `0x09` | 1 | `field_09` | observed | Mostly `0x64` (100); `0xC8`/`0x96`/`0xFA`/`0x32` on some. Read at `0x10A2`. |
| `0x0A`..`0x0C` | 3 | params | observed | Compared against runtime values (`0x12124`, `0x11FA2`, `0x11FB2`). |
| `0x0D` | 1 | `field_0D` | observed | Multiples of 10 (30..200). Compared at `0x116CA`/`0x13148`. |
| `0x0E` | 1 | `entry_coord` / in-map safe square | **asm-confirmed** | Packed `(Y<<4)|X` party position. Unpacked at `0x123A`: hi nibble → `-$79F0` (row/Y), lo nibble → `-$79F1` (col/X). This is the screen's spawn point; very likely also the "safe square" the party is moved to on combat *run* / in-map recall. |
| `0x0F` | 1 | `era_gate` | **asm-confirmed** | Compared against the current era index (`-$79B5`, low byte of `-$79B6`) in the **event interpreter** at `0x172BC`; mismatch skips the screen's event record. Usually `0x09`. See `13-time-era-calendar.md`. |
| `0x10` | 1 | pad | confirmed-zero | `0x00` in every record. |
| `0x11` | 1 | sublayout param | observed | Read/compared during screen setup (`0x12F58`). |
| `0x12` | 1 | `door_strength` | **asm-confirmed** | Materialized to `A4-$5608`; consumed by the **bash** handler @ `0x9C2A`. See `MM2_ATTRIB_OFF_DOOR_STRENGTH` / `mm2_attrib_door_strength()` in `mm2_attrib_codec.h`. |
| `0x13` | 1 | `door_trap` | **asm-confirmed** | Materialized to `A4-$5607`; consumed by the **unlock** handler @ `0x20D6E`. Not a second coordinate pair — was mislabeled "sublayout params" in older notes. See `MM2_ATTRIB_OFF_DOOR_TRAP` / `mm2_attrib_door_trap_byte()`. |
| `0x14` | 1 | sublayout param | observed | Read/compared during screen setup (`0x1A8B4`). |
| `0x15` | 1 | label / transition hi | strong | Outside: legacy "Outside Area: XX" label byte. Interior: high byte of the town↔cavern `complex_id`. |
| `0x16` | 1 | `recall_coord` | **asm-confirmed** | Packed `(Y<<4)|X` destination position. Unpacked at `0xB2DC` → `-$79F0`/`-$79F1`, paired with `0x18`. (Forms the low byte of `complex_id` for interiors.) |
| `0x17` | 1 | `level/floor` (interior) | strong | `0x01` for town/upper level, `0x02` for its cavern; higher for deeper castle floors. `0x00` outside. |
| `0x18` | 1 | `recall_screen` | **asm-confirmed** | Destination screen id: copied into the current-screen index `-$79F2` at `0xB2F2` by a command handler (family at `0xA816`–`0xB3B6`) gated by flag bit 6. Interior screens point to their overland location (Surface / Town-Portal style recall); outside: equals own `area_id`. |
| `0x19` | 1 | pad | confirmed-zero | `0x00` in every record. |
| `0x1A` | 1 | `flags` | **asm-confirmed** | Screen behaviour bitfield; bits individually `btst`-tested: bit0 (`0xBCCA`), bit3 (`0xBB98`), bit4 (`0xADE8`), bit5 (`0xB006`), bit6 (`0xB2C2`/`0xAAEE`/`0xB102`). Bit 6 gates the `0x18` transition. Often `0x80` in caverns/castles. |
| `0x1B` | 1 | flags2 | observed | Nonzero on a few screens (`0xB0`,`0xD0`,`0xE0`). |
| `0x1C`..`0x1F` | 4 | tail | mostly-zero | Set only on elemental planes (41..44: `0x1E/0x1F`) and a few others. |
| `0x20`..`0x3F` | 32 | `roof_bits` | confirmed | 256-bit roof bitmap, 1 bit per tile of the 16×16 screen. Bit order: tile `t` ⇒ byte `0x20 + (t>>3)`, bit `t&7`. (`mm2ed.bb` reads LSB-first per byte and reverses tile order.) |

### Two sub-layouts (selected by `surface_flag` at `0x04`)

- **Interior** (`surface_flag == 0`): `env_type` gives town/cavern/castle; the
  four neighbor bytes all equal `area_id` (no overland wrap); `0x15-0x18`
  carry the town/cavern complex linkage and floor.
- **Outside / overland** (`surface_flag != 0`): the four neighbor bytes are the
  real adjacent screen ids; `0x15` is the overland label; `0x18 == area_id`.

## Confirmed: World Adjacency Table (`0x05`–`0x08`)

For overland screens, bytes `0x05..0x08` are the four neighbouring screen ids.
This was verified to be **fully symmetric** across all 60 records: opposite
pairs are `(0x05, 0x07)` and `(0x06, 0x08)`. If screen A names B in slot *s*,
B names A in the opposite slot — 0 asymmetries over the whole file. Interior
screens set all four neighbours to themselves.

The precise compass assignment (which slot is North) is provisional; the proven
fact is the two opposite-direction pairs.

## Runtime field usage (asm-confirmed)

- The whole file is read into a buffer whose pointer is stored at `A4-$79F6`
  (allocated as `0xF00` bytes at `0x2655A`/`0x266AE`; filename string
  `"attrib.dat"` at `0x92E6`).
- On entering a screen, the loader at `0x923E` takes the current screen index
  from `A4-$79F2`, multiplies by 64 (`asl.w #$6`), and copies that 64-byte
  record into the **current-screen attrib buffer at `A4-$561A`**.
- Field reads therefore appear as `A4-$561A + offset`. Mapping:
  `byte 0x05 = -$5615 … byte 0x18 = -$5602 … byte 0x1A = -$5600`,
  roof bytes start at `-$55FA`.

Notable confirmed reads: neighbours `0x05-0x08` drive screen-to-screen
movement (`0x2606`+), `0x0E` sets party entry coords, `0x0F` gates events by
era, `0x16`/`0x18` form a transition (coord + dest screen), and `0x1A` is a
flag bitfield.

### Safe square / recall model (interpretation)

The two coordinate fields line up with MM2's two kinds of "safe square":

- **`0x0E` (in-map safe square):** where the party spawns on the screen, and
  the likely landing square for combat *run* and same-map recall.
- **`0x16` + `0x18` (cross-map recall):** the destination screen + coords used
  by a relocation command handler (`0xB2BE`, gated by flag bit 6). For interior
  screens this points at the screen's overland location, matching the
  Surface (dungeon→overland), Town Portal (→town) and Fly (overland) spells,
  which all teleport the party to a per-map fixed square. Example: Middlegate
  (area 0) and Middlegate Cavern (area 17) both recall to overland screen 11 at
  `(7,3)`.

## Relationship to the time/era system

`attrib.dat` is **static** — one record per screen, identical across all game
eras. The current era/year is runtime world state in the `A4` globals (see
`13-time-era-calendar.md`). The link between the two is **byte `0x0F`**: the
event interpreter compares it against the current era index before running a
screen's event record (`0x172BC`), which is how the same screen can behave
differently across eras. `attrib.dat` itself only describes the screen's fixed
geometry/environment/transition data.

## Codecs

- C: `EXTRACTED/decomp/mm2_attrib_codec.h` / `.c` — byte-exact
  decode/encode/load/save plus named accessors for the confirmed fields.
- Python: `tools/attrib_codec.py` — same field model, round-trips byte-exact.
