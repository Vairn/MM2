#!/usr/bin/env python3
"""Emit canvases/mm2-spell-asm-review.canvas.tsx from audit JSON."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
rows = json.loads((ROOT / "EXTRACTED" / "tmp_spell_audit_compact.json").read_text(encoding="utf-8"))

fixes = {
    "S3/3": 0xB9C4,
    "S4/5": 0xBB5C,
    "S4/6": 0xBB86,
    "S6/2": 0xBCB8,
    "S8/4": 0xBEBA,
    "C1/3": 0xBFC4,
    "C1/7": 0xBFEE,
    "C9/1": 0xC6AE,
    "C9/2": 0xC752,
}
for r in rows:
    if r["tag"] in fixes:
        r["stub"] = fixes[r["tag"]]
    if r["tag"] in ("S3/3", "S4/5", "S4/6", "S6/2", "S8/4", "C1/3", "C9/1"):
        r["d390"] = True
    if r["tag"] == "C3/3":
        r["d390"] = False
        r["stub"] = None
    if r["tag"] == "C9/1":
        r["sev"] = "ok"
        r["issues"] = []
    if r["sev"] == "note" and r["when"] in ("explore", "anytime"):
        r["sev"] = "ok"
        r["issues"] = []


def hx(s):
    return "null" if s is None else hex(int(s))


items = []
for r in rows:
    items.append(
        "{"
        f"tag:{json.dumps(r['tag'])},name:{json.dumps(r['name'])},when:{json.dumps(r['when'])},"
        f"outdoor:{str(r['outdoor']).lower()},path:{json.dumps(r['path'])},expl:{str(r['expl']).lower()},"
        f"d390:{str(r['d390']).lower()},stub:{hx(r['stub'])},sev:{json.dumps(r['sev'])}"
        "}"
    )
spell_lit = ",\n  ".join(items)

canvas = f"""import {{
  Divider,
  Grid,
  H1,
  H2,
  Stack,
  Table,
  Text,
  Badge,
  Card,
  CardHeader,
  CardBody,
  Stat,
  useHostTheme,
}} from 'cursor/canvas';

type Sev = 'ok' | 'gate' | 'note';
type Spell = {{
  tag: string;
  name: string;
  when: 'combat' | 'explore' | 'anytime';
  outdoor: boolean;
  path: string;
  expl: boolean;
  d390: boolean;
  stub: number | null;
  sev: Sev;
}};

const SPELLS: Spell[] = [
  {spell_lit}
];

function toneFor(sev: Sev): 'success' | 'danger' | 'warning' | 'neutral' {{
  if (sev === 'gate') return 'danger';
  if (sev === 'note') return 'warning';
  return 'success';
}}

function whenBadge(when: Spell['when']) {{
  if (when === 'combat') return <Badge tone="danger">combat</Badge>;
  if (when === 'explore') return <Badge tone="info">explore</Badge>;
  return <Badge tone="neutral">anytime</Badge>;
}}

