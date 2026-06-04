#!/usr/bin/env python3
"""CLI helpers for mm2_create_character.c (stat roll / eligibility smoke tests)."""

from __future__ import annotations

import argparse
import ctypes
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DECOMP = os.path.join(ROOT, "EXTRACTED", "decomp")

CLASSES = [
    "Knight", "Paladin", "Archer", "Cleric", "Sorcerer", "Robber", "Ninja", "Barbarian"
]
STATS = ["Mgt", "Int", "Per", "End", "Spd", "Acc", "Lck"]


class Mm2CreateStats(ctypes.Structure):
    _fields_ = [
        ("might", ctypes.c_uint8),
        ("intelligence", ctypes.c_uint8),
        ("personality", ctypes.c_uint8),
        ("endurance", ctypes.c_uint8),
        ("speed", ctypes.c_uint8),
        ("accuracy", ctypes.c_uint8),
        ("luck", ctypes.c_uint8),
    ]


class Mm2PendingCharacter(ctypes.Structure):
    _fields_ = [
        ("rolled", Mm2CreateStats),
        ("modified", Mm2CreateStats),
        ("class_id", ctypes.c_int8),
        ("race", ctypes.c_int8),
        ("alignment", ctypes.c_int8),
        ("sex", ctypes.c_int8),
        ("name", ctypes.c_char * 12),
    ]


def load_lib():
    if sys.platform == "win32":
        path = os.path.join(DECOMP, "mm2_create_character.dll")
        if not os.path.isfile(path):
            raise SystemExit(
                "Build mm2_create_character.dll first (not wired in CMake yet); "
                "use the C API from game/mm2_codecs instead."
            )
        return ctypes.CDLL(path)
    path = os.path.join(DECOMP, "libmm2_create_character.so")
    return ctypes.CDLL(path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    try:
        lib = load_lib()
    except SystemExit as exc:
        print(exc)
        rng = args.seed & 0xFFFFFFFF

        def roll():
            nonlocal rng
            rng = (rng * 214013 + 2531011) & 0xFFFFFFFF
            d20 = (rng >> 16) % 20 + 1
            rating = min(21, d20 + 2)
            return rating

        stats = [roll() for _ in range(7)]
        print("rolled", dict(zip(STATS, stats)))
        mins = [12, 13, 13, 13, 13, 13, 13, 15]
        for i, cls in enumerate(CLASSES):
            ok = stats[0] >= mins[i] if i in (0, 7) else stats[[1, 2, 2, 2, 5, 4, 4][i]] >= mins[i]
            if i == 6:
                ok = stats[4] >= 13 and stats[5] >= 13
            print(f"  {i + 1} {cls}: {'yes' if ok else '-'}")
        return

    rng = ctypes.c_uint32(args.seed)
    stats = Mm2CreateStats()
    lib.mm2_create_roll_stats(ctypes.byref(stats), ctypes.byref(rng))
    vals = [stats.might, stats.intelligence, stats.personality, stats.endurance, stats.speed, stats.accuracy, stats.luck]
    print("rolled", dict(zip(STATS, vals)))
    for i, cls in enumerate(CLASSES):
        ok = lib.mm2_create_class_eligible(i, ctypes.byref(stats))
        print(f"  {i + 1} {cls}: {'yes' if ok else '-'}")


if __name__ == "__main__":
    main()
