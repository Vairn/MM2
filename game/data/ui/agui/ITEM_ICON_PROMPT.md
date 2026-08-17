# Agui item-icon generator prompt

You generate the art. This repo will **not** invent unique item sprites.

Drop finished **12×12** indexed PNGs in [`items/`](items/) as `i01.png` … `iff.png`
(hex id, lowercase). Then:

```powershell
python tools/ingest_agui_item_icons.py --from-dir <your_export_folder>
python tools/pack_agui_ui.py
python tools/ui_pack_preview.py
```

`ingest_agui_item_icons.py` nearest-neighbor-scales **exact 8× / 16×** sources
(96×96 or 192×192) down to 12×12 and snaps to [`palette.json`](palette.json).
Soft / anti-aliased images will look wrong — the packer rejects off-palette pixels.

Attach style refs when the tool allows it: `icons/use.png`, `faces/face_00.png`.

---

## Master prompt (paste every batch)

```
Might and Magic II Amiga inventory icon, 1990 Deluxe Paint pixel art.

TECHNICAL (must obey):
- One item per image.
- Render at 96×96 pixels where EACH logical pixel is a SOLID 8×8 block
  (12×12 logical grid, no exceptions). Equivalent: 12×12 pixel art upscaled 800%
  with nearest-neighbor only.
- Transparent background (not checkerboard, not magenta).
- NO anti-aliasing, NO blur, NO dither, NO gradients, NO glow, NO drop shadows,
  NO outlines that fade, NO JPEG softness, NO photographic texture.
- Hard 1-pixel dark outline on the logical grid. One highlight pixel max.
- Centered, ~10×10 logical pixels of content, 1 logical-pixel margin.
- 3/4 view for weapons (blade up-right). Front-facing for armor, shields, helms.
  Straight-on for potions, keys, jewelry, tickets.
- Chunky 1990s Amiga / Gold Box inventory icon, NOT modern indie pixel art,
  NOT isometric Diablo loot, NOT anime, NOT 32×32 detail crammed into 12.

LOCKED PALETTE — use ONLY these 32 RGB colours (pen 0 = transparent):
#000000  #E0E0C0  #C0A060  #E8D09A  #A0A0A0  #383838  #282828  #181818
#101018  #50A090  #40C040  #C0A020  #C04040  #4080C0  #B0A080  #907050
#604830  #403020  #302418  #201810  #C0A080  #A07858  #785838  #584028
#C8B48C  #64503C  #483828  #305048  #FFFFFF  #000000  #808060  #B06040

There is NO purple/magenta. Magic = cream + blue (#E8D09A + #4080C0).
Fire = #C04040 / #B06040. Ice = #50A090 + #E0E0C0. Acid = #40C040.
Gold = #C0A020. Silver = #A0A0A0. Iron = #383838. Bronze = #B06040.
Wood/leather = #907050 / #A07858.

Readability at 12×12: silhouette first, material second, one elemental accent max.
Each item in the batch must be uniquely identifiable from the others.
```

## Negative prompt

```
anti-aliasing, blur, dither, gradient, glow, bloom, drop shadow, bevel lighting,
photograph, 3D render, isometric, 32x32, 64x64 detail, anime, cute chibi,
modern indie pixel art, watercolor, noise, JPEG artifacts, checkerboard
background, magenta background, purple, violet, pink (except fire highlight),
text, letters, numbers, UI frame, inventory box
```

## Metal / element language (use in the item line)

| Prefix / word | Look |
|---|---|
| I / Iron | dark grey iron |
| B / Bronze | rust bronze |
| S / Silver | pale silver |
| G / Gold | bright gold |
| Fire / Fiery / Flaming / Blazing / Lava | red-orange edge |
| Ice / Cold / Freeze | teal-white |
| Electric / Shock / Voltage / Thunder | blue-white spark (1 pixel) |
| Acid / Acidic | green edge |
| Energy / Photon / Flash / Star | gold-white |
| Magic / Wizard / Sage | cream + blue, no purple |
| Holy / Divine | cream + gold |
| Sleep / Slumber / Quiet | dark teal |

---

## How to batch

Generate **one sprite sheet per batch** OR one image per item.

**Sheet layout (preferred):** 8 columns × N rows. Each cell **96×96**. No labels
inside cells. 0px gutter. Filename `batch_XX.png`. Then:

```powershell
python tools/ingest_agui_item_icons.py --sheet batch_01.png --start 0x01 --cols 8 --cell 96
```

