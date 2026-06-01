#!/usr/bin/env python3
"""
Resolve MM2 copy-protection prompts against a scanned manual PDF.

Input:
  - copyprot_words.json (decoded challenge records)
  - "Might and Magic II - Gates to Another World - Manual-ENG.pdf"

Output:
  - JSON with resolved page/paragraph/line text + candidate word
  - CSV for quick inspection
"""

from __future__ import annotations

import argparse
import csv
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path

import fitz  # PyMuPDF


@dataclass
class Candidate:
    strategy: str
    word: str
    line_text: str
    checksum_raw_sum: int
    checksum_upper_sum: int
    checksum_alpha_sum: int
    delta_upper: int


@dataclass
class ResolvedEntry:
    index: int
    page: int
    paragraph: int
    line: int
    checksum: int
    pdf_page_index: int
    side: str
    resolved: bool
    paragraph_count: int
    line_count_in_paragraph: int
    line_text: str
    candidate_word: str
    strategy: str
    checksum_raw_sum: int
    checksum_upper_sum: int
    checksum_alpha_sum: int
    delta_upper: int
    candidates: list[Candidate]


def manual_page_to_pdf_page(page_no: int) -> tuple[int, str]:
    # Empirically for this scan:
    # pdf index 24 has manual pages 44(left) and 45(right)
    idx = (page_no + 4) // 2
    side = "left" if (page_no % 2 == 0) else "right"
    return idx, side


def extract_half_blocks(page: fitz.Page, side: str) -> list[tuple[float, float, float, float, str]]:
    w = page.rect.width
    h = page.rect.height
    clip = fitz.Rect(0, 0, w / 2.0, h) if side == "left" else fitz.Rect(w / 2.0, 0, w, h)
    blocks = page.get_text("blocks", clip=clip)
    out = []
    for b in blocks:
        x0, y0, x1, y1, txt, *_ = b
        if not txt.strip():
            continue
        out.append((x0, y0, x1, y1, txt))
    return out


def blocks_to_paragraphs(blocks: list[tuple[float, float, float, float, str]]) -> list[list[str]]:
    if not blocks:
        return []
    xmin = min(b[0] for b in blocks)
    xmax = max(b[2] for b in blocks)
    mid = (xmin + xmax) / 2.0
    c0 = [b for b in blocks if ((b[0] + b[2]) / 2.0) < mid]
    c1 = [b for b in blocks if ((b[0] + b[2]) / 2.0) >= mid]
    if not c0 or not c1:
        cols = [sorted(blocks, key=lambda b: (b[1], b[0]))]
    else:
        cols = [sorted(c0, key=lambda b: (b[1], b[0])), sorted(c1, key=lambda b: (b[1], b[0]))]

    paras: list[list[str]] = []
    for col in cols:
        for _, _, _, _, txt in col:
            lines = [ln.strip() for ln in txt.splitlines() if ln.strip()]
            lines = [ln for ln in lines if re.fullmatch(r"\d+", ln) is None]
            if lines:
                paras.append(lines)
    return paras


def first_word(line_text: str) -> str:
    m = re.search(r"[A-Za-z']+", line_text)
    return m.group(0) if m else ""


def score_word(word: str, expected_checksum: int) -> tuple[int, int, int, int]:
    raw = sum(ord(c) for c in word) & 0xFFFF
    upper = sum(ord(c.upper()) for c in word) & 0xFFFF
    alpha = sum((ord(c.upper()) - ord("A") + 1) for c in word if c.isalpha()) & 0xFFFF
    delta = abs(upper - (expected_checksum & 0xFFFF))
    return raw, upper, alpha, delta


def build_candidates(paras: list[list[str]], paragraph: int, line: int, checksum: int) -> list[Candidate]:
    out: list[Candidate] = []
    pidx = paragraph - 1
    lidx = line - 1
    if pidx < 0 or pidx >= len(paras):
        return out
    para = paras[pidx]
    if 0 <= lidx < len(para):
        line_text = para[lidx]
        word = first_word(line_text)
        if word:
            raw, upper, alpha, delta = score_word(word, checksum)
            out.append(
                Candidate(
                    strategy="line-index-first-word",
                    word=word,
                    line_text=line_text,
                    checksum_raw_sum=raw,
                    checksum_upper_sum=upper,
                    checksum_alpha_sum=alpha,
                    delta_upper=delta,
                )
            )

    para_text = " ".join(para)
    words = re.findall(r"[A-Za-z']+", para_text)
    if 0 <= lidx < len(words):
        word = words[lidx]
        raw, upper, alpha, delta = score_word(word, checksum)
        out.append(
            Candidate(
                strategy="word-index-in-paragraph",
                word=word,
                line_text=para_text,
                checksum_raw_sum=raw,
                checksum_upper_sum=upper,
                checksum_alpha_sum=alpha,
                delta_upper=delta,
            )
        )

    out.sort(key=lambda c: (c.delta_upper, c.strategy))
    # De-dup identical words while keeping best score.
    dedup: list[Candidate] = []
    seen: set[tuple[str, str]] = set()
    for c in out:
        k = (c.strategy, c.word.lower())
        if k in seen:
            continue
        seen.add(k)
        dedup.append(c)
    return dedup


