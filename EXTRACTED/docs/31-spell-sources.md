# MM2 spell sources (master index)

All **96** `spells.dat` records (48 sorcerer + 48 cleric): where each spell
can be obtained in retail data, in plain-language **Acquisition** notes.

Related docs:

- [19-spells-and-item-use.md](19-spells-and-item-use.md) — `spells.dat` flags, item use-byte encoding
- [28-town-services.md](28-town-services.md) — temple / mage guild / training handlers
- [18-items-dat-format.md](18-items-dat-format.md) — item effect byte @ `0x0F`
- [07-event-script-opcodes.md](07-event-script-opcodes.md) — `OP_2E` / `OP_1F` party effects, HoS hints
- Machine-readable shops: [`../shop_tables.json`](../shop_tables.json) (`tools/dump_shop_tables.py`)
- Player FAQ cross-check: [`Might and Magic FAQ.txt`](Might%20and%20Magic%20FAQ.txt) §3-2-2 (WHERE TO FIND THEM)

Overland **sector tiles** use FAQ notation **(column, row)** — e.g. C3 (1,9) north of Middlegate
ferry line. Guild/temple prices below come from retail data tables, not FAQ dollar hints.

## Acquisition types

| Type | Meaning |
|------|---------|
| **Learn** | Gained by leveling the matching spellcasting class (spell level on roster); assumed when not listed in static shop/item/world traces below. |
| **Bought** | Temple cleric (**D/E/F**) or mage guild (**A/B/C/D**) — guild requires membership. |
| **Item** | Cast/teach via `items.dat` use-power (`0x0F`); one charge per use, not permanent spellbook unless noted in-game. |
| **World** | Overland return tile, dungeon quest, or `exec_selector` script (`event.dat`); **not** the elemental-plane map screens 41–44 themselves for encase/transmute grants. |

Cast restrictions (combat / outdoor / dungeon) come from `spells.dat` only — they are
not acquisition paths. **Unknown** would mean no static trace; this doc marks those as
**Learn (assumed)** for the spell's school and level.

## Temple cleric stock (per town)

Temple **`OP_0E` `0x03`** sells **three cleric spells per town** only (`A4-$66F6`, handler **`0x1DAC6`**, loop **`cmpi #3`** @ `0x1DF1A`). Menu keys **`D` / `E` / `F`**. **No sorcerer/mage spells at temple.**

- **Middlegate:** D: C1/1 Apparition, E: C1/2 Awaken, F: C1/6 Power Cure
- **Atlantium:** D: C8/3 Mass Distortion, E: C9/3 Resurrection, F: C9/4 Uncurse Item
- **Tundara:** D: C3/1 Cold Ray, E: C3/5 Lasting Light, F: C4/4 Restore Alignment
- **Vulcania:** D: C4/6 Holy Bonus, E: C5/5 Remove Condition, F: C7/2 Fiery Flail
- **Sandsobar:** D: C2/2 Heroism, E: C2/5 Protection From Elements, F: C2/7 Weaken

## Mage guild (per town)

Guild **`OP_0E` `0x05`** sells **four sorcerer spells per town** (`A4-$66E2[town×4+slot]`, handler **`0x1D97A`**, loop **`cmpi #4`** @ `0x1E64A`). Gold = **`A4-$6698[town×4+slot] + A4-$6686[slot]`**. Menu keys **`A` / `B` / `C` / `D`** (dispatch sub **`#$41`** @ `0x1E864`). Grant @ **`0x1D872`** sets roster spellbook bit. **No cleric spells.**

### Middlegate

| Key | Spell | GP |
|-----|-------|-----|
| A | S1/1 Awaken | 196 |
| B | S1/3 Energy Blast | 166 |
| C | S1/7 Sleep | 142 |
| D | S2/3 Identify Monster | 251 |

### Atlantium

| Key | Spell | GP |
|-----|-------|-----|
| A | S8/2 Mega Volts | 243 |
| B | S8/3 Meteor Shower | 182 |
| C | S9/1 Implosion | 77 |
| D | S9/2 Inferno | 218 |

### Tundara

| Key | Spell | GP |
|-----|-------|-----|
| A | S4/2 Feeble Mind | 252 |
| B | S4/3 Fire Ball | 311 |
| C | S5/1 Disrupt | 192 |
| D | S5/3 Sand Storm | 271 |