`--start` is the first item id in that sheet, left-to-right, top-to-bottom.
Skip id `0x00` (empty). 255 icons total (`0x01`–`0xFF`).

If the model cannot do a clean sheet, generate one 96×96 image per item named
`i01.png` … `iff.png` and run `--from-dir`.

---

## Batch 01 — mundane 1H (ids 01–18)

```
Sprite sheet, 8 columns × 3 rows, 96×96 cells, 12×12 logical pixels. Row-major:

01 Small Club (wood cudgel)  02 Small Knife  03 Large Club  04 Dagger
05 Large Knife  06 Hand Axe  07 Cudgel  08 Spiked Club
09 Bull Whip  0A Long Dagger  0B Maul  0C Short Sword
0D Nunchakas  0E Mace  0F Spear  10 Cutlass
11 Flail  12 Sabre  13 Long Sword  14 Wakizashi
15 Scimitar  16 Battle Axe  17 Broad Sword  18 Katana

Plain steel / wood, no magic glow. Unique silhouettes per weapon type.
```

## Batch 02 — magic 1H clubs–axes (ids 19–2C)

```
Sprite sheet, 8×3, leftover cells empty transparent. Same grid rules.

19 Slumber Club (dark teal)  1A Power Club (might, thicker)  1B Lucky Knife (gold edge)
1C Looter Knife (thief, dark)  1D Power Cudgel  1E Energy Whip (gold-white)
1F Sonic Whip  20 Mighty Whip  21 Scorch Maul (fire)  22 Mauler Mace
23 Exacto Spear  24 Fiery Spear (fire)  25 Fast Cutlass  26 Quick Flail
27 Shock Flail (elec)  28 Sharp Sabre  29 Ego Scimitar  2A True Axe
2B Blazing Axe (fire)  2C Electric Axe (elec)
```

## Batch 03 — magic 1H swords+ (ids 2D–41)

```
Sprite sheet, 8×3.

2D Rapid Katana  2E Accurate Sword  2F Chance Sword (luck gold)
30 Speedy Sword  31 Flash Sword (energy)  32 Flaming Sword (fire)
33 Electric Sword  34 Acidic Sword (green)  35 Cold Blade (ice)
36 Sage Dagger (blue-cream)  37 Holy Cudgel (gold-cream)  38 Divine Mace
39 Ice Scimitar  3A Grand Axe  3B Swift Axe  3C Dyno Katana (elec)
3D Force Sword  3E Magic Sword (cream-blue)  3F Thunder Sword
40 Energy Blade  41 Photon Blade (bright gold-white, unique)
```

## Batch 04 — two-handed (ids 42–5B)

```
Sprite sheet, 8×4, unused cells empty.

42 Staff  43 Sickle  44 Scythe  45 Glaive
46 War Hammer  47 Trident  48 Pike  49 Naginata
4A Bardiche  4B Great Hammer  4C Halberd  4D Great Axe
4E Flamberge (wavy blade)  4F Wind Staff  50 Tri-Sickle (three blades)
51 Ice Sickle  52 Fire Glaive  53 Harsh Hammer  54 Stone Hammer
55 Genius Staff  56 Wizard Staff  57 Soul Scythe  58 Dark Trident
59 Titan's Pike (huge)  5A Moon Halberd  5B Sun Naginata (gold)
```

## Batch 05 — missiles (ids 5C–6E)

```
Sprite sheet, 8×2.

5C Blowpipe  5D Sling  5E Short Bow  5F Crossbow
60 Long Bow  61 Great Bow  62 Shaman Pipe  63 Cinder Pipe (fire)
64 Quiet Sling  65 Pirate Crossbow  66 Burning Crossbow  67 Fireball Bow
68 Voltage Bow  69 Giant Sling  6A Energy Sling  6B Death Bow (dark)
6C Star Bow  6D Meteor Bow  6E Ancient Bow (ornate gold)
```

## Batch 06 — keys + shields (ids 6F–7E)

```
Sprite sheet, 8×2.

6F Green Key (#40C040)  70 Yellow Key (#C0A020)  71 Red Key (#C04040)
72 Black Key (#181818)  73 Small Shield  74 Large Shield  75 Great Shield
76 Fire Shield  77 Electric Shield  78 Acid Shield  79 Cold Shield
7A Silver Shield  7B Bronze Shield  7C Iron Shield  7D Magic Shield
7E Gold Shield
```

## Batch 07 — armor (ids 7F–9A)

