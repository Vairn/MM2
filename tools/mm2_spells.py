#!/usr/bin/env python3
"""MM2 spell tables + item "use power" (effect-byte) decoder.

The item effect byte (items.dat offset 0x0F) is NOT a coarse "spell type" — it
is a *flat spell index* that pins one exact spell (e.g. Sonic Whip = C2/4 =
Pain, Web Caster = S3/5 = Web). Encoding (verified byte-exact against the
RPGClassics "Use Ability" column for 49 items, 0 mismatches):

    byte == 0x00            -> no use power
    0x01 .. 0x7F            -> non-spell stat boost, nibble packed
                              hi = kind {0 MaxHP,1 Might,2 Speed,3 Accuracy,
                                         5 Level(Enchant),6 SpellLevel}
                              lo = amount
    0x81 .. 0xB0            -> Sorcerer spell, flat index = byte - 0x80
    0xB1 .. 0xE0            -> Cleric  spell, flat index = byte - 0xB0

Flat index (1-based) walks the spell list level by level. Each level has a
different number of spells (same counts for both schools):

    L1 L2 L3 L4 L5 L6 L7 L8 L9
     7  7  6  6  5  5  4  4  4      (48 spells per school)

Spell names/levels are transcribed from the RPGClassics MM2 spell list
(alphabetical within each level, matching the in-game spell numbering — every
item's themed spell lines up, e.g. Hourglass->Time Distortion, Mirror->
Duplication, Disruptor->Disrupt, Antidote Ale->Cure Poison).
"""
from __future__ import annotations

SPELLS_PER_LEVEL = [7, 7, 6, 6, 5, 5, 4, 4, 4]  # L1..L9, both schools

SORCERER = {
    1: ['Awaken', 'Detect Magic', 'Energy Blast', 'Flame Arrow', 'Light', 'Location', 'Sleep'],
    2: ['Eagle Eye', 'Electric Arrow', 'Identify Monster', 'Jump', 'Levitate', "Lloyd's Beacon", 'Protection from Magic'],
    3: ['Acid Stream', 'Fly', 'Invisibility', 'Lightning Bolt', 'Web', 'Wizard Eye'],
    4: ['Cold Beam', 'Feeble Mind', 'Fire Ball', 'Guard Dog', 'Shield', 'Time Distortion'],
    5: ['Disrupt', 'Fingers of Death', 'Sand Storm', 'Shelter', 'Teleport'],
    6: ['Disintegration', 'Entrapment', 'Fantastic Freeze', 'Recharge Item', 'Super Shock'],
    7: ['Dancing Sword', 'Duplication', 'Etherealize', 'Prismatic Light'],
    8: ['Incinerate', 'Mega Volts', 'Meteor Shower', 'Power Shield'],
    9: ['Implosion', 'Inferno', 'Star Burst', 'Enchant Item'],
}
CLERIC = {
    1: ['Apparition', 'Awaken', 'Bless', 'First Aid', 'Light', 'Power Cure', 'Turn Undead'],
    2: ['Cure Wounds', 'Heroism', "Nature's Gate", 'Pain', 'Protection From Elements', 'Silence', 'Weaken'],
    3: ['Cold Ray', 'Create Food', 'Cure Poison', 'Immobilize', 'Lasting Light', 'Walk on Water'],
    4: ['Acid Spray', 'Air Transmutation', 'Cure Disease', 'Restore Alignment', 'Surface', 'Holy Bonus'],
    5: ['Air Encasement', 'Deadly Swarm', 'Frenzy', 'Paralyze', 'Remove Condition'],
    6: ['Earth Transmutation', 'Rejuvenate', 'Stone to Flesh', 'Water Encasement', 'Water Transmutation'],
    7: ['Earth Encasement', 'Fiery Flail', 'Moon Ray', 'Raise Dead'],
    8: ['Fire Encasement', 'Fire Transmutation', 'Mass Distortion', 'Town Portal'],
    9: ['Divine Intervention', 'Holy Word', 'Resurrection', 'Uncurse Item'],
}

USE_BOOST_KINDS = {0: 'Max HP', 1: 'Might', 2: 'Speed', 3: 'Accuracy',
                   5: 'Level', 6: 'Spell Level'}


def _flatmap(tbl):
    """flat 1-based index -> (level, number, name)."""
    out = {}
    f = 0
    for lv in range(1, 10):
        for i, nm in enumerate(tbl[lv]):
            f += 1
            out[f] = (lv, i + 1, nm)
    return out


SORC_FLAT = _flatmap(SORCERER)
CLER_FLAT = _flatmap(CLERIC)