### Vulcania

| Key | Spell | GP |
|-----|-------|-----|
| A | S6/1 Disintegration | 390 |
| B | S6/3 Fantastic Freeze | 322 |
| C | S6/5 Super Shock | 221 |
| D | S7/2 Duplication | 202 |

### Sandsobar

| Key | Spell | GP |
|-----|-------|-----|
| A | S2/7 Protection from Magic | 188 |
| B | S3/1 Acid Stream | 126 |
| C | S3/4 Lightning Bolt | 73 |
| D | S4/1 Cold Beam | 210 |

## Sorcerer spells

| Lv | # | Name | Cast | Acquisition |
|----|---|------|------|-------------|
| 1 | 1 | Awaken | Anytime | Bought: Mage guild Middlegate (key A); Item: Wakeup Horn |
| 1 | 2 | Detect Magic | Non-combat only | Learn: Sorcerer level 1 (assumed) |
| 1 | 3 | Energy Blast | Combat only | Bought: Mage guild Middlegate (key B); Item: Energy Sling; Item: Energy Whip; Item: Ray Gun |
| 1 | 4 | Flame Arrow | Combat only | Learn: Sorcerer level 1 (assumed) |
| 1 | 5 | Light | Non-combat only | Item: Lantern; Item: Torch |
| 1 | 6 | Location | Non-combat only | Item: Compass; Item: Sextant |
| 1 | 7 | Sleep | Combat only | Bought: Mage guild Middlegate (key C); Item: Slumber Club |
| 2 | 1 | Eagle Eye | Non-combat only; Outdoor | World: Nordon quest — Eagle Eye (event loc 60; overland C2 (10,2); retrieve goblet from cave below) |
| 2 | 2 | Electric Arrow | Combat only | Item: Shock Flail |
| 2 | 3 | Identify Monster | Combat only | Bought: Mage guild Middlegate (key D); Item: Monster Tome |
| 2 | 4 | Jump | Non-combat only | Item: Rope'n'Hooks |
| 2 | 5 | Levitate | Non-combat only | Learn: Sorcerer level 2 (assumed) |
| 2 | 6 | Lloyd's Beacon | Non-combat only | World: Corak's Cave (map 22), tile (11,7) — Lloyd's Beacon behind fake walls (event loc 22) |
| 2 | 7 | Protection from Magic | Anytime | Bought: Mage guild Sandsobar (key A); Item: Magic Charm |
| 3 | 1 | Acid Stream | Combat only | Bought: Mage guild Sandsobar (key B); Item: Acidic Sword |
| 3 | 2 | Fly | Non-combat only; Outdoor | Item: Witch Broom |
| 3 | 3 | Invisibility | Combat only | Item: Elven Cloak; Item: Invisocloak |
| 3 | 4 | Lightning Bolt | Combat only | Bought: Mage guild Sandsobar (key C); Item: Electric Axe; Item: Flash Sword; Item: Storm Wand; Item: Thunder Swd; Item: Voltage Bow |
| 3 | 5 | Web | Combat only | Item: Web Caster |
| 3 | 6 | Wizard Eye | Non-combat only | World: Sandsobar (map 4), tile (7,4) — beggar at "The Beggar's Gift" (door signs evt 41/42); event loc 4 evt 43, exec_selector 0x51, 100 gp (FAQ §3-2-2, §4-3) |
| 4 | 1 | Cold Beam | Combat only | Bought: Mage guild Sandsobar (key D); Item: Cold Blade; Item: Ice Sickle |
| 4 | 2 | Feeble Mind | Combat only | Bought: Mage guild Tundara (key A) |
| 4 | 3 | Fire Ball | Combat only | Bought: Mage guild Tundara (key B); Item: Cinder Pipe; Item: Fiery Spear; Item: Fire Glaive; Item: Fireball Bow; Item: Flaming Swd; Item: Lava Grenade |
| 4 | 4 | Guard Dog | Non-combat only | Item: Dog Whistle |
| 4 | 5 | Shield | Combat only | Item: Defense Ring |
| 4 | 6 | Time Distortion | Combat only | Item: Hourglass |
| 5 | 1 | Disrupt | Combat only | Bought: Mage guild Tundara (key C); Item: Disruptor |
| 5 | 2 | Fingers of Death | Combat only | Item: Lich Hand; Item: Soul Scythe; World: Overland C1 (map 7), tile (1,8) — Fingers of Death scroll; eat Devil's Food Brownie, face east (event loc 7) |
| 5 | 3 | Sand Storm | Combat only; Outdoor | Bought: Mage guild Tundara (key D) |
| 5 | 4 | Shelter | Non-combat only | Item: Instant Keep |
| 5 | 5 | Teleport | Non-combat only | Item: Teleport Orb |
| 6 | 1 | Disintegration | Combat only | Bought: Mage guild Vulcania (key A); Item: Energy Blade; Item: Phaser |
| 6 | 2 | Entrapment | Combat only | Learn: Sorcerer level 6 (assumed) |
| 6 | 3 | Fantastic Freeze | Combat only | Bought: Mage guild Vulcania (key B); Item: Freeze Wand; Item: Ice Scimitar |
| 6 | 4 | Recharge Item | Non-combat only | Item: Energizer |
| 6 | 5 | Super Shock | Combat only | Bought: Mage guild Vulcania (key C); Item: Electric Swd |
| 7 | 1 | Dancing Sword | Combat only | World: Overland A2 (map 9), tile (15,11) — Mist Warrior (Dancing Sword; event loc 9, tile (11,15)) |
| 7 | 2 | Duplication | Non-combat only | Bought: Mage guild Vulcania (key D); Item: Magic Mirror |
| 7 | 3 | Etherealize | Non-combat only | Learn: Sorcerer level 7 (assumed) |
| 7 | 4 | Prismatic Light | Combat only | Item: Wizard Staff |
| 8 | 1 | Incinerate | Combat only | Learn: Sorcerer level 8 (assumed) |
| 8 | 2 | Mega Volts | Combat only | Bought: Mage guild Atlantium (key A) |
| 8 | 3 | Meteor Shower | Combat only; Outdoor | Bought: Mage guild Atlantium (key B); Item: Meteor Bow |
| 8 | 4 | Power Shield | Combat only | Learn: Sorcerer level 8 (assumed) |
| 9 | 1 | Implosion | Combat only | Bought: Mage guild Atlantium (key C); Item: Photon Blade |
| 9 | 2 | Inferno | Combat only | Bought: Mage guild Atlantium (key D) |
| 9 | 3 | Star Burst | Combat only; Outdoor | Item: Element Orb; Item: Star Bow; World: Overland D1 (map 8), tile (5,6) — Star Burst in Dead Zone (event loc 8, tile (6,5); FAQ: Nature's Gate on day 93) |
| 9 | 4 | Enchant Item | Non-combat only | World: Gemmaker's Cave (event loc 31, tile (3,3)) |

## Cleric spells

| Lv | # | Name | Cast | Acquisition |
|----|---|------|------|-------------|
| 1 | 1 | Apparition | Combat only | Bought: Temple Middlegate (cleric D) |
| 1 | 2 | Awaken | Anytime | Bought: Temple Middlegate (cleric E) |
| 1 | 3 | Bless | Combat only | Learn: Cleric level 1 (assumed) |
| 1 | 4 | First Aid | Anytime | Item: Magic Herbs |
| 1 | 5 | Light | Non-combat only | Learn: Cleric level 1 (assumed) |
| 1 | 6 | Power Cure | Anytime | Bought: Temple Middlegate (cleric F) |
| 1 | 7 | Turn Undead | Combat only | Item: Holy Charm |
| 2 | 1 | Cure Wounds | Anytime | Item: Herbal Patch |
| 2 | 2 | Heroism | Combat only | Bought: Temple Sandsobar (cleric D); Item: Hero Medal |
| 2 | 3 | Nature's Gate | Non-combat only; Outdoor | World: Overland C3 (map 14), tile (1,9) — Nature's Gate after Red Hot Wolf Nipple Chips (event loc 14, tile (9,1); exec_selector 0x59; strings event loc 64) |
| 2 | 4 | Pain | Combat only | Item: Sonic Whip |
| 2 | 5 | Protection From Elements | Anytime | Bought: Temple Sandsobar (cleric E) |
| 2 | 6 | Silence | Combat only | Item: Quiet Sling; Item: Silent Horn |
| 2 | 7 | Weaken | Combat only | Bought: Temple Sandsobar (cleric F) |
| 3 | 1 | Cold Ray | Combat only | Bought: Temple Tundara (cleric D) |
| 3 | 2 | Create Food | Non-combat only | Item: Magic Meal |
| 3 | 3 | Cure Poison | Anytime | Item: Antidote Ale |
| 3 | 4 | Immobilize | Combat only | Learn: Cleric level 3 (assumed) |
| 3 | 5 | Lasting Light | Non-combat only | Bought: Temple Tundara (cleric E); Item: Burning xBow; Item: Super Flare |
| 3 | 6 | Walk on Water | Non-combat only; Outdoor | World: Overland C2 (map 11), tile (11,1) — Walk on Water for 50 gp (event loc 11, tile (1,11)) |
| 4 | 1 | Acid Spray | Combat only | Learn: Cleric level 4 (assumed) |
| 4 | 2 | Air Transmutation | Non-combat only; Outdoor | Item: Air Disc; World: Overland A1 (map 5), tile (8,8) — Air Transmutation scroll (event loc 5; OP_2E) |
| 4 | 3 | Cure Disease | Anytime | Item: Dove's Blood |
| 4 | 4 | Restore Alignment | Non-combat only | Bought: Temple Tundara (cleric F) |
| 4 | 5 | Surface | Non-combat only | Learn: Cleric level 4 (assumed) |
| 4 | 6 | Holy Bonus | Combat only | Bought: Temple Vulcania (cleric D) |
| 5 | 1 | Air Encasement | Combat only | Item: Air Talon; Item: Wind Staff; World: Overland A1 (map 5), tile (1,14) — Air Encasement after Elemental Plane of Air (event loc 5, tile (14,1); OP_2E) |
| 5 | 2 | Deadly Swarm | Combat only | Learn: Cleric level 5 (assumed) |
| 5 | 3 | Frenzy | Combat only | Item: Speed Boots; World: Murray's Cave (map 26), tile (1,8) — Frenzy after 15 Crazed Natives (event loc 16; FAQ §3-2-2 C5-3) |
| 5 | 4 | Paralyze | Combat only | Learn: Cleric level 5 (assumed) |
| 5 | 5 | Remove Condition | Anytime | Bought: Temple Vulcania (cleric E); Item: Cureall Wand |
| 6 | 1 | Earth Transmutation | Non-combat only; Outdoor | Item: Earth Disc; World: Overland E4 (map 40), tile (8,8) — Earth Transmutation tablet (event loc 40; OP_2E) |
| 6 | 2 | Rejuvenate | Non-combat only | Learn: Cleric level 6 (assumed) |
| 6 | 3 | Stone to Flesh | Anytime | Learn: Cleric level 6 (assumed) |
| 6 | 4 | Water Encasement | Combat only | Item: Water Talon; World: Overland A4 (map 15), tile (1,1) — Water Encasement after Elemental Plane of Water (event loc 15; OP_2E) |
| 6 | 5 | Water Transmutation | Non-combat only; Outdoor | Item: Water Disc; World: Overland A4 (map 15), tile (8,8) — Water Transmutation scroll (event loc 15; OP_2E) |
| 7 | 1 | Earth Encasement | Combat only | Item: Earth Talon; World: Overland E4 (map 40), tile (14,1) — Earth Encasement after Elemental Plane of Earth (event loc 40, tile (1,14); OP_2E) |
| 7 | 2 | Fiery Flail | Combat only | Bought: Temple Vulcania (cleric F) |
| 7 | 3 | Moon Ray | Combat only; Outdoor | Item: Moon Halberd; Item: Moon Rock |
| 7 | 4 | Raise Dead | Anytime | Item: Ruby Ankh |
| 8 | 1 | Fire Encasement | Combat only | Item: Fire Talon; World: Overland E1 (map 33), tile (14,14) — Fire Encasement after Elemental Plane of Fire (event loc 33; OP_2E) |
| 8 | 2 | Fire Transmutation | Non-combat only; Outdoor | Item: Fire Disc; World: Overland E1 (map 33), tile (8,8) — Fire Transmutation plaque (event loc 33; OP_2E) |
| 8 | 3 | Mass Distortion | Combat only | Bought: Temple Atlantium (cleric D) |
| 8 | 4 | Town Portal | Non-combat only | Learn: Cleric level 8 (assumed) |
| 9 | 1 | Divine Intervention | Combat only | Item: Divine Mace; World: Druid's Cave (map 27), tile (15,14) — Elder Druid quest vs Horvath (exec_selector 0x67; strings event loc 66) |
| 9 | 2 | Holy Word | Combat only | Item: Holy Cudgel; World: Overland C1 (map 7), tile (5,5) — Holy Word carved on tree, face south (event loc 7; FAQ §3-2-2) |
| 9 | 3 | Resurrection | Non-combat only | Bought: Temple Atlantium (cleric E) |
| 9 | 4 | Uncurse Item | Non-combat only | Bought: Temple Atlantium (cleric F) |

## Items that teach/cast spells

| Item | Spell |
|------|-------|
| Wakeup Horn | S1/1 Awaken |
| Energy Sling | S1/3 Energy Blast |
| Energy Whip | S1/3 Energy Blast |
| Ray Gun | S1/3 Energy Blast |
| Lantern | S1/5 Light |
| Torch | S1/5 Light |
| Compass | S1/6 Location |
| Sextant | S1/6 Location |
| Slumber Club | S1/7 Sleep |
| Shock Flail | S2/2 Electric Arrow |
| Monster Tome | S2/3 Identify Monster |
| Rope'n'Hooks | S2/4 Jump |
| Magic Charm | S2/7 Protection from Magic |
| Acidic Sword | S3/1 Acid Stream |
| Witch Broom | S3/2 Fly |
| Elven Cloak | S3/3 Invisibility |
| Invisocloak | S3/3 Invisibility |
| Electric Axe | S3/4 Lightning Bolt |
| Flash Sword | S3/4 Lightning Bolt |
| Storm Wand | S3/4 Lightning Bolt |
| Thunder Swd | S3/4 Lightning Bolt |
| Voltage Bow | S3/4 Lightning Bolt |
| Web Caster | S3/5 Web |
| Cold Blade | S4/1 Cold Beam |
| Ice Sickle | S4/1 Cold Beam |
| Cinder Pipe | S4/3 Fire Ball |
| Fiery Spear | S4/3 Fire Ball |
| Fire Glaive | S4/3 Fire Ball |
| Fireball Bow | S4/3 Fire Ball |
| Flaming Swd | S4/3 Fire Ball |
| Lava Grenade | S4/3 Fire Ball |
| Dog Whistle | S4/4 Guard Dog |
| Defense Ring | S4/5 Shield |
| Hourglass | S4/6 Time Distortion |
| Disruptor | S5/1 Disrupt |
| Lich Hand | S5/2 Fingers of Death |
| Soul Scythe | S5/2 Fingers of Death |
| Instant Keep | S5/4 Shelter |
| Teleport Orb | S5/5 Teleport |
| Energy Blade | S6/1 Disintegration |
| Phaser | S6/1 Disintegration |
| Freeze Wand | S6/3 Fantastic Freeze |
| Ice Scimitar | S6/3 Fantastic Freeze |
| Energizer | S6/4 Recharge Item |
| Electric Swd | S6/5 Super Shock |
| Magic Mirror | S7/2 Duplication |
| Wizard Staff | S7/4 Prismatic Light |
| Meteor Bow | S8/3 Meteor Shower |
| Photon Blade | S9/1 Implosion |
| Element Orb | S9/3 Star Burst |
| Star Bow | S9/3 Star Burst |
| Magic Herbs | C1/4 First Aid |
| Holy Charm | C1/7 Turn Undead |
| Herbal Patch | C2/1 Cure Wounds |
| Hero Medal | C2/2 Heroism |
| Sonic Whip | C2/4 Pain |
| Quiet Sling | C2/6 Silence |
| Silent Horn | C2/6 Silence |
| Magic Meal | C3/2 Create Food |
| Antidote Ale | C3/3 Cure Poison |
| Burning xBow | C3/5 Lasting Light |
| Super Flare | C3/5 Lasting Light |
| Air Disc | C4/2 Air Transmutation |
| Dove's Blood | C4/3 Cure Disease |
| Air Talon | C5/1 Air Encasement |
| Wind Staff | C5/1 Air Encasement |
| Speed Boots | C5/3 Frenzy |
| Cureall Wand | C5/5 Remove Condition |
| Earth Disc | C6/1 Earth Transmutation |
| Water Talon | C6/4 Water Encasement |
| Water Disc | C6/5 Water Transmutation |
| Earth Talon | C7/1 Earth Encasement |
| Moon Halberd | C7/3 Moon Ray |
| Moon Rock | C7/3 Moon Ray |
| Ruby Ankh | C7/4 Raise Dead |
| Fire Talon | C8/1 Fire Encasement |
| Fire Disc | C8/2 Fire Transmutation |
| Divine Mace | C9/1 Divine Intervention |
| Holy Cudgel | C9/2 Holy Word |

## World grants traced in `event.dat`

| Spell | Acquisition (world) |
|-------|---------------------|
| S2/1 Eagle Eye | World: Nordon quest — Eagle Eye (event loc 60; overland C2 (10,2); retrieve goblet from cave below) |
| S2/6 Lloyd's Beacon | World: Corak's Cave (map 22), tile (11,7) — Lloyd's Beacon behind fake walls (event loc 22) |
| S3/6 Wizard Eye | World: Sandsobar (map 4), tile (7,4) — beggar at "The Beggar's Gift" (door signs evt 41/42); event loc 4 evt 43, exec_selector 0x51, 100 gp (FAQ §3-2-2, §4-3) |
| S5/2 Fingers of Death | World: Overland C1 (map 7), tile (1,8) — Fingers of Death scroll; eat Devil's Food Brownie, face east (event loc 7) |
| S7/1 Dancing Sword | World: Overland A2 (map 9), tile (15,11) — Mist Warrior (Dancing Sword; event loc 9, tile (11,15)) |
| S9/3 Star Burst | World: Overland D1 (map 8), tile (5,6) — Star Burst in Dead Zone (event loc 8, tile (6,5); FAQ: Nature's Gate on day 93) |
| S9/4 Enchant Item | World: Gemmaker's Cave (event loc 31, tile (3,3)) |
| C2/3 Nature's Gate | World: Overland C3 (map 14), tile (1,9) — Nature's Gate after Red Hot Wolf Nipple Chips (event loc 14, tile (9,1); exec_selector 0x59; strings event loc 64) |
| C3/6 Walk on Water | World: Overland C2 (map 11), tile (11,1) — Walk on Water for 50 gp (event loc 11, tile (1,11)) |
| C4/2 Air Transmutation | World: Overland A1 (map 5), tile (8,8) — Air Transmutation scroll (event loc 5; OP_2E) |
| C5/1 Air Encasement | World: Overland A1 (map 5), tile (1,14) — Air Encasement after Elemental Plane of Air (event loc 5, tile (14,1); OP_2E) |
| C5/3 Frenzy | World: Murray's Cave (map 26), tile (1,8) — Frenzy after 15 Crazed Natives (event loc 16; FAQ §3-2-2 C5-3) |
| C6/1 Earth Transmutation | World: Overland E4 (map 40), tile (8,8) — Earth Transmutation tablet (event loc 40; OP_2E) |
| C6/4 Water Encasement | World: Overland A4 (map 15), tile (1,1) — Water Encasement after Elemental Plane of Water (event loc 15; OP_2E) |
| C6/5 Water Transmutation | World: Overland A4 (map 15), tile (8,8) — Water Transmutation scroll (event loc 15; OP_2E) |
| C7/1 Earth Encasement | World: Overland E4 (map 40), tile (14,1) — Earth Encasement after Elemental Plane of Earth (event loc 40, tile (1,14); OP_2E) |
| C8/1 Fire Encasement | World: Overland E1 (map 33), tile (14,14) — Fire Encasement after Elemental Plane of Fire (event loc 33; OP_2E) |
| C8/2 Fire Transmutation | World: Overland E1 (map 33), tile (8,8) — Fire Transmutation plaque (event loc 33; OP_2E) |
| C9/1 Divine Intervention | World: Druid's Cave (map 27), tile (15,14) — Elder Druid quest vs Horvath (exec_selector 0x67; strings event loc 66) |
| C9/2 Holy Word | World: Overland C1 (map 7), tile (5,5) — Holy Word carved on tree, face south (event loc 7; FAQ §3-2-2) |

---

*Generated by `python tools/build_spell_sources_doc.py`. Regenerate after `python tools/dump_shop_tables.py --json EXTRACTED/shop_tables.json`.*
