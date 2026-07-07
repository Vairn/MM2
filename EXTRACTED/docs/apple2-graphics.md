# Apple II graphics (Might and Magic II)

Initial RE notes for the **1988 Apple IIe** port. Confirmed facts only; gaps explicitly marked.

| Artifact | Path |
|----------|------|
| WOZ source | `EXTRACTED/apple/Might and Magic Book Two (woz-a-day collection)/*.woz` |
| Decoded DSK | `EXTRACTED/apple2/disks/disk_{a..f}.dsk` |
| Loader map | `EXTRACTED/apple2/loader_map.json` |
| Loader scan | `EXTRACTED/apple2/loader_scan.json` |
| Boot / track disasm | `EXTRACTED/apple2/asm/` |
| Investigation | `EXTRACTED/disk_investigation/protection_investigation.json` |
| PC reference | `EXTRACTED/docs/54-pc-dos-graphics-formats.md`, `tools/decode_pc_gfx.py` |
| Amiga reference | `EXTRACTED/docs/06-gfx-loading.md` (`.32` planar sheets) |

---

## Disk provenance (confirmed 2026-07)

| Check | Result |
|-------|--------|
| Source | **woz-a-day** preservation set — Applesauce v1.0.5 captures, dated **2018-08-24** |
| WOZ META | `publisher=New World Computing`, `copyright=1988`, `requires_ram=128K`, `requires_machine=2e\|2e+\|2c\|2gs` |
| Crack / trainer strings | **None** — searched all six `.woz` and decoded `.dsk` for `CRACKED`, `TRAINER`, scene group names, etc. |
| Assessment | **Original retail layout**, not a crack. Cracked Apple images typically *remove* quarter-track data; these retain it on **all sides A–F**. |

**Gap:** We do not have a second source (e.g. bare `.dsk` from another dump) to cross-check, but the woz-a-day metadata and absence of crack signatures argue against a scene release.

---

## Display hardware (confirmed)

- WOZ **META** chunk: `requires_machine=2e|2e+|2c|2gs`, `requires_ram=128K`.
- External references agree: **double hi-res (DHGR)** mode for first-person view (560×192 effective b/w or 16 colors with artifacting depending on setup).

**Not** the same on-disk format as:

- Amiga `.32` TV/planar sheets (`frame_count` + offset table — see `06-gfx-loading.md`).
- PC `.4` / `.16` LZW containers (`54-pc-dos-graphics-formats.md`).

---

## Disk / file layer (confirmed)

| Item | Status |
|------|--------|
| WOZ2 6-and-2 decode | **OK** — 0 missing sectors all sides (`nibbler dsk`) |
| DOS 3.3 VTOC @ T17S0 | **Non-standard** on **all sides** — disk A T17S0 bytes `00 00 6E 03 0E …`; not a parseable VTOC |
| Catalog file extract | **Not done** — `read_catalog()` fails on **A–F** (`index out of range`) |
| Layout | **Custom NWC filesystem** — publisher boot loader, not a DOS 3.3 catalog |
| Anti-copy measure | **Quarter-track data** on **all six sides** (`nibbler protect`); standard DOS catalog absent |
| DHGR setup code in DSK | **Disk A only** — `STA $C050/$C054/$C05E` at T00S8, T30S9 |

**Correction (2026-07):** Prior notes treated the non-DOS layout as generic “copy protection.” Re-investigation shows this is the **original retail disk format** (woz-a-day captures with quarter-tracks intact). The blocker for file extraction is the **custom loader + quarter-track stage-2**, not a crack patch or missing catalog on data sides.

## Boot loader chain (disk A — partial, confirmed)

### Stage 1 — T00S0 @ `$0800`

Sector loads at **`$0800`**; P6 ROM entry **`$0801`**. Full listing: `asm/disk_a_t00_s00.asm`.

| Step | Behavior | Evidence |
|------|----------|----------|
| Slot vector | When `$27==$09`: `$3E=$5C`, `$3F=(($2B>>4)\|$C0)` | Bytes @ `$0808`–`$0814` |
| Sector loop | Index `$4C08` … `$08`; each iter reads table entries `$4F08,X` / `$4D08,X` | `$0816`–`$082A` |
| Per-sector dispatch | **`JMP ($3E00)`** | Byte triplet `6C 3E 00` @ DSK+`0x2E` |
| Slot-6 target | **`$C65C`** (`$3E=$5C`, `$3F=$C6` with `$2B=$60`) | Traced from boot bytes |
| After 8 loads | **`JMP $610C`** | `4C 61 0C` @ `$0832` |
| Alt vectors | `$5F08`, `$D30B`, `$1E0F` | Jump table @ `$0835`–`$083B` |

**Track 0** is almost entirely 6502 code (14/16 sectors pass opcode heuristic). Listings: `asm/disk_a_t00_s*.asm`.

### DHGR mode setup (confirmed code, not gfx data)

