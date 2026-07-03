# MM1 items, monsters, spells, and events

**Not yet decoded in this repository.** This page records what we know from companion projects and where to start a future trace.

---

## Status summary

| Data | Likely source (DOS) | This repo |
|------|---------------------|-----------|
| **Items** | `ITEMS.DTA` or embedded tables in `MM.EXE` | ❌ no codec |
| **Monsters** | `MONSTERS.DTA` | ❌ no codec |
| **Spells** | `SPELLS.DTA` / exe tables | ❌ no codec |
| **Map events** | Per-map `*.OVR` (code + data) | ❌ not exported |
| **Strings** | `MM.EXE`, `STRINGS.DTA` (variant) | partial — slug table @ `0x10C07` only |
| **Shops / tables** | OVR / exe | ❌ |

For gameplay reference (not binary layouts), the FAQ monster lists and item notes in [`Might and Magic FAQ.txt`](Might%20and%20Magic%20FAQ.txt) cover MM2 primarily; MM1-specific tables are sparse there.

---

## Best external references

| Resource | Use for |
|----------|---------|
| [ScummVM `engines/mm/mm1`](https://github.com/scummvm/scummvm/tree/master/engines/mm/mm1) | Authoritative loaders for monsters, items, spells, map scripts |
| [ScummVM `mm1/datafiles`](https://github.com/scummvm/scummvm/tree/master/dists/engine-data/mm1) | Expected filenames and platform variants |
| lagdotcom RE (`GOG/.../re/lagdotcom/`) | WALLPIX / maze tooling (already used here) |
| RPGClassics / Fandom MM1 tables | Human-readable item/monster lists for validation |

---

## Map events (`*.OVR`)

Each MAZEDATA screen has a companion overlay file (e.g. `SORPIGAL.OVR`, `AREAA1.OVR`). Structure:

- 14-byte header (code size, data size, …)
- **Code segment** — map script bytecode (movement, encounters, doors, text)
- **Data segment** — tables, WALLPIX lane ids (outdoor), exit ids, etc.

ScummVM’s `engines/mm/mm1/maps/` implements the interpreter. We only parse **data-segment** fields needed for outdoor conversion (`tools/mm1_wallpix.py` `parse_ovr_data()`).

**Next trace targets:** event opcode table in OVR code, encounter hooks, item give/take ops — mirror the MM2 [event.dat](06-event-dat-format.md) work once opcode layout is confirmed.

---

## Items & monsters (planned)

When decoding starts, expect parallels with MM2:

| MM2 | MM1 (hypothesis — verify in ScummVM) |
|-----|--------------------------------------|
| `items.dat` 256×20 | `ITEMS.DTA` fixed records |
| `monsters.dat` 256×26 | `MONSTERS.DTA` fixed records |
| `spells.dat` 96×2 | `SPELLS.DTA` or exe-embedded |
| `event.dat` container | per-map `*.OVR` scripts |

Deliverables will follow repo convention: struct in `EXTRACTED/decomp/`, Python + C codec in `tools/`, format doc numbered in `EXTRACTED/docs/`, MM2ED-style editor section if warranted.

---

## See also

- [50-mm1-overview](50-mm1-overview.md) — decoded maze + outdoor work
- [22-mm1-mazedata-format](22-mm1-mazedata-format.md) — geometry only
- [18-items-dat-format](18-items-dat-format.md) — MM2 item layout (comparison baseline)
- [16-monster-ability-format](16-monster-ability-format.md) — MM2 monster layout
