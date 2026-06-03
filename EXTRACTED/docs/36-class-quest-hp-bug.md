# Class-quest reward bugs (Cleric 32,000 HP report)

ASM trace of the Mount Farview / Juror reward handler and related HP helpers.
See also [`06-roster-format.md`](06-roster-format.md) (`$79` '+' bit), FAQ §4-11.

Codec: `tools/decode_class_quest.py` dumps embedded tables.

## Confirmed: '+' mechanism

| Item | Detail |
|------|--------|
| Display | Character sheet @ **`0x3A9A`**: `$79` bit **`0x80`** → `'+'` string index **`0x2B`** |
| Write | **`0x9FE0`**: `or.b d1, $79(a0)` inside **`0x9D76`** reward loop |
| Mask table | **`A4-$7148`** (data hunk **`0xEB6`**) — per-index OR byte |
| Offset table | **`A4-$7154`** (data hunk **`0xEAA`**) — **misused as address** @ **`0x9FBE`** |

## Bug 1: wrong roster pointer @ `0x9FBE` (high confidence)

```asm
009FB4  lea.l   -$7154(a4), a0
009FBA  move.b  (a0,d0.l), d1      ; table byte used as OFFSET, not message id
009FBE  movea.l d1, a0               ; BUG: a0 = offset, not roster base
009FC0  adda.l  -$e(a5), a0          ; a0 = roster + offset
009FE0  or.b    d1, $79(a0)          ; mask from -$7148, wrong base
```

**Intended:** `movea.l -$e(a5), a0` then `or.b d1, $79(a0)`.

**Actual:** OR targets **`roster + table_byte + $79`**. Table **`-$7154`**
starts with **`[5,5,5,5,5,5,6,6,…]`**, so typical writes land at **`$7E`**
(padding) or spill into the **next character record** when the offset is
large (**`17`**, **`20`**, **`21`**, … appear later in the table).

Dump indices vs corrupt offset:

```bash
python tools/decode_class_quest.py
```

## Bug 2: training HP path writes `$5C` not `$5E` @ `0x7D34`

MaxHP on the sheet reads **`$5E`** @ **`0x3AC8`**. Training distributor
**`0x7CB0`** (via **`-$7D22`**, **`0x9BCA`**) does:

```asm
007D34  add.w   d0, $5c(a0)    ; $5C = gems in roster layout, not MaxHP
```

If HP restoration was intended, target should be **`$5E`** (and likely sync
**`$74`**).

## Bug 3: XP table long added to gold @ `0x9F98`

```asm
009F98  add.l   d0, $66(a0)    ; $66 = gold, not $62 XP
```

FAQ promises **5M experience**; this path adds **`A4-$7184`** dwords to gold.

## Why 32,000 (`0x7D00`)?

No **`0x7D00`** immediate in the reward path. Best numeric match: FAQ level-31
Group-B XP threshold (**32000**). A stray **16-bit word** landing in **`$5E`**
would display as 32,000 HP. Needs a **runtime roster dump** at claim time to
confirm **`$5E`/`$74`** vs padding/spill from Bug 1.

## Why only Cleric? Why only after '+'?

**Not Cleric-only code.** Handler **`0x9D76`** is shared by all eight classes.
Class is taken from the **ticket item** in equip slot **`$3A`** (`0xD0`…`0xD7` =
Knight…Barbarian), not from roster **`$0F`**. Cleric = ticket **`0xD3`**.

**Only after quest** because this routine runs **once**, on **Mount Farview
Juror turn-in** when the matching ticket is present. Nothing in normal
exploration/training calls **`0x9FE0`**. “After '+'” ≈ “after Farview reward.”

**Why reports cluster on Cleric** (selection bias, not a separate code path):

- Cleric quest is often done **late** (high level → FAQ level-31 Group-B XP step
  is **32000**, same number as the HP report).
- Party order: offset bug writes at **`roster + off + $79`**; front-line classes
  processed first can **spill into the next slot** (often the healer).
- Cleric is **Group A** XP; the 32000 echo is still likely coincidence unless a
  dump shows **`$5E = 0x7D00`**.

**'+' vs mask `0x08` paradox:** Sheet `'+'` needs **`$79` bit `0x80`**. Table
row index 3 (Cleric-shaped) has mask **`0x08`**, not **`0x80`**, and Bug 1
writes to **`$7E`** (padding), not **`$79`**. No dumped row has **`off=0`** and
**`mask & 0x80`**. So either **`-7B54`** maps to a different index at runtime,
the visible `'+'` came from another save/session, or guild low bits were mistaken
for `'+'`. **`0x9FE0`** is the only OR-to-`$79` site in this path.

## Suggested fixes (original binary patch points)

| Pri | Address | Fix |
|-----|---------|-----|
| 1 | **`0x9FBE`** | `movea.l -$e(a5),a0` instead of `movea.l d1,a0` |
| 2 | **`0x7D34`** | Use **`$5E`** (and **`$74`**) if this path restores HP |
| 3 | **`0x9F98`** | Use **`$62`** if reward is XP |

## Runtime confirmation checklist

1. Hex dump Cleric record at reward: **`$5C`–`$5F`**, **`$74`–`$75`**, **`$79`**, **`$7A`–`$81`**, next char **`$5E`**.
2. Log table index **`d0`** @ **`0x9FBA`** (ticket × **`-$7B54`** selector 3 + town).
3. Character level at completion (31 → strengthens **`0x7D00`** table link).
