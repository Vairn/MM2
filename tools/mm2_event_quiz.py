#!/usr/bin/env python3
"""Interactive CLI for the MM2 event-opcode gameplay confirmation quiz.

Run from the repo root:

    cd c:\\_20260421_\\D-REC\\development\\MM2
    python tools/mm2_event_quiz.py

Answers are saved to EXTRACTED/event_quiz_answers.json (created on first run).
Questions live in tools/mm2_event_quiz_questions.json (sourced from
EXTRACTED/docs/07-event-script-opcodes.md).

During the quiz:
  A/B/C/D  — record your answer
  s        — skip (leave unanswered)
  q        — quit and save progress
  ?        — show in-game example for the current question

Flags:
  --reset       Delete saved answers and start fresh (Q1=A, Q2=A pre-filled)
  --summary     Print answer summary vs RE guesses (no interactive loop)
  --question N  Jump to question N (1–22) when resuming
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
QUESTIONS_PATH = Path(__file__).resolve().parent / "mm2_event_quiz_questions.json"
ANSWERS_PATH = ROOT / "EXTRACTED" / "event_quiz_answers.json"

KNOWN_PREFILL = {1: "A", 2: "A"}


def load_questions() -> list[dict]:
    with QUESTIONS_PATH.open(encoding="utf-8") as fh:
        data = json.load(fh)
    questions = data["questions"]
    if len(questions) != 22:
        raise SystemExit(f"Expected 22 questions, got {len(questions)}")
    return questions


def load_answers() -> dict:
    if not ANSWERS_PATH.is_file():
        return {}
    with ANSWERS_PATH.open(encoding="utf-8") as fh:
        payload = json.load(fh)
    return payload.get("answers", {})


def save_answers(answers: dict) -> None:
    ANSWERS_PATH.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "version": 1,
        "updated": datetime.now(timezone.utc).isoformat(),
        "answers": answers,
    }
    with ANSWERS_PATH.open("w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=2, ensure_ascii=False)
        fh.write("\n")


def reset_answers() -> dict:
    answers = {str(q): letter for q, letter in KNOWN_PREFILL.items()}
    save_answers(answers)
    return answers


def option_letters(question: dict) -> list[str]:
    return sorted(question["options"].keys())


def format_question(question: dict) -> str:
    lines = [
        "",
        f"=== Question {question['id']}/22: {question['title']} ===",
        "",
        question["scenario"],
        "",
    ]
    for letter in option_letters(question):
        lines.append(f"  {letter}. {question['options'][letter]}")
    lines.append("")
    re_guess = question["re_guess"]
    detail = question.get("re_guess_detail", "")
    lines.append(f"RE guess: {re_guess}" + (f" - {detail}" if detail else ""))
    return "\n".join(lines)


def print_summary(questions: list[dict], answers: dict) -> int:
    """Print summary; return exit code (0 = all answered)."""
    total = len(questions)
    answered = 0
    matches = 0
    mismatches: list[tuple[int, str, str, str]] = []

    print("\nMM2 Event Opcode Quiz - Summary")
    print("=" * 40)
    print(f"Answers file: {ANSWERS_PATH}")
    print()

    for q in questions:
        qid = str(q["id"])
        user = answers.get(qid)
        re_guess = q["re_guess"]
        if user is None:
            status = "- (unanswered)"
        else:
            answered += 1
            if user.upper() == re_guess.upper():
                status = f"{user} [match]"
                matches += 1
            else:
                status = f"{user} [MISMATCH vs RE {re_guess}]"
                mismatches.append((q["id"], q["title"], user, re_guess))

        print(f"Q{q['id']:2d}  {status}")

    print()
    print(f"Answered: {answered}/{total}  |  Matches with RE guess: {matches}/{answered or 0}")

    if mismatches:
        print()
        print("Mismatches (high-value feedback for RE):")
        print("-" * 40)
        for qid, title, user, re_guess in mismatches:
            print(f"  Q{qid}: you={user}, RE={re_guess}")
            print(f"       {title}")

    if answered < total:
        print()
        print(f"{total - answered} question(s) still unanswered. Run without --summary to continue.")
        return 1

    if mismatches:
        print()
        print("Quiz complete. Please note any mismatches inline in")
        print("EXTRACTED/docs/07-event-script-opcodes.md - that feedback")
        print("is the highest-value input for the opcode gap table.")
    else:
        print()
        print("Quiz complete - all answers match RE guesses.")

    return 0 if answered == total else 1


def prompt_answer(question: dict, answers: dict) -> str | None:
    """Return 'quit', 'skip', 'show', or a letter answer."""
    letters = option_letters(question)
    valid = "/".join(letters)
    qid = str(question["id"])
    existing = answers.get(qid)
    if existing:
        print(f"(Previously answered: {existing})")

    while True:
        try:
            raw = input(f"Your answer [{valid}], s=skip, ?=example, q=quit: ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return "quit"

        if not raw:
            continue
        key = raw.upper()
        if raw.lower() == "q":
            return "quit"
        if raw.lower() == "s":
            return "skip"
        if raw == "?":
            return "show"
        if key in letters:
            return key
        print(f"  Invalid input - enter {valid}, s, ?, or q.")


def run_quiz(questions: list[dict], answers: dict, start_id: int = 1) -> None:
    pending = [q for q in questions if str(q["id"]) not in answers and q["id"] >= start_id]
    if not pending and start_id == 1:
        pending = [q for q in questions if str(q["id"]) not in answers]
    if start_id > 1:
        pending = [q for q in questions if q["id"] >= start_id and str(q["id"]) not in answers]

    if not pending:
        print("All questions already answered.")
        print_summary(questions, answers)
        return

    print(f"\nMM2 Event Opcode Quiz - {len(pending)} question(s) remaining")
    print(f"Progress saved to: {ANSWERS_PATH}\n")

    for question in pending:
        print(format_question(question))
        example = question.get("example")
        if example:
            print("(Press ? for in-game example)")

        while True:
            result = prompt_answer(question, answers)
            if result == "quit":
                save_answers(answers)
                print(f"\nProgress saved ({len(answers)}/22 answered).")
                return
            if result == "skip":
                print("Skipped.\n")
                break
            if result == "show":
                if example:
                    print(f"\nExample: {example}\n")
                else:
                    print("\n(No example available for this question.)\n")
                continue
            answers[str(question["id"])] = result
            save_answers(answers)
            print(f"Recorded: Q{question['id']} = {result}\n")
            break

    print_summary(questions, answers)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Interactive gameplay confirmation quiz for MM2 event opcodes.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--reset",
        action="store_true",
        help="Delete saved answers and pre-fill Q1=A, Q2=A",
    )
    parser.add_argument(
        "--summary",
        action="store_true",
        help="Print summary vs RE guesses and exit",
    )
    parser.add_argument(
        "--question",
        type=int,
        metavar="N",
        help="Start at question N (1–22)",
    )
    args = parser.parse_args(argv)

    if not QUESTIONS_PATH.is_file():
        print(f"Questions file not found: {QUESTIONS_PATH}", file=sys.stderr)
        return 1

    questions = load_questions()

    if args.reset:
        answers = reset_answers()
        print(f"Reset answers -> {ANSWERS_PATH}")
        print(f"Pre-filled: {', '.join(f'Q{k}={v}' for k, v in sorted(KNOWN_PREFILL.items()))}")
        if not args.summary:
            return 0

    answers = load_answers()
    if not ANSWERS_PATH.is_file() and not args.reset:
        answers = reset_answers()
        print(f"Created {ANSWERS_PATH} with pre-filled Q1=A, Q2=A")

    if args.summary:
        return print_summary(questions, answers)

    start_id = args.question if args.question else 1
    if start_id < 1 or start_id > 22:
        print("--question must be 1–22", file=sys.stderr)
        return 1

    run_quiz(questions, answers, start_id=start_id)
    return 0


if __name__ == "__main__":
    sys.exit(main())
