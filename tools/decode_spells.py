#!/usr/bin/env python3
"""Dump spells.dat divided into each spell, with cost/gems/cast type.

Each spell is 2 bytes (96 spells: Sorcerer 0..47, Cleric 48..95). The decode is
cross-validated against the MM2 manual (Appendix B):

    byte0:  bit 0x80 = non-combat only, bit 0x40 = combat only (neither=anytime)
            bit 0x10 = special/computed gem cost (hard-coded in game code)
            low nibble = gem cost (0..15)
    byte1:  low nibble = flat SP cost; 0 => per caster level
            high nibble = per-level SP multiplier (the X in "X/L")

Usage:  python tools/decode_spells.py [spells.dat] [--check]
"""
from __future__ import annotations

import sys

import mm2_spells as ms


def dump(path: str):
    recs = ms.load_spells_dat(path)
    school = None
    for r in recs:
        if r['school'] != school:
            school = r['school']
            print('\n=== %s SPELLS ===' % ('SORCERER' if school == 'S' else 'CLERIC'))
        tag = '%s%d/%d' % (r['school'], r['level'], r['number'])
        cast = ms.CAST_NAMES[r['cast']]
        if r['outdoor']:
            cast += ', Outdoor'
        flag = '  [special-cost]' if r['special_cost'] else ''
        print('%-6s %-22s %02X %02X  %-16s  %-22s  obj: %s%s' % (
            tag, r['name'], r['b0'], r['b1'], r['cost'], cast, r['manual'][8], flag))


def check(path: str):
    """Compare file-decoded cost/gems/cast with the manual reference."""
    recs = ms.load_spells_dat(path)
    g = sp = c = 0
    bad = []
    for r in recs:
        _, _, _, m_sp, m_pl, m_gems, m_cast, _, _ = r['manual']
        gem_ok = (r['gems'] == m_gems) or (m_gems > 15)
        sp_ok = (r['sp'] == m_sp and r['per_level'] == bool(m_pl)) or m_gems > 15 or m_sp > 15
        cast_ok = (r['cast'] == m_cast)
        g += gem_ok; sp += sp_ok; c += cast_ok
        if not (gem_ok and sp_ok and cast_ok):
            bad.append((r['school'], r['level'], r['number'], r['name'],
                        '%02X%02X' % (r['b0'], r['b1']),
                        'gem %d/%d' % (r['gems'], m_gems),
                        'sp %d%s/%d%s' % (r['sp'], 'L' if r['per_level'] else '',
                                          m_sp, 'L' if m_pl else ''),
                        'cast %s/%s' % (r['cast'], m_cast)))
    n = len(recs)
    print('gems %d/%d   sp %d/%d   cast %d/%d  (file vs manual)' % (g, n, sp, n, c, n))
    for b in bad:
        print('  mismatch:', b)


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    path = args[0] if args else 'spells.dat'
    dump(path)
    if '--check' in sys.argv:
        print()
        check(path)