export default function SpellAsmReview() {{
  const theme = useHostTheme();
  const gates = SPELLS.filter((s) => s.sev === 'gate');
  const combatDat = SPELLS.filter((s) => s.when === 'combat').length;
  const exploreDat = SPELLS.filter((s) => s.when === 'explore').length;
  const anytimeDat = SPELLS.filter((s) => s.when === 'anytime').length;
  const outdoor = SPELLS.filter((s) => s.outdoor).length;
  const remExplOk = SPELLS.filter((s) => s.expl).length;
  const d390 = SPELLS.filter((s) => s.d390).length;

  const filter = (school: 'S' | 'C') => SPELLS.filter((s) => s.tag.startsWith(school));

  const tableFor = (list: Spell[]) => (
    <Table
      stickyHeader
      striped
      headers={{['Spell', 'spells.dat', 'Outdoor', 'Remake path', 'Explore OK', 'D390', 'Stub']}}
      columnAlign={{['left', 'left', 'center', 'left', 'center', 'center', 'left']}}
      rowTone={{list.map((s) => toneFor(s.sev))}}
      rows={{list.map((s) => [
        <Text key="n" weight="semibold">{{s.tag}} {{s.name}}</Text>,
        whenBadge(s.when),
        s.outdoor ? <Badge tone="warning">yes</Badge> : <Text tone="secondary">—</Text>,
        <Text key="p">{{s.path}}</Text>,
        s.expl ? <Badge tone="success">yes</Badge> : <Badge tone="neutral">fail</Badge>,
        s.d390 ? <Badge tone="warning">yes</Badge> : <Text tone="secondary">—</Text>,
        <Text key="s" tone="secondary">
          {{s.stub != null ? `0x${{s.stub.toString(16).toUpperCase()}}` : '—'}}
        </Text>,
      ])}}
    />
  );

  return (
    <Stack gap={{20}} style={{{{ padding: 16, maxWidth: 1100 }}}}>
      <Stack gap={{6}}>
        <H1>MM2 spell ASM review</H1>
        <Text tone="secondary">
          Capstone stubs + spells.dat gates vs CombatSession::resolvePlayerCast (explore + combat).
          Sources: EXTRACTED/mm2.capstone.asm, spells.dat, docs/19 + 26.
        </Text>
      </Stack>

      <Grid columns={{5}} gap={{12}}>
        <Stat value={{String(SPELLS.length)}} label="Spells audited" />
        <Stat value={{String(gates.length)}} label="Explore gate bugs" />
        <Stat value={{`${{combatDat}}/${{exploreDat}}/${{anytimeDat}}`}} label="dat combat/explore/any" />
        <Stat value={{String(outdoor)}} label="Outdoor-only" />
        <Stat value={{String(d390)}} label="ASM D390 stubs" />
      </Grid>

      <Card>
        <CardHeader>Verdict</CardHeader>
        <CardBody>
          <Stack gap={{8}}>
            <Text>
              All 96 flats have a remake path (direct / letterPick / autoAoE). Combat letter and auto-AoE
              spells correctly fail outside combat. The real gap is combat buffs that still apply in exploration.
            </Text>
            <Text tone="secondary">
              ASM $D390 is not a pure in-combat bit: item-cast (-$3F0C) bypasses; else Return-to-cast Enter
              confirm + tile bit1 at -$55D6. Remake approximates it as exploration_cast_ fail for damage leaves,
              but skips it for several buff counters. Remake does not enforce spells.dat outdoor (byte1 0x80) at cast time.
            </Text>
            <Text tone="secondary">
              Explore entry: 0x6E30 picker then $CD90. Combat: 0x11A68 picker, school gate $13708, then $CD90 / $CFD0.
            </Text>
          </Stack>
        </CardBody>
      </Card>

      <H2>High: combat-only buffs succeed in explore</H2>
      <Text tone="secondary">
        spells.dat byte0 bit 0x40 = combat-only. These remake direct handlers bump GS counters or run combat FX
        without an explore fail.
      </Text>
      <Table
        headers={{['Spell', 'ASM stub', 'Effect', 'Remake today']}}
        rows={{[
          ['S3/3 Invisibility', '0xB9C4', 'D390 then +-$799C', 'applies in explore'],
          ['S4/5 Shield', '0xBB5C', 'D390 then +-$799B', 'applies; comment wrongly cites 0xBB84 rts'],
          ['S8/4 Power Shield', '0xBEBA', 'D390 then power-shield ctr', 'applies in explore'],
          ['C1/3 Bless', '0xBFC4', 'D390 then +-$799D', 'applies; comment wrongly cites 0xBFEC rts'],
          ['C1/7 Turn Undead', '0xBFEE to 0xC028', 'D390 inside C028; undead kill', 'runs FX in explore'],
          ['C2/2 Heroism', '0xC566 family', 'D390 + party pick', 'party pick works in explore'],
          ['C4/6 Holy Bonus', 'combat leaf', 'level>>1 to -$7999', 'applies in explore'],
          ['C5/3 Frenzy', 'combat leaf', 'party frenzy', 'applies in explore'],
          ['C9/2 Holy Word', '0xC752', 'turn-undead arg0', 'runs FX in explore'],
        ]}}
        rowTone={{Array(9).fill('danger')}}
      />

      <H2>Matched combat gates</H2>
      <Table
        headers={{['Spell', 'ASM', 'Remake']}}
        rows={{[
          ['S4/6 Time Distortion', '0xBB86 D390 + btst#3 -$5600', 'explore fail + bit3 fail'],
          ['S6/2 Entrapment', '0xBCB8 D390 + btst#0 -$5600', 'explore fail + bit0 fail'],
          ['C9/1 Divine Intervention', '0xC6AE D390 + -$51A once', 'explore fail + already-used'],
          ['Letter / auto AoE (41 spells)', 'D43C/D464 or auto table', 'exploration_cast_ fails'],
        ]}}
        rowTone={{Array(4).fill('success')}}
      />

      <H2>System gaps</H2>
      <Table
        headers={{['Gap', 'Evidence', 'Impact']}}
        rows={{[
          [
            'No spells.dat when/outdoor gate',
            'resolvePlayerCast never reads byte0 0x40/0x80 or byte1 0x80',
            'Outdoor spells castable indoors; combat buffs castable in explore',
          ],
          [
            'D390 simplified',
            'Capstone 0xD390: -$3F0C OR Enter confirm + -$55D6 bit1',
            'Item-cast bypass OK; explore confirm path not modeled for buffs',
          ],
          [
            'Doc explore dispatcher',
            '0x6E94 jsr $cd90 (not $cdb8)',
            'docs/26 still mentions CDB8 for explore in places',
          ],
        ]}}
        rowTone={{['warning', 'warning', 'info']}}
      />

      <Divider />
      <H2>Full matrix — Sorcerer</H2>
      <Text tone="secondary">
        Red row marker = explore gate mismatch. Remake explore-OK count: {{remExplOk}}/96.
      </Text>
      {{tableFor(filter('S'))}}

      <H2>Full matrix — Cleric</H2>
      {{tableFor(filter('C'))}}

      <Text tone="secondary" style={{{{ color: theme.text.secondary }}}}>
        Generated from tools/_audit_spell_remake.py + Capstone D390 sites. Stub column uses sparse mapping where
        reliable; High table addresses are Capstone hand-fixed.
      </Text>
    </Stack>
  );
}}
"""

out = Path(
    r"C:\Users\Adam Templeton\.cursor\projects\c-20260421-D-REC-development-MM2\canvases\mm2-spell-asm-review.canvas.tsx"
)
out.write_text(canvas, encoding="utf-8")
print("wrote", out)
print("bytes", out.stat().st_size)
print("gates", sum(1 for r in rows if r["sev"] == "gate"))