def spell_at(school: str, flat: int):
    """school 'S'|'C', flat 1-based -> (level, number, name) or None."""
    table = SORC_FLAT if school.upper() == 'S' else CLER_FLAT
    return table.get(flat)


def decode_effect(byte: int):
    """Decode an items.dat effect byte (0x0F) into a structured dict."""
    if byte == 0:
        return {'kind': 'none', 'text': 'none'}
    if byte < 0x80:
        name = USE_BOOST_KINDS.get(byte >> 4, 'unknown%d' % (byte >> 4))
        return {'kind': 'boost', 'boost': name, 'amount': byte & 0xF,
                'text': '%s +%d' % (name, byte & 0xF)}
    if byte <= 0xB0:
        school, flat = 'S', byte - 0x80
    else:
        school, flat = 'C', byte - 0xB0
    info = spell_at(school, flat)
    if info is None:
        return {'kind': 'spell', 'school': school, 'flat': flat,
                'text': '%s#%d (?)' % (school, flat)}
    lv, n, nm = info
    return {'kind': 'spell', 'school': school, 'level': lv, 'number': n,
            'name': nm, 'text': '%s%d/%d %s' % (school, lv, n, nm)}


def effect_text(byte: int) -> str:
    return decode_effect(byte)['text']


# ---------------------------------------------------------------------------
# spells.dat reference data + 2-byte record codec
# ---------------------------------------------------------------------------
# Cost/type metadata transcribed from the MM2 "Gates to Another World" manual,
# Appendix B. cast: 'A' anytime, 'C' combat-only, 'N' non-combat-only.
# Fields: (school, level, num, sp, per_level, gems, cast, where, object)
SPELL_META = [
    # ---- SORCERER ----
    ('S', 1, 1, 1, False, 0, 'A', '', 'All sleeping party members'),
    ('S', 1, 2, 1, False, 0, 'N', '', 'Items in backpack'),
    ('S', 1, 3, 1, True, 1, 'C', '', '1 monster'),
    ('S', 1, 4, 1, False, 0, 'C', '', '1 monster'),
    ('S', 1, 5, 1, False, 0, 'N', '', 'Entire party'),
    ('S', 1, 6, 1, False, 0, 'N', '', 'Entire party'),
    ('S', 1, 7, 1, False, 0, 'C', '', '4 monsters +1/L'),
    ('S', 2, 1, 2, True, 0, 'N', 'Outdoor', '5 steps/L'),
    ('S', 2, 2, 2, False, 0, 'C', '', '1 monster'),
    ('S', 2, 3, 2, False, 1, 'C', '', '1 monster'),
    ('S', 2, 4, 2, False, 0, 'N', '', 'Entire party'),
    ('S', 2, 5, 2, False, 0, 'N', '', 'Entire party'),
    ('S', 2, 6, 2, False, 1, 'N', 'Dungeon', 'Entire party'),
    ('S', 2, 7, 1, True, 1, 'A', '', 'Entire party'),
    ('S', 3, 1, 1, True, 2, 'C', '', '1 monster'),
    ('S', 3, 2, 3, False, 0, 'N', 'Outdoor', 'Entire party'),
    ('S', 3, 3, 3, False, 0, 'C', '', 'Entire party'),
    ('S', 3, 4, 1, True, 2, 'C', '', '4 monsters'),
    ('S', 3, 5, 3, False, 2, 'C', 'not hand-to-hand', '4 monsters +1/L'),
    ('S', 3, 6, 3, True, 2, 'N', 'Indoor', '5 steps/L'),
    ('S', 4, 1, 1, True, 3, 'C', '', '1 monster'),
    ('S', 4, 2, 4, False, 3, 'C', '', '5 monsters'),
    ('S', 4, 3, 1, True, 3, 'C', 'not hand-to-hand', '6 monsters'),
    ('S', 4, 4, 4, False, 0, 'N', '', 'Entire party'),
    ('S', 4, 5, 4, False, 0, 'C', '', 'Entire party'),
    ('S', 4, 6, 4, False, 3, 'C', '', 'Entire party'),
    ('S', 5, 1, 5, False, 5, 'C', 'not hand-to-hand', '1 monster'),
    ('S', 5, 2, 5, False, 5, 'C', '', '3 monsters (not undead)'),
    ('S', 5, 3, 2, True, 5, 'C', 'Outdoor', '10 monsters'),
    ('S', 5, 4, 5, False, 0, 'N', '', 'Entire party'),
    ('S', 5, 5, 5, False, 0, 'N', '', 'Entire party'),
    ('S', 6, 1, 6, False, 6, 'C', '', '3 monsters'),
    ('S', 6, 2, 6, False, 6, 'C', '', 'All'),
    ('S', 6, 3, 2, True, 6, 'C', 'not hand-to-hand', '3 monsters'),
    ('S', 6, 4, 6, False, 6, 'N', '', 'Spell caster'),
    ('S', 6, 5, 2, True, 6, 'C', '', '1 monster'),
    ('S', 7, 1, 3, True, 7, 'C', '', '10 monsters'),
    ('S', 7, 2, 7, False, 100, 'N', '', 'Spell caster'),
    ('S', 7, 3, 7, False, 7, 'N', '', 'Entire party'),
    ('S', 7, 4, 7, False, 7, 'C', '', '10 monsters'),
    ('S', 8, 1, 3, True, 8, 'C', '', '1 monster'),
    ('S', 8, 2, 3, True, 8, 'C', '', '10 monsters'),
    ('S', 8, 3, 8, False, 8, 'C', 'Outdoor', 'All (SP-limited)'),
    ('S', 8, 4, 8, False, 8, 'C', '', 'Entire party'),
    ('S', 9, 1, 10, False, 10, 'C', '', '1 monster'),
    ('S', 9, 2, 3, True, 10, 'C', '', '10 monsters'),
    ('S', 9, 3, 10, False, 20, 'C', 'Outdoor', 'All (SP-limited)'),
    ('S', 9, 4, 50, False, 50, 'N', '', 'Spell caster'),
    # ---- CLERIC ----
    ('C', 1, 1, 1, False, 0, 'C', '', '10 monsters'),
    ('C', 1, 2, 1, False, 0, 'A', '', 'All sleeping party members'),
    ('C', 1, 3, 1, False, 0, 'C', '', 'Entire party'),
    ('C', 1, 4, 1, False, 0, 'A', '', '1 character'),
    ('C', 1, 5, 1, False, 0, 'N', '', 'Entire party'),
    ('C', 1, 6, 1, True, 1, 'A', '', '1 character'),
    ('C', 1, 7, 1, False, 0, 'C', '', 'All undead monsters'),
    ('C', 2, 1, 2, False, 0, 'A', '', '1 character'),
    ('C', 2, 2, 2, False, 1, 'C', '', '1 character'),
    ('C', 2, 3, 2, False, 0, 'N', 'Outdoor', 'Entire party'),
    ('C', 2, 4, 2, False, 0, 'C', '', '1 monster (not undead)'),
    ('C', 2, 5, 2, False, 1, 'A', '', 'Entire party'),
    ('C', 2, 6, 2, False, 0, 'C', '', '4 monsters +1/L'),
    ('C', 2, 7, 2, False, 1, 'C', '', '10 monsters'),
    ('C', 3, 1, 3, False, 2, 'C', 'not hand-to-hand', '5 monsters'),
    ('C', 3, 2, 3, False, 2, 'N', '', 'Spell caster'),
    ('C', 3, 3, 3, False, 0, 'A', '', '1 character'),
    ('C', 3, 4, 3, False, 0, 'C', '', '5 monsters'),
    ('C', 3, 5, 3, False, 0, 'N', '', 'Entire party'),
    ('C', 3, 6, 3, False, 2, 'N', 'Outdoor', 'Entire party'),
    ('C', 4, 1, 4, False, 3, 'C', 'not hand-to-hand', '3 monsters'),
    ('C', 4, 2, 4, False, 3, 'N', 'Outdoor', 'Entire party'),
    ('C', 4, 3, 4, False, 0, 'A', '', '1 character'),
    ('C', 4, 4, 4, False, 3, 'N', '', '1 character'),
    ('C', 4, 5, 4, False, 0, 'N', '', 'Entire party'),
    ('C', 4, 6, 4, False, 3, 'C', '', 'Entire party'),
    ('C', 5, 1, 5, False, 5, 'C', '', '1 monster'),
    ('C', 5, 2, 5, False, 5, 'C', '', '10 monsters'),
    ('C', 5, 3, 5, False, 5, 'C', '', '1 character (once each)'),
    ('C', 5, 4, 5, False, 5, 'C', '', '10 monsters'),
    ('C', 5, 5, 5, False, 5, 'A', '', '1 character'),
    ('C', 6, 1, 6, False, 6, 'N', 'Outdoor', 'Entire party'),
    ('C', 6, 2, 6, False, 6, 'N', '', '1 character'),
    ('C', 6, 3, 6, False, 6, 'A', '', '1 character'),
    ('C', 6, 4, 6, False, 6, 'C', '', '1 monster'),
    ('C', 6, 5, 6, False, 6, 'N', 'Outdoor', 'Entire party'),
    ('C', 7, 1, 7, False, 7, 'C', '', '1 monster'),
    ('C', 7, 2, 7, False, 7, 'C', '', '1 monster'),
    ('C', 7, 3, 7, False, 7, 'C', 'Outdoor', '10 monsters'),
    ('C', 7, 4, 7, False, 7, 'A', '', '1 character'),
    ('C', 8, 1, 8, False, 8, 'C', '', '1 monster'),
    ('C', 8, 2, 8, False, 8, 'N', 'Outdoor', 'Entire party'),
    ('C', 8, 3, 8, False, 8, 'C', '', '2 monsters'),
    ('C', 8, 4, 8, False, 8, 'N', '', 'Entire party'),
    ('C', 9, 1, 10, False, 20, 'C', '', 'Entire party'),
    ('C', 9, 2, 10, False, 10, 'C', '', 'All'),
    ('C', 9, 3, 10, False, 10, 'N', '', '1 character'),
    ('C', 9, 4, 10, False, 50, 'N', '', 'Spell caster'),
]

