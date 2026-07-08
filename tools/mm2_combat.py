#!/usr/bin/env python3
"""MM2 combat emulator — control flow and math traced from mm2.capstone.asm.

Implements doc 17 / asm addresses:
  0x12A22 round loop + initiative
  0x11314 / 0x11426 / 0x11508 player hit + damage vs monster
  0x0FE3C / 0x10478 monster hit + damage vs party
  0x1064C monster turn (flee @ 0x10728, group @ 0x105B6 / 0x10002, archer @ 0x107A4)
  0x10DFC flee, 0x100B0 multiply, 0x11F0A adds-friends
  0x10B74 victory rewards

Lookup tables are lifted from ``EXTRACTED/ghidra/mm2_data_00.bin`` (see
``mm2_combat_tables.py``). Stat-effect tiers use A4-$73DE via -$7F56(A4)
(asm 0x6488). Runtime weapon bytes $4C-$4F are rebuilt @ 0xF270 from
equipment; the emulator approximates those from equipped item damage.

Usage::

    python tools/mm2_combat.py test
    python tools/mm2_combat.py simulate --monsters 1,1,1 --auto --seed 42
    python tools/mm2_combat.py interactive --monsters 212 --roster roster.dat
"""
from __future__ import annotations

import argparse
import random
import sys
from dataclasses import dataclass, field
from enum import Enum, IntFlag
from pathlib import Path
from typing import Callable, Iterable, Optional

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from decode_items import Item, load as load_items  # noqa: E402
from decode_monsters import (  # noqa: E402
    FILE_SIZE as MONSTERS_FILE_SIZE,
    PARTY_VERB_NAMES,
    RECORD_SIZE as MONSTER_RECORD_SIZE,
    SINGLE_EFFECT_NAMES,
    decode_record as decode_monster_record,
    enrich_decoded_stats,
)
from decode_roster import CLASSES, decode_record as decode_roster_record  # noqa: E402
from mm2_spells import load_spells_dat  # noqa: E402
from mm2_combat_tables import (  # noqa: E402
    CLASSES as CLASS_NAMES,
    FLEE_THRESHOLD,
    GROUP_DAMAGE_BY_PARTY_IDX,
    HIT_DIV_BY_CLASS,
    MONSTER_AC_REF,
    MONSTER_HIT_ADDEND,
    PARTY_ATTACK_THRESHOLD,
    SWING_DIV_BY_CLASS,
    build_weapon_fields,
    item_is_ranged,
    stat_effect,
)

MAX_MONSTER_SLOTS = 11
MAX_PARTY = 8
FRONT_RANK_DEFAULT = 4
ENCOUNTER_DIFFICULTY = 0  # A4-$6FC2; 0 lets all flee tiers attempt

OFFENSIVE_SPELLS = {
    "Apparition", "Energy Blast", "Flame Arrow", "Electric Arrow", "Acid Stream",
    "Lightning Bolt", "Cold Beam", "Fire Ball", "Disrupt", "Fingers of Death",
    "Sand Storm", "Disintegration", "Super Shock", "Prismatic Light", "Incinerate",
    "Mega Volts", "Meteor Shower", "Implosion", "Inferno", "Star Burst",
    "Pain", "Cold Ray", "Acid Spray", "Deadly Swarm", "Fiery Flail", "Moon Ray",
    "Mass Distortion", "Holy Word",
}
CONTROL_SPELLS = {
    "Sleep", "Web", "Immobilize", "Paralyze", "Entrapment", "Fantastic Freeze",
    "Air Encasement", "Water Encasement", "Earth Encasement", "Fire Encasement",
    "Silence", "Weaken",
}
HEAL_SPELLS = {"First Aid", "Power Cure", "Cure Wounds", "Rejuvenate"}
CURE_SPELLS = {"Awaken", "Cure Poison", "Cure Disease", "Remove Condition"}
BUFF_SPELLS = {
    "Bless", "Heroism", "Holy Bonus", "Shield", "Power Shield",
    "Protection from Magic", "Protection From Elements", "Frenzy",
}
BOSS_KILL_SPELLS = {"Implosion", "Dancing Sword", "Inferno", "Star Burst", "Disintegration"}


class PlayerCommand(str, Enum):
    ATTACK = "A"
    FIGHT = "F"
    SHOOT = "S"
    CAST = "C"
    BLOCK = "B"
    RUN = "R"
    USE = "U"
    VIEW = "V"


class HitResult(str, Enum):
    MISS = "miss"
    GLANCE = "glance"
    HIT = "hit"


class CharCondition(IntFlag):
    NONE = 0
    HURT = 0x01
    SILENCED = 0x02
    ASLEEP = 0x04
    AFRAID = 0x08
    PARALYZED = 0x10
    UNCONSCIOUS = 0x20
    DEAD = 0x40


class MonsterStatus(IntFlag):
    AWAKE = 0x01
    ASLEEP = 0x02
    PARALYZED = 0x04
    HALF_DAMAGE = 0x08  # bit 2 @ 0x10562
    HALF_STRENGTH = 0x10  # bit 3 @ 0x10562
    FLED = 0x40
    SPECIAL = 0x80


@dataclass
class CombatConfig:
    max_rounds: int = 200
    encounter_difficulty: int = ENCOUNTER_DIFFICULTY