| Location | DSK offset | Listing | Evidence |
|----------|------------|---------|----------|
| T00S8 | `$0BC8` | `asm/disk_a_t00_s08_b800.asm` | `STA $C050`, `$C054`, `$C05E`, `$C052` @ `$B8C8` when counter `$4608==2` |
| T30S9 | `$1E3B2` | `asm/disk_a_t30_s09.asm` | Same soft-switch cluster + aux/main copy (`LDA ($FC),Y` / `STA ($FE),Y`) |

Disks **B–F**: **zero** `STA $C05x` hits in `loader_scan.json` — DHGR init appears confined to **disk A** loader.

### `nibbler boot` simulation (confirmed result)

```powershell
python -m nibbler boot "EXTRACTED\apple\Might and Magic Book Two (woz-a-day collection)\Might and Magic Book Two disk A.woz"
```

| Result | Value |
|--------|-------|
| Instructions executed | **16** |
| Stop reason | **halt** (BRK vector → `$0003`) |
| Final PC | `$0003` |
| RAM populated | Page `$08` only (boot sector) |
| Snapshot | `EXTRACTED/apple2/boot_halt_ram.bin` |

**Cause (confirmed):** Stage-1 reaches `JMP ($3E00)` → `$C65C`, but stage-2 body is **not present** in the minimal P6 ROM environment. The in-sector routine at `$088C` calls **`JSR $000D`** (nibble read helper) which is not installed by `nibbler boot`.

**Gap:** Full loader RAM map (`$610C` body, gfx decode, DHGR page fill) requires either quarter-track–aware emulation or manual sector→address trace through the T00 6-and-2 decode routines (T00S1/S2).

---

## Filename / stem scans (confirmed — not gfx files)

`tools/extract_apple2_woz.py` scans decoded DSK for Amiga gfx stems. Hits are **in-game text**, not loadable filenames:

| Disk | Example hit | Context |
|------|-------------|---------|
| A | `TOWN` @ `$1A533` | Menu string `"G - GO TO TOWNS"` |
| B–F | `CASTLE`, `CAVE`, `MONSTER`, … | Quest/event prose, combat messages |

**Conclusion:** ASCII stem search alone does **not** locate gfx blobs on Apple II.

---

## Graphics format search (confirmed negative)

| Check | Result |
|-------|--------|
| Amiga `castle.32` first 8 bytes (`00 20 00 03 00 A0 00 5C`) | **Not found** in any DSK |
| Amiga file sizes as LE u32/u16 (`57098` etc.) | **No meaningful clusters** |
| PC LZW `uncompressed_size` header | **Not found** |
| DHGR screen pages `$2000–$3FFF` after boot sim | **All zero** |
| High-entropy sectors (entropy ≥ 7.15) | Disk A only: T34S5, T00S3/S6 — **unclassified** (saved to `gfx/candidates/`) |

**Gap:** No round-trip gfx decode. Candidate sectors are entropy outliers only — **not confirmed** as DHGR bitmaps or compressed art.

---

## Code vs data sector counts (`apple2_scan_loaders.py`)

| Disk | Code-like sectors | Data-like sectors |
|------|-------------------|-------------------|
| A | 216 | 344 |
| B | 193 | 367 |
| C | 193 | 367 |
| D | 111 | 449 |
| E | 210 | 350 |
| F | 139 | 421 |

Disk **D** has the most data-heavy layout — plausible gfx/event payload disk, but **no format signature** yet.

---

## Recommended next steps

1. **Trace T00S1 6-and-2 decode** (`asm/disk_a_t00_s01.asm`) to map which tracks/sectors stage-1 loads before `$610C`.
2. **Quarter-track reads** — `nibbler protect` reports non-standard track positions; may be required for stage-2 @ `$C65C`.
3. **Full emulator** (MAME `apple2e`, Linapple) with 128K + Disk II — dump RAM at `$2000–$4000` and `$C600+` after boot settles.
4. Once a loaded blob is captured, compare size/entropy to Amiga `castle.32` (57,098 bytes) and PC `CASTLE.4`.
5. Scan disk D contiguous data runs for custom packer headers (after loader addresses `$610C` / `$4D0D` family are found).

---

## Regenerate artifacts

```powershell
pip install git+https://github.com/BrentRector/orchard.git
python tools\extract_apple2_woz.py EXTRACTED\apple -o EXTRACTED\apple2

# Sector dump + disasm
python tools\apple2_dump_sector.py EXTRACTED\apple2\disks\disk_a.dsk 0 --all -o EXTRACTED\apple2\asm
python tools\apple2_disasm_track.py EXTRACTED\apple2\disks\disk_a.dsk 0 --org 0x801 -o EXTRACTED\apple2\asm

# Loader / DHGR scan (all sides)
python tools\apple2_scan_loaders.py EXTRACTED\apple2\disks\disk_*.dsk -o EXTRACTED\apple2\loader_scan.json

# Boot simulation (expect early halt)
python -m nibbler boot "EXTRACTED\apple\Might and Magic Book Two (woz-a-day collection)\Might and Magic Book Two disk A.woz" --save EXTRACTED\apple2\boot_halt_ram.bin
```