```
Sprite sheet, 8×4.

7F Padded Armor (cloth)  80 Leather Suit  81 Scale Armor  82 Ring Mail
83 Chain Mail  84 Splint Mail  85 Plate Mail  86 Plate Armor (full)
87 Iron Scale  88 Bronze Scale  89 Silver Scale
8A Iron Ring  8B Bronze Ring  8C Silver Ring
8D Iron Chain  8E Bronze Chain  8F Silver Chain
90 Iron Splint  91 Bronze Splint  92 Silver Splint
93 Iron Plate  94 Bronze Plate  95 Silver Plate
96 Gold Scale  97 Gold Ring Mail  98 Gold Chain  99 Gold Splint
9A Gold Plate Mail (richest)
```

Torso cuirass icons, not a full person. Metal prefix recolors the same armor
silhouette so Iron/Bronze/Silver/Gold of one type stay family-similar.

## Batch 08 — helms (ids 9B–9F)

```
Five 96×96 images (or 8×1 sheet).

9B Helm (plain steel)  9C Iron Helm  9D Bronze Helm  9E Silver Helm  9F Gold Helm
```

## Batch 09 — use items (ids A0–BB)

```
Sprite sheet, 8×4.

A0 Magic Herbs  A1 Torch  A2 Lantern  A3 Thief's Pick
A4 Rope'n Hooks  A5 Wakeup Horn  A6 Compass  A7 Sextant
A8 Force Potion (red vial)  A9 Skill Potion (blue)  AA MaxHP Potion (green)
AB Holy Charm  AC Herbal Patch  AD Hero Medal  AE Silent Horn
AF Magic Meal  B0 Antidote Ale  B1 Super Flare  B2 Dove's Blood (vial)
B3 Ray Gun (sci-fi, still 12×12 chunky)  B4 Magic Charm  B5 Witch Broom
B6 Invisocloak  B7 Storm Wand  B8 Lava Grenade  B9 Hourglass
BA Instant Keep (tiny castle)  BB Teleport Orb
```

## Batch 10 — gear / wands (ids BC–CF)

```
Sprite sheet, 8×3.

BC Skeleton Key  BD Defense Ring  BE Might Gauntlet  BF Accuracy Gauntlet
C0 Stealth Cape  C1 Admit 8 Pass (ticket)  C2 Speed Boots  C3 Cure-all Wand
C4 Moon Rock  C5 Ruby Ankh  C6 Disruptor  C7 Lich Hand
C8 Phaser  C9 Freeze Wand  CA Energizer  CB Magic Mirror
CC Elven Cloak  CD Elven Boots  CE Sage Robe  CF Enchanted Idol
```

## Batch 11 — quest (ids D0–EA)

```
Sprite sheet, 8×4.

D0 Green Ticket  D1 Yellow Ticket  D2 Red Ticket  D3 Black Ticket
D4 Fe Farthing (coin)  D5 Castle Key  D6 Mark's Keys  D7 Dog Whistle
D8 Web Caster  D9 Monster Tome  DA Cupie Doll (tiny doll, not a UI dummy)
DB Water Talon  DC Air Talon  DD Fire Talon  DE Earth Talon
DF Element Orb  E0 Gold Goblet  E1 +7 Loincloth  E2 Valor Sword
E3 Honor Sword  E4 Noble Sword  E5 Corak's Soul (crystal)  E6 Emerald Ring
E7 Water Disc  E8 Air Disc  E9 Fire Disc  EA Earth Disc
```

## Batch 12 — class relics + Cron (ids EB–FF)

```
Sprite sheet, 8×3.

EB Sapphire Pin  EC Amethyst Box (use blue+cream, no purple)  ED Coral Broach
EE Lapis Scarab  EF Amber Skull  F0 Quartz Skull  F1 Agate Grail
F2 Opal Pendant  F3 Crystal Vial  F4 Ruby Amulet  F5 Ivory Cameo
F6 Ruby Tiara  F7 Onyx Effigy  F8 Pearl Choker  F9 Topaz Shard
FA Sun Crown  FB J-26 Fluxer (sci-fi widget)  FC M-27 Radicon
FD A-1 Todilor  FE N-19 Capitor  FF Useless Item (bent stick / junk)
```

---

## Review before packing

1. Open an icon at **1:1** (12×12) and at **8× nearest-neighbor**. If edges look
   soft at 1:1, regenerate that item.
2. Count distinct colours in one icon — should be a handful of palette pens, not 200.
3. Run ingest + `pack_agui_ui.py`. Off-palette pixels fail the pack (`--max-off-palette 0`).