CAST_NAMES = {'A': 'Anytime', 'C': 'Combat only', 'N': 'Non-combat only'}


def spell_meta(record_index: int):
    """Manual reference tuple for a 0-based record index (S 0..47, C 48..95)."""
    return SPELL_META[record_index]


def record_school_flat(record_index: int):
    """0-based record index -> ('S'|'C', flat 1-based index)."""
    if record_index < 48:
        return 'S', record_index + 1
    return 'C', record_index - 48 + 1


def format_cost(sp, per_level, gems):
    s = '%d/L' % sp if per_level else '%d SP' % sp
    if gems:
        s += ' + %d Gem%s' % (gems, '' if gems == 1 else 's')
    return s


def decode_record(b0: int, b1: int):
    """Decode a spells.dat 2-byte record into a dict.

    byte0: 0x40 combat-only, 0x80 non-combat-only (neither=anytime),
           0x10 special/computed gem cost, low nibble = gems.
    byte1: 0x80 outdoor-only, bits 6-4 = per-level SP multiplier (when low
           nibble 0), low nibble = flat SP cost (0 => per caster level).
    """
    if b0 & 0x40:
        cast = 'C'
    elif b0 & 0x80:
        cast = 'N'
    else:
        cast = 'A'
    per_level = (b1 & 0x0F) == 0
    sp = ((b1 >> 4) & 0x07) if per_level else (b1 & 0x0F)
    return {
        'gems': b0 & 0x0F,
        'cast': cast,
        'special_cost': bool(b0 & 0x10),
        'outdoor': bool(b1 & 0x80),
        'per_level': per_level,
        'sp': sp,
        'cost': format_cost(sp, per_level, b0 & 0x0F),
        'b0': b0, 'b1': b1,
    }


