#!/usr/bin/env python3
"""Emit SpellCastWhen/outdoor initializers from spells.dat for SpellBook.h."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
data = (ROOT / "spells.dat").read_bytes()

SORC = [
    "Awaken", "Detect Magic", "Energy Blast", "Flame Arrow", "Light", "Location", "Sleep",
    "Eagle Eye", "Electric Arrow", "Identify Monster", "Jump", "Levitate", "Lloyd's Beacon",
    "Protection from Magic", "Acid Stream", "Fly", "Invisibility", "Lightning Bolt", "Web",
    "Wizard Eye", "Cold Beam", "Feeble Mind", "Fire Ball", "Guard Dog", "Shield",
    "Time Distortion", "Disrupt", "Fingers of Death", "Sand Storm", "Shelter", "Teleport",
    "Disintegration", "Entrapment", "Fantastic Freeze", "Recharge Item", "Super Shock",
    "Dancing Sword", "Duplication", "Etherealize", "Prismatic Light", "Incinerate",
    "Mega Volts", "Meteor Shower", "Power Shield", "Implosion", "Inferno", "Star Burst",
    "Enchant Item",
]
CLER = [
    "Apparition", "Awaken", "Bless", "First Aid", "Light", "Power Cure", "Turn Undead",
    "Cure Wounds", "Heroism", "Nature's Gate", "Pain", "Protection From Elements", "Silence",
    "Weaken", "Cold Ray", "Create Food", "Cure Poison", "Immobilize", "Lasting Light",
    "Walk on Water", "Acid Spray", "Air Transmutation", "Cure Disease", "Restore Alignment",
    "Surface", "Holy Bonus", "Air Encasement", "Deadly Swarm", "Frenzy", "Paralyze",
    "Remove Condition", "Earth Transmutation", "Rejuvenate", "Stone to Flesh",
    "Water Encasement", "Water Transmutation", "Earth Encasement", "Fiery Flail", "Moon Ray",
    "Raise Dead", "Fire Encasement", "Fire Transmutation", "Mass Distortion", "Town Portal",
    "Divine Intervention", "Holy Word", "Resurrection", "Uncurse Item",
]


def when_of(b0: int) -> str:
    c = bool(b0 & 0x40)
    n = bool(b0 & 0x80)
    if c and not n:
        return "SpellCastWhen::Combat"
    if n and not c:
        return "SpellCastWhen::Explore"
    return "SpellCastWhen::Anytime"


def dump(names, offset: int, label: str) -> None:
    print(f"inline constexpr SpellCastWhen k{label}CastWhen[kSpellsPerSchool] = {{")
    for i, name in enumerate(names):
        b0 = data[(offset + i) * 2]
        print(f"    {when_of(b0)},  // {i:2d} {name}")
    print("};")
    print(f"inline constexpr bool k{label}Outdoor[kSpellsPerSchool] = {{")
    for i, name in enumerate(names):
        b1 = data[(offset + i) * 2 + 1]
        print(f"    {str(bool(b1 & 0x80)).lower()},  // {i:2d} {name}")
    print("};")


dump(SORC, 0, "Sorcerer")
print()
dump(CLER, 48, "Cleric")