def resolve_entry(doc: fitz.Document, entry: dict) -> ResolvedEntry:
    idx, side = manual_page_to_pdf_page(int(entry["page"]))
    if idx < 0 or idx >= len(doc):
        return ResolvedEntry(
            index=entry["index"],
            page=entry["page"],
            paragraph=entry["paragraph"],
            line=entry["line"],
            checksum=entry["checksum"],
            pdf_page_index=idx,
            side=side,
            resolved=False,
            paragraph_count=0,
            line_count_in_paragraph=0,
            line_text="",
            candidate_word="",
            strategy="",
            checksum_raw_sum=0,
            checksum_upper_sum=0,
            checksum_alpha_sum=0,
            delta_upper=0,
            candidates=[],
        )

    page = doc[idx]
    paras = blocks_to_paragraphs(extract_half_blocks(page, side))

    pidx = int(entry["paragraph"]) - 1
    lidx = int(entry["line"]) - 1
    if pidx < 0 or pidx >= len(paras):
        return ResolvedEntry(
            index=entry["index"],
            page=entry["page"],
            paragraph=entry["paragraph"],
            line=entry["line"],
            checksum=entry["checksum"],
            pdf_page_index=idx,
            side=side,
            resolved=False,
            paragraph_count=len(paras),
            line_count_in_paragraph=0,
            line_text="",
            candidate_word="",
            strategy="",
            checksum_raw_sum=0,
            checksum_upper_sum=0,
            checksum_alpha_sum=0,
            delta_upper=0,
            candidates=[],
        )

    para = paras[pidx]
    candidates = build_candidates(paras, int(entry["paragraph"]), int(entry["line"]), int(entry["checksum"]))
    if lidx < 0 or lidx >= len(para):
        best = candidates[0] if candidates else None
        return ResolvedEntry(
            index=entry["index"],
            page=entry["page"],
            paragraph=entry["paragraph"],
            line=entry["line"],
            checksum=entry["checksum"],
            pdf_page_index=idx,
            side=side,
            resolved=False,
            paragraph_count=len(paras),
            line_count_in_paragraph=len(para),
            line_text=best.line_text if best else "",
            candidate_word=best.word if best else "",
            strategy=best.strategy if best else "",
            checksum_raw_sum=best.checksum_raw_sum if best else 0,
            checksum_upper_sum=best.checksum_upper_sum if best else 0,
            checksum_alpha_sum=best.checksum_alpha_sum if best else 0,
            delta_upper=best.delta_upper if best else 0,
            candidates=candidates,
        )

    text = para[lidx]
    best = candidates[0] if candidates else None
    return ResolvedEntry(
        index=entry["index"],
        page=entry["page"],
        paragraph=entry["paragraph"],
        line=entry["line"],
        checksum=entry["checksum"],
        pdf_page_index=idx,
        side=side,
        resolved=True,
        paragraph_count=len(paras),
        line_count_in_paragraph=len(para),
        line_text=best.line_text if best else text,
        candidate_word=best.word if best else first_word(text),
        strategy=best.strategy if best else "line-index-first-word",
        checksum_raw_sum=best.checksum_raw_sum if best else 0,
        checksum_upper_sum=best.checksum_upper_sum if best else 0,
        checksum_alpha_sum=best.checksum_alpha_sum if best else 0,
        delta_upper=best.delta_upper if best else 0,
        candidates=candidates,
    )


def main() -> None:
    ap = argparse.ArgumentParser(description="Resolve MM2 copy-protection word prompts against manual PDF.")
    ap.add_argument(
        "--copyprot-json",
        default=r"c:\_20260421_\D-REC\development\MM2\copyprot_words.json",
        help="Decoded copy-protection entries JSON",
    )
    ap.add_argument(
        "--manual-pdf",
        default=r"c:\_20260421_\D-REC\development\MM2\Might and Magic II - Gates to Another World - Manual-ENG.pdf",
        help="Manual PDF path",
    )
    ap.add_argument(
        "--json-out",
        default=r"c:\_20260421_\D-REC\development\MM2\copyprot_words_resolved.json",
        help="Resolved output JSON",
    )
    ap.add_argument(
        "--csv-out",
        default=r"c:\_20260421_\D-REC\development\MM2\copyprot_words_resolved.csv",
        help="Resolved output CSV",
    )
    args = ap.parse_args()

    entries = json.loads(Path(args.copyprot_json).read_text(encoding="utf-8"))["entries"]
    doc = fitz.open(args.manual_pdf)
    resolved = [resolve_entry(doc, e) for e in entries]

    Path(args.json_out).write_text(
        json.dumps({"entries": [asdict(r) for r in resolved]}, indent=2),
        encoding="utf-8",
    )

    with Path(args.csv_out).open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "index",
                "page",
                "paragraph",
                "line",
                "checksum_hex",
                "resolved",
                "candidate_word",
                "strategy",
                "line_text",
                "pdf_page_index",
                "side",
                "paragraph_count",
                "line_count_in_paragraph",
                "checksum_raw_sum",
                "checksum_upper_sum",
                "checksum_alpha_sum",
                "delta_upper",
            ]
        )
        for r in resolved:
            w.writerow(
                [
                    r.index,
                    r.page,
                    r.paragraph,
                    r.line,
                    f"0x{r.checksum:04X}",
                    int(r.resolved),
                    r.candidate_word,
                    r.strategy,
                    r.line_text,
                    r.pdf_page_index,
                    r.side,
                    r.paragraph_count,
                    r.line_count_in_paragraph,
                    r.checksum_raw_sum,
                    r.checksum_upper_sum,
                    r.checksum_alpha_sum,
                    r.delta_upper,
                ]
            )

    ok = sum(1 for r in resolved if r.resolved)
    print(f"resolved {ok}/{len(resolved)} entries")
    for r in resolved[:20]:
        print(
            f"[{r.index:02d}] p{r.page} para{r.paragraph} line{r.line} "
            f"-> {'OK' if r.resolved else 'MISS'} word={r.candidate_word!r} "
            f"strategy={r.strategy} delta_upper={r.delta_upper} text={r.line_text!r}"
        )


if __name__ == "__main__":
    main()