def encode_record(sp, per_level, gems, cast, special_cost=False, outdoor=False) -> tuple:
    """Encode cost/type fields back into (b0, b1)."""
    b0 = gems & 0x0F
    if cast == 'C':
        b0 |= 0x40
    elif cast == 'N':
        b0 |= 0x80
        if special_cost:
            b0 |= 0x20  # observed on non-combat special-cost spells
    if special_cost:
        b0 |= 0x10
    if per_level:
        b1 = (sp & 0x07) << 4
    else:
        b1 = sp & 0x0F
    if outdoor:
        b1 |= 0x80
    return b0, b1


def load_spells_dat(path: str = 'spells.dat'):
    """Read spells.dat -> list of 96 decoded record dicts (with meta merged)."""
    data = open(path, 'rb').read()
    out = []
    for i in range(96):
        rec = decode_record(data[i * 2], data[i * 2 + 1])
        school, flat = record_school_flat(i)
        meta = SPELL_META[i]
        lv, num, name = spell_at(school, flat)
        rec.update(index=i, school=school, flat=flat, level=lv, number=num,
                   name=name, manual=meta)
        out.append(rec)
    return out


if __name__ == '__main__':
    import sys
    path = sys.argv[1] if len(sys.argv) > 1 else 'items.dat'
    data = open(path, 'rb').read()
    rs = 20
    for i in range(256):
        nm = data[i * rs:i * rs + 12].decode('latin1').strip()
        b = data[i * rs + 0x0F]
        if b >= 0x80:
            print('%3d %-13s -> 0x%02X %s' % (i, nm, b, effect_text(b)))