@dataclass
class Character:
    name: str
    class_id: int
    char_class: str
    level: int
    might: int
    speed: int
    accuracy: int
    luck: int
    ac: int
    hp_max: int
    hp_cur: int
    sp_max: int
    sp_cur: int
    gems: int = 0
    spell_school: Optional[str] = None  # 'S' or 'C'
    known_spells: set[int] = field(default_factory=set)  # spells.dat record indices (0..95)
    spell_level: int = 0
    weapon_melee: int = 0
    weapon_ranged: int = 0
    acc_melee: int = 0
    acc_ranged: int = 0
    w4c: int = 0
    w4d: int = 0
    w4e: int = 0
    w4f: int = 0
    has_bow: bool = False
    is_caster: bool = False
    condition: CharCondition = CharCondition.NONE
    blocking: bool = False
    front_rank: bool = True
    is_shooting: bool = False

    @property
    def alive(self) -> bool:
        return self.hp_cur > 0 and CharCondition.DEAD not in self.condition

    @property
    def can_act(self) -> bool:
        if not self.alive:
            return False
        if self.condition & (CharCondition.ASLEEP | CharCondition.PARALYZED
                             | CharCondition.UNCONSCIOUS):
            return False
        return True

    @property
    def silenced(self) -> bool:
        return bool(self.condition & CharCondition.SILENCED)

    def hit_bonus(self) -> int:
        """A4-$5E2C @ 0x11314: level // HIT_DIV[class]."""
        div = max(1, HIT_DIV_BY_CLASS[self.class_id])
        return self.level // div

    def swing_count(self) -> int:
        """A4-$5E2E @ 0x11356: level // SWING_DIV[class] + 1."""
        div = max(1, SWING_DIV_BY_CLASS[self.class_id])
        return self.level // div + 1

    def combat_weapon_pack(self, archer_roll: int = 0) -> tuple[int, int, int]:
        """Map runtime $4C-$4F / combat globals @ 0x1137A+.

        Returns (rng_max, hit_acc, damage_addend) where:
          rng_max       -> A4-$5E30 weapon die ($4C / $4E)
          hit_acc       -> A4-$5E2B after luck tier @ 0x1140C ($4D/$4F + tier(luck))
          damage_addend -> A4-$5E2F after might tier @ 0x113FC (+ archer roll @ 0x113B4)
        """
        if self.is_shooting:
            rng_max = self.w4e or self.weapon_ranged or max(1, self.level)
            base_acc = self.w4f or self.acc_ranged
        else:
            rng_max = self.w4c or self.weapon_melee or max(1, self.level // 2)
            base_acc = self.w4d or self.acc_melee
        hit_acc = base_acc + stat_effect(self.luck)
        damage_addend = archer_roll + base_acc + stat_effect(self.might)
        return rng_max, hit_acc, damage_addend

    @classmethod
    def from_roster(cls, rec: dict, items: list[Item], front: bool) -> Character:
        sc = rec["stats_current"]
        cid = rec["class"]
        if isinstance(cid, str):
            class_id = CLASS_NAMES.index(cid) if cid in CLASS_NAMES else 0
        else:
            class_id = cid
        char_class = rec["class"] if isinstance(rec["class"], str) else CLASS_NAMES[class_id]
        damages = [it.damage for it in items]
        w4c, w4d, w4e, w4f = build_weapon_fields(rec["equip"], damages)
        weapon_m = w4c
        weapon_r = w4e
        acc_m = w4d if w4d else sc["Acc"]
        acc_r = w4f if w4f else sc["Acc"]
        has_bow = char_class == "Archer" or w4e > 0 or any(
            slot_id and item_is_ranged(slot_id) for slot_id, _, _ in rec["equip"]
        )
        is_caster = char_class in ("Cleric", "Sorcerer", "Paladin", "Archer")
        spell_school: Optional[str] = None
        if char_class in ("Sorcerer", "Archer"):
            spell_school = "S"
        elif char_class in ("Cleric", "Paladin"):
            spell_school = "C"
        known_spells: set[int] = set()
        if spell_school:
            raw = bytes.fromhex(rec.get("spells_raw", ""))
            for flat in range(1, 49):
                # roster.dat stores the 48-spell school bitset in reverse byte order:
                # low-level spells are in byte 0x51, high-level at 0x4C.
                spell_bit = flat - 1
                byte = 5 - (spell_bit // 8)
                bit = spell_bit % 8
                if byte < 0 or byte >= len(raw):
                    continue
                if ((raw[byte] >> bit) & 1) == 0:
                    continue
                rec_idx = (flat - 1) if spell_school == "S" else (48 + flat - 1)
                known_spells.add(rec_idx)
        return cls(
            name=rec["name"],
            class_id=class_id,
            char_class=char_class,
            level=rec["level"],
            might=sc["Might"],
            speed=sc["Spd"],
            accuracy=sc["Acc"],
            luck=sc["Luck"],
            ac=rec["ac"],
            hp_max=rec["hp_max"],
            hp_cur=rec["hp_cur"],
            sp_max=rec["sp_max"],
            sp_cur=rec["sp_cur"],
            gems=rec.get("gems", 0),
            spell_school=spell_school,
            known_spells=known_spells,
            spell_level=rec.get("spell_lvl", 0),
            weapon_melee=weapon_m or max(1, sc["Might"] // 3),
            weapon_ranged=weapon_r,
            acc_melee=acc_m,
            acc_ranged=acc_r,
            w4c=w4c,
            w4d=w4d,
            w4e=w4e,
            w4f=w4f,
            has_bow=has_bow,
            is_caster=is_caster,
            condition=CharCondition(rec["condition"] & 0x7F),
            front_rank=front,
        )


@dataclass
class MonsterType:
    index: int
    name: str
    hp: int
    xp: int
    ac: int
    damage: int
    initiative: int
    party_verb: int
    party_chance: int
    single_effect: int
    archer: bool
    undead: bool
    flee_tier: int
    multiplies: bool
    add_friends: int
    spd_lo: int
    spd_hi: int
    item_drop_level: int
    drops_gems: bool
    gold_tier: int

    @property
    def type_hi(self) -> int:
        return (self.index >> 4) & 0x0F


@dataclass
class MonsterSlot:
    type_id: int
    hp: int
    status: MonsterStatus = MonsterStatus.AWAKE
    speed: int = 0
    acted: bool = False
    group_counter: int = 0  # A4-$503, init from record spd_lo @ slot setup

    def alive(self) -> bool:
        return self.hp > 0 and not (self.status & MonsterStatus.FLED)

    def can_act(self) -> bool:
        if not self.alive():
            return False
        if self.status & MonsterStatus.SPECIAL:
            return False
        if self.status & (MonsterStatus.ASLEEP | MonsterStatus.PARALYZED):
            return False
        if not (self.status & MonsterStatus.AWAKE):
            return False
        if self.status & 0x30:
            return False
        return True


@dataclass
class Rewards:
    xp: int = 0
    gold: int = 0
    gems: int = 0
    item_drop_level: int = 0


@dataclass
class BattleLog:
    lines: list[str] = field(default_factory=list)

    def say(self, msg: str) -> None:
        self.lines.append(msg)
        print(msg)


@dataclass
class Battle:
    party: list[Character]
    monsters: list[MonsterSlot]
    types: dict[int, MonsterType]
    rng: random.Random
    config: CombatConfig
    log: BattleLog = field(default_factory=BattleLog)
    round_num: int = 0
    front_rank_cutoff: int = FRONT_RANK_DEFAULT
    rewards: Rewards = field(default_factory=Rewards)
    defeated_types: list[int] = field(default_factory=list)
    spells: list[dict] = field(default_factory=list)  # spells.dat decoded records
    outcome: Optional[str] = None

    @property
    def live_monsters(self) -> list[tuple[int, MonsterSlot]]:
        return [(i, m) for i, m in enumerate(self.monsters) if m.alive()]

    @property
    def live_party(self) -> list[tuple[int, Character]]:
        return [(i, c) for i, c in enumerate(self.party) if c.alive]

    def monster_type(self, slot: MonsterSlot) -> MonsterType:
        return self.types[slot.type_id]


def _repo_root() -> Path:
    return _TOOLS.parent


def _find_dat(name: str, explicit: Optional[str]) -> Path:
    if explicit:
        p = Path(explicit)
        if not p.is_file():
            raise FileNotFoundError(p)
        return p
    for base in (_repo_root(), _repo_root() / "EXTRACTED", _repo_root() / "MM2BOOT"):
        p = base / name
        if p.is_file():
            return p
    raise FileNotFoundError(f"{name} not found")


def load_monster_types(path: Path) -> dict[int, MonsterType]:
    data = path.read_bytes()
    if len(data) != MONSTERS_FILE_SIZE:
        raise ValueError(f"{path}: expected {MONSTERS_FILE_SIZE} bytes")
    out: dict[int, MonsterType] = {}
    for idx in range(len(data) // MONSTER_RECORD_SIZE):
        rec = data[idx * MONSTER_RECORD_SIZE:(idx + 1) * MONSTER_RECORD_SIZE]
        d = enrich_decoded_stats(decode_monster_record(idx, rec))
        if not d["name"].strip():
            continue
        out[idx] = MonsterType(
            index=idx,
            name=d["name"],
            hp=d["hp"],
            xp=d["xp"],
            ac=d["ac_val"],
            damage=d["damage_val"],
            initiative=d["initiative"],
            party_verb=d["party_verb"],
            party_chance=d["party_chance"],
            single_effect=d["single_effect"],
            archer=bool(d["archer"]),
            undead=bool(d["undead"]),
            flee_tier=d["flee_tier"],
            multiplies=bool(d["multiplies"]),
            add_friends=(d["add_friends_base"] + 1) * (10 if d["add_friends_x10"] else 1),
            spd_lo=d["speed_lo"],
            spd_hi=d["speed_hi"],
            item_drop_level=d["item_drop_level"],
            drops_gems=bool(d["drops_gems"]),
            gold_tier=d["gold_tier"],
        )
    return out


def _parse_slot_list(spec: Optional[str]) -> list[int]:
    if not spec:
        return []
    slots: list[int] = []
    for raw in spec.split(","):
        raw = raw.strip()
        if not raw:
            continue
        idx = int(raw, 0)
        if idx < 0 or idx >= 64:
            raise ValueError(f"roster slot out of range: {idx}")
        slots.append(idx)
    return slots


def load_party_from_roster(path: Path, items: list[Item], slot_spec: Optional[str] = None) -> list[Character]:
    from decode_roster import load_roster_bytes

    data = load_roster_bytes(path)
    party: list[Character] = []
    explicit_slots = _parse_slot_list(slot_spec)
    if explicit_slots:
        for slot in explicit_slots:
            rec = decode_roster_record(data, slot)
            if rec is None:
                raise ValueError(f"empty roster slot {slot} in {path}")
            party.append(Character.from_roster(rec, items, front=len(party) < FRONT_RANK_DEFAULT))
    else:
        for slot in range(64):
            rec = decode_roster_record(data, slot)
            if rec is None or not rec["in_party"]:
                continue
            party.append(Character.from_roster(rec, items, front=len(party) < FRONT_RANK_DEFAULT))
        if not party:
            # Many saves keep party bits clear; fall back to first non-empty records.
            for slot in range(64):
                rec = decode_roster_record(data, slot)
                if rec is None:
                    continue
                party.append(Character.from_roster(rec, items, front=len(party) < FRONT_RANK_DEFAULT))
                if len(party) >= MAX_PARTY:
                    break
            if party:
                print(f"[mm2_combat] warning: no in-party flags in {path}; using first {len(party)} roster slots.")
    if not party:
        raise ValueError(f"no in-party characters in {path}")
    return party[:MAX_PARTY]


def make_test_party() -> list[Character]:
    return [
        Character("Test Knight", 0, "Knight", 5, 18, 15, 16, 12, 10,
                  60, 60, 0, 0, weapon_melee=12, front_rank=True),
        Character("Test Cleric", 3, "Cleric", 5, 14, 12, 14, 10, 8,
                  40, 40, 30, 30, gems=10, spell_school="C", known_spells={48},
                  spell_level=2, weapon_melee=6, is_caster=True, front_rank=True),
    ]


def make_test_types() -> dict[int, MonsterType]:
    return {
        1: MonsterType(1, "Orc", 20, 100, 8, 6, 12, 0, 0, 1, False, False,
                       0, False, 0, 1, 1, 0, False, 0),
        5: MonsterType(5, "Skeleton", 15, 80, 6, 5, 10, 0, 0, 3, False, True,
                       1, False, 0, 1, 1, 0, False, 0),
    }


class CombatEngine:
    """ASM-aligned combat resolution."""

    def __init__(self, battle: Battle) -> None:
        self.b = battle

    def roll(self, lo: int, hi: int) -> int:
        return self.b.rng.randint(lo, hi)

    def roll100(self) -> int:
        return self.roll(1, 100)

    # --- player vs monster (0x11426 / 0x11508) ---------------------------

    def resolve_player_hit(self, char: Character, mt: MonsterType) -> HitResult:
        r1 = self.roll100()
        if r1 <= 6:
            return HitResult.MISS
        if r1 <= 9:
            return HitResult.GLANCE

        base = 3 if char.condition & CharCondition.HURT else 25
        threshold = min(250, base + char.hit_bonus())
        r2 = self.roll(1, threshold)
        _, hit_acc, _ = char.combat_weapon_pack()
        hit_roll = r2 + hit_acc
        if hit_roll > 255:
            return HitResult.MISS
        if hit_roll <= 10:
            return HitResult.GLANCE
        if hit_roll >= mt.ac:
            return HitResult.HIT
        return HitResult.GLANCE

    def resolve_player_damage(self, char: Character, glance: bool) -> int:
        archer_roll = 0
        if char.class_id == 2 and not char.is_shooting:
            archer_roll = self.roll(1, min(char.level, 100))
        rng_max, _, damage_addend = char.combat_weapon_pack(archer_roll)
        total = 0
        swings = 1 if glance else char.swing_count()
        for _ in range(swings):
            r = self.roll(1, rng_max) + damage_addend
            if r > 250:
                r = 1
            total += r
        if not char.is_shooting:
            bonus_roll = self.roll100()
            if char.class_id == 5 and bonus_roll <= 90 and bonus_roll > 5:  # Robber @ 0x115A6
                total *= 2
            elif char.class_id == 6 and bonus_roll <= 94 and bonus_roll > 5:  # Ninja @ 0x115CA
                total *= 4 if char.spell_level >= 2 else 2
        return max(0, total)

    def player_attack_monster(self, char: Character, m_idx: int) -> None:
        m = self.b.monsters[m_idx]
        mt = self.b.monster_type(m)
        verb = "shoots" if char.is_shooting else "attacks"
        self.b.log.say(f"{char.name} {verb} {mt.name}.")
        hit = self.resolve_player_hit(char, mt)
        if hit == HitResult.MISS:
            self.b.log.say("  Miss!")
            return
        if hit == HitResult.GLANCE:
            self.b.log.say("  Glancing blow.")
        dmg = self.resolve_player_damage(char, hit == HitResult.GLANCE)
        if dmg <= 0:
            return
        if char.blocking:
            pass
        self.apply_damage_to_monster(m_idx, dmg)

    # --- monster vs player (0x0FE3C / 0x10478) ---------------------------

    def monster_hit_player(self, mt: MonsterType) -> bool:
        hi = mt.type_hi
        threshold = hi + MONSTER_HIT_ADDEND[hi]
        return self.roll100() <= threshold

    def monster_damage_player(self, mt: MonsterType, char: Character, m: MonsterSlot) -> int:
        """0x10478: swings from -$11B2, per-proc damage RNG(1..-$11B3)."""
        hi = mt.type_hi
        ac_ref = MONSTER_AC_REF[hi]
        base = 5 if char.ac >= ac_ref else max(1, ac_ref - char.ac)
        if m.status & MonsterStatus.HALF_STRENGTH:
            base = max(1, base // 2)
        total = 0
        swings = max(1, mt.spd_lo)
        for _ in range(swings):
            roll = self.roll(1, 1009) // 10
            if roll <= base:
                total += self.roll(1, max(1, mt.damage))
        if m.status & MonsterStatus.HALF_DAMAGE:
            total = max(1, total // 2)
        if char.blocking:
            total = max(1, total // 2)
        return total

    def apply_single_effect(self, idx: int, effect: int) -> None:
        if effect <= 0 or effect >= len(SINGLE_EFFECT_NAMES):
            return
        name = SINGLE_EFFECT_NAMES[effect]
        char = self.b.party[idx]
        self.b.log.say(f"  {char.name}: {name}")
        if effect == 5:
            char.condition |= CharCondition.ASLEEP
        elif effect == 7:
            char.condition |= CharCondition.SILENCED
        elif effect == 8:
            char.condition |= CharCondition.PARALYZED
        elif effect == 10:
            char.hp_cur = 0
            char.condition |= CharCondition.DEAD
        elif effect == 11:
            char.condition |= CharCondition.PARALYZED

    def monster_melee_player(self, idx: int, mt: MonsterType) -> None:
        t_idx = self.pick_party_target()
        if t_idx is None:
            return
        char = self.b.party[t_idx]
        m = self.b.monsters[idx]
        self.b.log.say(f"{mt.name} attacks {char.name}.")
        if not self.monster_hit_player(mt):
            self.b.log.say("  Miss!")
            return
        dmg = self.monster_damage_player(mt, char, m)
        self.apply_damage_to_char(t_idx, dmg)
        self.apply_single_effect(t_idx, mt.single_effect)

    def monster_group_attack(self, idx: int, mt: MonsterType) -> None:
        verb = PARTY_VERB_NAMES[mt.party_verb] if mt.party_verb < len(PARTY_VERB_NAMES) else "attacks"
        self.b.log.say(f"{mt.name} {verb} the party!")
        party_n = len(self.b.live_party)
        gi = min(3, max(0, party_n - 1))
        base = GROUP_DAMAGE_BY_PARTY_IDX[gi]
        for i, char in self.b.live_party:
            dmg = max(1, base + mt.party_verb // 3)
            if char.blocking:
                dmg = max(1, dmg // 2)
            self.apply_damage_to_char(i, dmg)
        # Gameplay rule: frenzy/explode-style group attacks are suicidal.
        # Apply party-wide damage first, then kill the attacker.
        v = verb.lower()
        suicidal = (
            "frenz" in v
            or "explod" in v
            or "inferno" in v
            or "incinerate" in v
        )
        if suicidal and idx < len(self.b.monsters) and self.b.monsters[idx].alive():
            self.b.monsters[idx].hp = 0
            self.b.log.say(f"{mt.name} burns itself out!")

    # --- monster AI (0x1064C) ---------------------------------------------

    def try_flee(self, mt: MonsterType) -> bool:
        """0x10728: flee when table[tier] < A4-$6FC2 and roll100 <= 50."""
        if mt.flee_tier < 0 or mt.flee_tier >= len(FLEE_THRESHOLD):
            return False
        gate = FLEE_THRESHOLD[mt.flee_tier]
        if gate >= self.b.config.encounter_difficulty:
            return False
        return self.roll100() <= 50

    def group_attack_ready(self, m: MonsterSlot, mt: MonsterType) -> bool:
        """0x105B6: status bit6, or (group_counter>0 and roll<=party threshold)."""
        if m.status & 0x40:
            return True
        if m.group_counter <= 0:
            return False
        thr = PARTY_ATTACK_THRESHOLD[mt.party_chance & 7]
        if self.roll100() <= thr:
            m.group_counter = max(0, m.group_counter - 1)
            return True
        return False

    def monster_turn(self, idx: int) -> None:
        m = self.b.monsters[idx]
        mt = self.b.monster_type(m)
        if not m.can_act():
            return

        if self.try_flee(mt):
            self.b.log.say(f"{mt.name} runs away!")
            m.status = MonsterStatus(int(m.status) | int(MonsterStatus.FLED))
            return

        if self.group_attack_ready(m, mt):
            self.monster_group_attack(idx, mt)
            self.post_monster_specials(idx, mt)
            return

        if mt.party_verb and self.roll100() <= PARTY_ATTACK_THRESHOLD[mt.party_chance & 7]:
            self.monster_group_attack(idx, mt)
            self.post_monster_specials(idx, mt)
            return

        if mt.archer and self.roll100() <= 50:
            self.monster_melee_player(idx, mt)  # 0x10584 ranged uses same damage path stub
            self.post_monster_specials(idx, mt)
            return

        self.monster_melee_player(idx, mt)
        self.post_monster_specials(idx, mt)

    def post_monster_specials(self, idx: int, mt: MonsterType) -> None:
        if mt.multiplies:
            self.try_multiply(idx, mt)
        if mt.add_friends:
            self.try_add_friends(idx, mt)

    def try_multiply(self, idx: int, mt: MonsterType) -> None:
        live = len(self.b.live_monsters)
        if live <= 10 or live >= 110:
            return
        extra = live - 10
        added = 0
        for _ in range(extra):
            if len(self.b.monsters) >= MAX_MONSTER_SLOTS:
                break
            self.b.monsters.append(MonsterSlot(
                type_id=mt.index, hp=mt.hp, speed=mt.initiative,
                status=MonsterStatus.AWAKE, group_counter=mt.spd_lo,
            ))
            added += 1
        if added:
            self.b.log.say(f"{mt.name} multiplies! (+{added})")

    def try_add_friends(self, idx: int, mt: MonsterType) -> None:
        if self.roll100() > 15:
            return
        count = min(mt.add_friends, MAX_MONSTER_SLOTS - len(self.b.monsters))
        if count <= 0:
            return
        for _ in range(count):
            self.b.monsters.append(MonsterSlot(
                type_id=mt.index, hp=mt.hp, speed=mt.initiative,
                status=MonsterStatus.AWAKE, group_counter=mt.spd_lo,
            ))
        self.b.log.say(f"{mt.name} adds friends! (+{count})")

    # --- shared -------------------------------------------------------------

    def apply_damage_to_monster(self, idx: int, dmg: int) -> None:
        m = self.b.monsters[idx]
        m.hp = max(0, m.hp - dmg)
        mt = self.b.monster_type(m)
        self.b.log.say(f"  -> {mt.name} takes {dmg} damage ({m.hp} HP left)")
        if m.hp <= 0:
            self.b.defeated_types.append(m.type_id)
            self.b.log.say(f"  {mt.name} is defeated!")

    def apply_damage_to_char(self, idx: int, dmg: int) -> None:
        c = self.b.party[idx]
        c.hp_cur = max(0, c.hp_cur - dmg)
        self.b.log.say(f"  -> {c.name} takes {dmg} damage ({c.hp_cur}/{c.hp_max} HP)")
        if c.hp_cur <= 0:
            c.condition |= CharCondition.UNCONSCIOUS
            self.b.log.say(f"  {c.name} collapses!")

    def pick_monster_target(self, auto: bool = True) -> Optional[int]:
        live = self.b.live_monsters
        if not live:
            return None
        return min(live, key=lambda t: t[1].hp)[0]

    def pick_party_target(self) -> Optional[int]:
        live = self.b.live_party
        if not live:
            return None
        front = [(i, c) for i, c in live if c.front_rank]
        pool = front or live
        return self.b.rng.choice(pool)[0]

    def grant_victory_rewards(self) -> None:
        self.b.log.say("\n*** Victory! ***")
        rw = Rewards()
        for type_id in self.b.defeated_types:
            mt = self.b.types.get(type_id)
            if mt is None:
                continue
            rw.xp += mt.xp
            if mt.drops_gems:
                rw.gems += self.roll(1, 10)
            gold_byte = type_id
            if mt.gold_tier == 2:
                gold_byte = max(1, gold_byte // 16)
            elif mt.gold_tier >= 3:
                gold_byte = max(1, gold_byte // 2)
            if mt.gold_tier >= 1:
                if mt.gold_tier == 1:
                    gold_byte = 0
                else:
                    gold_byte += self.roll(1, max(1, gold_byte))
                rw.gold += gold_byte + self.roll(1, 50) + 6
            rw.item_drop_level = max(rw.item_drop_level, mt.item_drop_level)
        self.b.rewards = rw
        self.b.log.say(f"XP +{rw.xp}  Gold +{rw.gold}  Gems +{rw.gems}")
        if rw.item_drop_level:
            self.b.log.say(f"Item drop tier: {rw.item_drop_level}")

    def reset_round_flags(self) -> None:
        for m in self.b.monsters:
            m.acted = False
        for c in self.b.party:
            c.blocking = False
            c.is_shooting = False

    def wake_monsters(self) -> None:
        for m in self.b.monsters:
            if not m.alive():
                continue
            st = int(m.status) & 0xFE
            if self.roll(1, 2) == 1:
                st |= int(MonsterStatus.AWAKE)
            m.status = MonsterStatus(st | int(MonsterStatus.AWAKE))

    def pick_initiative(self) -> tuple[Optional[int], Optional[int], int, int]:
        best_m_spd, best_m_idx = -1, None
        for i, m in self.b.live_monsters:
            if m.acted or not m.can_act():
                continue
            if m.speed > best_m_spd:
                best_m_spd, best_m_idx = m.speed, i

        best_c_spd, best_c_idx = -1, None
        for i, c in self.b.live_party:
            if getattr(c, "_acted", False) or not c.can_act:
                continue
            if c.speed > best_c_spd:
                best_c_spd, best_c_idx = c.speed, i
        return best_c_idx, best_m_idx, best_c_spd, best_m_spd

    def player_capabilities(self, char: Character) -> set[PlayerCommand]:
        cmds = {PlayerCommand.VIEW, PlayerCommand.RUN, PlayerCommand.USE}
        if char.can_act and char.front_rank:
            cmds.add(PlayerCommand.ATTACK)
            cmds.add(PlayerCommand.FIGHT)
            cmds.add(PlayerCommand.BLOCK)
        if char.can_act and (char.char_class == "Archer" or char.front_rank) and char.has_bow:
            cmds.add(PlayerCommand.SHOOT)
        if char.can_act and char.is_caster and not char.silenced and self.pick_combat_spell(char) is not None:
            cmds.add(PlayerCommand.CAST)
        return cmds

    def player_turn(self, idx: int, player_fn: Callable[["Battle", int], PlayerCommand]) -> None:
        char = self.b.party[idx]
        if not char.can_act:
            self.b.log.say(f"{char.name} cannot act.")
            return
        cmd = player_fn(self.b, idx)
        caps = self.player_capabilities(char)
        if cmd not in caps and cmd not in {PlayerCommand.VIEW, PlayerCommand.RUN}:
            fallback = PlayerCommand.FIGHT if PlayerCommand.FIGHT in caps else PlayerCommand.BLOCK
            self.b.log.say(f"{char.name}: invalid {cmd.value}, using {fallback.value}.")
            cmd = fallback

        if cmd == PlayerCommand.VIEW:
            self.b.log.say(
                f"  {char.name}: L{char.level} {char.char_class} "
                f"HP {char.hp_cur}/{char.hp_max} AC {char.ac}"
            )
            return
        if cmd == PlayerCommand.RUN:
            self.try_run()
            return
        if cmd == PlayerCommand.BLOCK:
            char.blocking = True
            self.b.log.say(f"{char.name} blocks.")
            return
        if cmd in (PlayerCommand.ATTACK, PlayerCommand.FIGHT):
            char.is_shooting = False
            target = self.pick_monster_target()
            if target is None:
                return
            self.player_attack_monster(char, target)
            return
        if cmd == PlayerCommand.SHOOT:
            char.is_shooting = True
            target = self.pick_monster_target()
            if target is None:
                return
            self.player_attack_monster(char, target)
            return
        if cmd == PlayerCommand.CAST:
            self.player_cast(char)
            return
        if cmd == PlayerCommand.USE:
            self.b.log.say(f"{char.name} uses an item (not implemented).")

    def spell_costs(self, char: Character, spell: dict) -> tuple[int, int]:
        sp_cost = spell["sp"] * max(1, char.level) if spell["per_level"] else spell["sp"]
        # Special-cost spells keep a low-nibble placeholder in b0; use manual gems.
        gem_cost = spell["manual"][5] if spell.get("special_cost") else spell["gems"]
        return max(0, int(sp_cost)), max(0, int(gem_cost))

    def pick_combat_spell(self, char: Character) -> Optional[dict]:
        if not self.b.spells or not char.spell_school:
            return None
        boss_hp = max((m.hp for _, m in self.b.live_monsters), default=0)
        candidates: list[tuple[tuple[int, int, int], dict]] = []
        for sp in self.b.spells:
            if sp["school"] != char.spell_school:
                continue
            if sp["cast"] == "N":  # non-combat only
                continue
            if char.known_spells and sp["index"] not in char.known_spells:
                continue
            if char.spell_level > 0 and sp["level"] > char.spell_level:
                continue
            sp_cost, gem_cost = self.spell_costs(char, sp)
            if char.sp_cur < sp_cost or char.gems < gem_cost:
                continue
            score = 0
            kind = self.spell_kind(sp)
            if kind in {"offense", "control"}:
                score += 120
            elif kind in {"heal", "cure"}:
                injured = any(c.alive and c.hp_cur < c.hp_max for _, c in self.b.live_party)
                conditioned = any(c.alive and c.condition != CharCondition.NONE for _, c in self.b.live_party)
                if injured or conditioned:
                    score += 110
            elif kind == "buff":
                if self.b.round_num <= 2:
                    score += 90
            if sp["name"] == "Turn Undead":
                if any(self.b.monster_type(m).undead for _, m in self.b.live_monsters):
                    score += 115
                else:
                    score -= 80
            # Boss pressure: favor top-tier single-target nukes.
            if boss_hp >= 300:
                obj = str(sp["manual"][8]).lower()
                if sp["name"] in BOSS_KILL_SPELLS:
                    score += 140
                if "1 monster" in obj:
                    score += 40
                if sp["name"] in {"Flame Arrow", "Electric Arrow", "Acid Stream", "Pain"}:
                    score -= 40
            score += sp["level"] * 4
            score += 2 if sp["cast"] == "C" else 0
            candidates.append(((score, -sp_cost, -gem_cost), sp))
        if not candidates:
            return None
        candidates.sort(key=lambda x: x[0], reverse=True)
        top = candidates[0]
        if top[0][0] <= 0:
            return None
        return top[1]

    def spell_kind(self, spell: dict) -> str:
        name = spell["name"]
        if name in OFFENSIVE_SPELLS or "monster" in str(spell["manual"][8]).lower():
            return "offense"
        if name in CONTROL_SPELLS:
            return "control"
        if name in HEAL_SPELLS:
            return "heal"
        if name in CURE_SPELLS:
            return "cure"
        if name in BUFF_SPELLS:
            return "buff"
        return "utility"

    def monster_spell_targets(self, spell: dict) -> list[int]:
        live = [i for i, _ in self.b.live_monsters]
        if not live:
            return []
        obj = str(spell["manual"][8]).lower()
        if "all" in obj:
            return live
        count = 1
        if "monsters" in obj:
            token = obj.split("monsters", 1)[0].strip().split(" ")[-1]
            if token.isdigit():
                count = max(1, int(token))
        if count >= len(live):
            return live
        self.b.rng.shuffle(live)
        return live[:count]

    def party_spell_targets(self, spell: dict) -> list[int]:
        live = [i for i, _ in self.b.live_party]
        if not live:
            return []
        obj = str(spell["manual"][8]).lower()
        if "entire party" in obj:
            return live
        if "1 character" in obj or "spell caster" in obj:
            # Prefer most-injured living party member.
            best = min(live, key=lambda i: (self.b.party[i].hp_cur / max(1, self.b.party[i].hp_max), self.b.party[i].hp_cur))
            return [best]
        return [live[0]]

    def spell_damage(self, char: Character, spell: dict) -> int:
        scale = max(1, char.level + max(1, char.spell_level))
        base = self.roll(1, max(2, scale)) + (spell["level"] * 2)
        name = spell["name"]
        if name in {"Disintegration", "Implosion", "Inferno", "Holy Word"}:
            base *= 3
        elif name in {"Fire Ball", "Lightning Bolt", "Moon Ray", "Deadly Swarm", "Acid Spray"}:
            base *= 2
        elif name == "Meteor Shower":
            base += self.roll(1, max(2, scale // 2)) * 2
        return max(1, base)

    def apply_spell_to_monsters(self, char: Character, spell: dict, targets: list[int]) -> int:
        hits = 0
        kind = self.spell_kind(spell)
        for idx in targets:
            if idx >= len(self.b.monsters) or not self.b.monsters[idx].alive():
                continue
            mt = self.b.monster_type(self.b.monsters[idx])
            name = spell["name"]
            if name == "Turn Undead":
                if not mt.undead:
                    continue
                dmg = self.spell_damage(char, spell) * 4
                self.apply_damage_to_monster(idx, dmg)
                hits += 1
                continue
            if kind == "control":
                if self.roll100() <= min(95, 45 + char.spell_level * 5):
                    if name in {"Weaken", "Silence"}:
                        self.b.monsters[idx].status |= MonsterStatus.HALF_STRENGTH
                        self.b.log.say(f"  {mt.name} is weakened.")
                    else:
                        self.b.monsters[idx].status |= MonsterStatus.PARALYZED
                        self.b.log.say(f"  {mt.name} is held.")
                    hits += 1
                continue
            dmg = self.spell_damage(char, spell)
            self.apply_damage_to_monster(idx, dmg)
            hits += 1
        return hits

    def apply_spell_to_party(self, char: Character, spell: dict, targets: list[int]) -> int:
        hits = 0
        kind = self.spell_kind(spell)
        for idx in targets:
            if idx >= len(self.b.party):
                continue
            t = self.b.party[idx]
            if not t.alive:
                continue
            if kind == "heal":
                amt = self.roll(1, max(2, char.level + spell["level"])) + spell["level"] * 2
                before = t.hp_cur
                t.hp_cur = min(t.hp_max, t.hp_cur + amt)
                healed = t.hp_cur - before
                self.b.log.say(f"  {t.name} recovers {healed} HP.")
                hits += 1 if healed > 0 else 0
                continue
            if kind == "cure":
                before = t.condition
                t.condition &= ~(CharCondition.ASLEEP | CharCondition.PARALYZED | CharCondition.HURT | CharCondition.SILENCED)
                if spell["name"] in {"Cure Poison", "Cure Disease", "Remove Condition"}:
                    t.condition &= ~CharCondition.AFRAID
                if t.condition != before:
                    self.b.log.say(f"  {t.name} is restored.")
                    hits += 1
                continue
            if kind == "buff":
                bonus = max(1, spell["level"] // 2)
                if spell["name"] in {"Bless", "Heroism", "Holy Bonus", "Frenzy"}:
                    t.might += bonus
                    t.accuracy += bonus
                elif spell["name"] in {"Shield", "Power Shield", "Protection from Magic", "Protection From Elements"}:
                    t.ac += bonus
                self.b.log.say(f"  {t.name} is empowered.")
                hits += 1
        return hits

    def player_cast(self, char: Character) -> None:
        spell = self.pick_combat_spell(char)
        if spell is None:
            self.b.log.say(f"{char.name} has no castable combat spell.")
            return
        sp_cost, gem_cost = self.spell_costs(char, spell)
        if char.sp_cur < sp_cost or char.gems < gem_cost:
            self.b.log.say(f"{char.name} lacks resources for {spell['name']}.")
            return
        char.sp_cur -= sp_cost
        char.gems -= gem_cost
        self.b.log.say(f"{char.name} casts {spell['name']} (SP {sp_cost}, Gems {gem_cost}).")
        kind = self.spell_kind(spell)
        if kind in {"heal", "cure", "buff"}:
            targets = self.party_spell_targets(spell)
            hits = self.apply_spell_to_party(char, spell, targets)
        else:
            targets = self.monster_spell_targets(spell)
            if not targets:
                self.b.log.say("  No valid targets.")
                return
            hits = self.apply_spell_to_monsters(char, spell, targets)
        if hits == 0:
            self.b.log.say("  The spell has no effect.")

    def player_cast_stub(self, char: Character) -> None:
        # Deprecated fallback kept for traceability in case spell tables are unavailable.
        target = self.pick_monster_target()
        if target is None:
            return
        mt = self.b.monster_type(self.b.monsters[target])
        cost = max(1, char.level)
        if char.sp_cur < cost:
            self.b.log.say(f"{char.name} lacks spell points.")
            return
        char.sp_cur -= cost
        dmg = self.roll(1, 6) * max(1, char.level // 2)
        self.b.log.say(f"{char.name} casts a spell at {mt.name} for {dmg} damage.")
        self.apply_damage_to_monster(target, dmg)

    def try_run(self) -> bool:
        avg_spd = sum(c.speed for _, c in self.b.live_party) / max(1, len(self.b.live_party))
        avg_m = sum(self.b.monster_type(m).initiative for _, m in self.b.live_monsters)
        avg_m /= max(1, len(self.b.live_monsters))
        chance = 40 + int(avg_spd - avg_m)
        chance = max(10, min(90, chance))
        if self.roll100() <= chance:
            self.b.outcome = "fled"
            self.b.log.say(f"The party escapes! ({chance}%)")
            return True
        self.b.log.say(f"The party fails to escape ({chance}%).")
        return False

    def run_combat(
        self,
        player_fn: Callable[[Battle, int], PlayerCommand],
    ) -> str:
        b = self.b
        while b.round_num < b.config.max_rounds:
            if not b.live_monsters:
                b.outcome = "victory"
                self.grant_victory_rewards()
                return b.outcome
            if not b.live_party:
                b.outcome = "defeat"
                self.b.log.say("The party has fallen.")
                return b.outcome

            b.round_num += 1
            b.log.say(f"\n=== Round {b.round_num} ===")
            self.reset_round_flags()
            for c in b.party:
                c._acted = False  # type: ignore[attr-defined]
            self.wake_monsters()

            max_turns = len(b.party) + len(b.monsters) + 4
            for _ in range(max_turns):
                if b.outcome == "fled":
                    return b.outcome
                if not b.live_monsters or not b.live_party:
                    break

                c_idx, m_idx, c_spd, m_spd = self.pick_initiative()
                if c_idx is None and m_idx is None:
                    break

                if c_idx is not None and c_spd > m_spd and c_spd > 0:
                    self.player_turn(c_idx, player_fn)
                    b.party[c_idx]._acted = True  # type: ignore[attr-defined]
                elif m_idx is not None:
                    self.monster_turn(m_idx)
                    b.monsters[m_idx].acted = True
                else:
                    break

            if b.outcome == "fled":
                return b.outcome

        b.outcome = "timeout"
        b.log.say("Combat stopped (round limit).")
        return b.outcome


def build_monster_slots(
    type_ids: Iterable[int],
    types: dict[int, MonsterType],
) -> list[MonsterSlot]:
    slots: list[MonsterSlot] = []
    for tid in type_ids:
        if tid not in types:
            raise ValueError(f"unknown monster #{tid}")
        mt = types[tid]
        slots.append(MonsterSlot(
            type_id=tid,
            hp=mt.hp,
            speed=mt.initiative,
            status=MonsterStatus.AWAKE,
            group_counter=mt.spd_lo,
        ))
        if len(slots) >= MAX_MONSTER_SLOTS:
            break
    if not slots:
        raise ValueError("need at least one monster")
    return slots


def auto_player(battle: Battle, idx: int) -> PlayerCommand:
    char = battle.party[idx]
    if not battle.live_monsters:
        return PlayerCommand.VIEW
    engine = CombatEngine(battle)
    caps = engine.player_capabilities(char)
    if PlayerCommand.CAST in caps:
        return PlayerCommand.CAST
    if PlayerCommand.SHOOT in caps and char.char_class == "Archer":
        return PlayerCommand.SHOOT
    if PlayerCommand.FIGHT in caps:
        return PlayerCommand.FIGHT
    if PlayerCommand.BLOCK in caps:
        return PlayerCommand.BLOCK
    return PlayerCommand.VIEW


def interactive_player(battle: Battle, idx: int) -> PlayerCommand:
    char = battle.party[idx]
    engine = CombatEngine(battle)
    caps = engine.player_capabilities(char)
    print(f"\n{char.name} ({char.char_class}) HP {char.hp_cur}/{char.hp_max} "
          f"SP {char.sp_cur}/{char.sp_max}")
    print("Commands:", " ".join(c.value for c in sorted(caps, key=lambda x: x.value)))
    while True:
        raw = input("> ").strip().upper()[:1]
        if not raw:
            continue
        try:
            cmd = PlayerCommand(raw)
        except ValueError:
            print("Unknown command.")
            continue
        if cmd in caps or cmd in {PlayerCommand.RUN, PlayerCommand.VIEW}:
            return cmd
        print("Not available.")


def print_status(battle: Battle) -> None:
    print("\n-- Monsters --")
    for i, m in enumerate(battle.monsters):
        if not m.alive():
            continue
        mt = battle.monster_type(m)
        print(f"  [{i}] {mt.name}: {m.hp} HP")
    print("-- Party --")
    for i, c in enumerate(battle.party):
        if not c.alive:
            continue
        print(f"  [{i}] {c.name}: {c.hp_cur}/{c.hp_max} HP  SP {c.sp_cur}")


def run_simulation(args: argparse.Namespace) -> int:
    types = load_monster_types(_find_dat("monsters.dat", args.monsters_dat))
    items = load_items(str(_find_dat("items.dat", args.items_dat)))
    try:
        spells_path = _find_dat("spells.dat", args.spells_dat)
        spells = load_spells_dat(str(spells_path))
    except FileNotFoundError:
        spells = []

    if args.test_party:
        party = make_test_party()
    elif args.roster:
        party = load_party_from_roster(Path(args.roster), items, args.party_slots)
    else:
        party = make_test_party()

    type_ids = [int(x, 0) for x in args.monsters.split(",")]
    monsters = build_monster_slots(type_ids, types)
    battle = Battle(
        party=party,
        monsters=monsters,
        types=types,
        spells=spells,
        rng=random.Random(args.seed),
        config=CombatConfig(max_rounds=args.max_rounds),
    )
    engine = CombatEngine(battle)
    print_status(battle)
    player_fn = auto_player if args.auto else interactive_player
    outcome = engine.run_combat(player_fn)
    print_status(battle)
    print(f"\nOutcome: {outcome}")
    return 0 if outcome in ("victory", "fled") else 1


def run_test() -> int:
    types = make_test_types()
    party = make_test_party()
    monsters = build_monster_slots([1, 1, 5], types)
    try:
        spells_path = _find_dat("spells.dat", None)
        spells = load_spells_dat(str(spells_path))
    except FileNotFoundError:
        spells = []
    battle = Battle(
        party=party,
        monsters=monsters,
        types=types,
        spells=spells,
        rng=random.Random(0),
        config=CombatConfig(max_rounds=50),
    )
    CombatEngine(battle).run_combat(auto_player)
    assert battle.outcome == "victory", battle.outcome
    assert battle.rewards.xp > 0
    print("mm2_combat test OK")
    return 0


def _add_battle_args(p: argparse.ArgumentParser) -> None:
    p.add_argument("--monsters", "-m", required=True,
                   help="Comma-separated monster type ids (e.g. 1,1,5)")
    p.add_argument("--roster", help="roster.dat path")
    p.add_argument("--party-slots",
                   help="Optional roster slots (comma-separated, e.g. 0,1,2,3); overrides in-party bits")
    p.add_argument("--test-party", action="store_true")
    p.add_argument("--monsters-dat", help="Path to monsters.dat")
    p.add_argument("--items-dat", help="Path to items.dat")
    p.add_argument("--spells-dat", help="Path to spells.dat")
    p.add_argument("--seed", type=int, default=None)
    p.add_argument("--max-rounds", type=int, default=200)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="MM2 combat emulator (ASM-traced)")
    sub = parser.add_subparsers(dest="cmd", required=True)

    sim = sub.add_parser("simulate", help="Run a battle")
    _add_battle_args(sim)
    sim.add_argument("--auto", action="store_true")

    interactive = sub.add_parser("interactive", help="Prompt for party commands")
    _add_battle_args(interactive)

    sub.add_parser("test", help="Smoke test without .dat files")

    args = parser.parse_args(argv[1:])
    if args.cmd == "test":
        return run_test()
    args.auto = getattr(args, "auto", False) if args.cmd == "simulate" else False
    return run_simulation(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
