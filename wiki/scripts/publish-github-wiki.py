#!/usr/bin/env python3
"""Push wiki/gh-wiki/ to the GitHub Wiki git repository."""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WIKI = Path(__file__).resolve().parents[1]
SOURCE = WIKI / "gh-wiki"
EXPORT = WIKI / "scripts" / "export-github-wiki.py"
DEFAULT_REMOTE = "https://github.com/Vairn/MM2.wiki.git"


def run(
    cmd: list[str],
    *,
    cwd: Path,
    check: bool = True,
    capture: bool = False,
) -> subprocess.CompletedProcess:
    print("+", " ".join(cmd))
    return subprocess.run(
        cmd,
        cwd=cwd,
        check=check,
        capture_output=capture,
        text=capture,
    )


def git_branch(cwd: Path) -> str:
    r = run(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=cwd, capture=True)
    return (r.stdout or "").strip() or "master"


def publish(source: Path, remote: str, *, dry_run: bool, message: str, no_export: bool) -> int:
    if not no_export:
        run([sys.executable, str(EXPORT)], cwd=ROOT)

    if not source.is_dir() or not (source / "Home.md").exists():
        print(f"Missing {source}/Home.md — run export-github-wiki.py first", file=sys.stderr)
        return 1

    md_count = len(list(source.glob("*.md")))
    gallery = source / "images" / "gallery"
    if md_count < 10:
        print(
            f"Refusing to publish: only {md_count} markdown pages in {source} "
            "(expected doc + gallery pages). Run export-github-wiki.py without --skip-gallery.",
            file=sys.stderr,
        )
        return 1
    if gallery.is_dir() and not any(gallery.rglob("*.png")):
        print(
            f"Refusing to publish: {gallery} has no PNG exports. "
            "Run export-github-wiki.py with game assets present.",
            file=sys.stderr,
        )
        return 1

    with tempfile.TemporaryDirectory(prefix="mm2-wiki-") as tmp:
        clone_dir = Path(tmp) / "wiki"
        cloned = False
        try:
            run(["git", "clone", remote, str(clone_dir)], cwd=Path(tmp))
            cloned = True
        except subprocess.CalledProcessError:
            print("Wiki repo empty or missing — initializing fresh clone")
            clone_dir.mkdir()
            run(["git", "init"], cwd=clone_dir)
            run(["git", "remote", "add", "origin", remote], cwd=clone_dir)
            run(["git", "checkout", "-b", "master"], cwd=clone_dir, check=False)

        # Replace wiki contents (keep .git)
        for item in clone_dir.iterdir():
            if item.name == ".git":
                continue
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()

        for item in source.iterdir():
            dest = clone_dir / item.name
            if item.is_dir():
                shutil.copytree(item, dest)
            else:
                shutil.copy2(item, dest)

        run(["git", "add", "-A"], cwd=clone_dir)
        status = run(["git", "status", "--porcelain"], cwd=clone_dir, capture=True)
        if not (status.stdout or "").strip():
            print("Nothing to commit — wiki already up to date.")
            return 0

        run(["git", "commit", "-m", message], cwd=clone_dir)

        if dry_run:
            print("Dry run — skipping push.")
            return 0

        branch = git_branch(clone_dir)
        try:
            run(["git", "push", "origin", f"HEAD:{branch}"], cwd=clone_dir)
        except subprocess.CalledProcessError:
            try:
                run(["git", "push", "-u", "origin", branch], cwd=clone_dir)
            except subprocess.CalledProcessError as exc:
                print(f"Push failed ({exc}).")
                print(
                    "If you see 'Repository not found', enable the wiki on GitHub:\n"
                    "  Settings -> General -> Features -> Wikis\n"
                    "Then create the first wiki page on https://github.com/Vairn/MM2/wiki\n"
                    "and re-run: python wiki/scripts/publish-github-wiki.py --no-export"
                )
                return 1

        print(f"Published to {remote} ({branch})")
        print("https://github.com/Vairn/MM2/wiki")
        return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Publish wiki/gh-wiki/ to GitHub Wiki")
    ap.add_argument("--source", type=Path, default=SOURCE)
    ap.add_argument("--remote", default=DEFAULT_REMOTE)
    ap.add_argument("--message", default="Sync wiki from MM2 repository")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--no-export", action="store_true", help="Skip re-export; push existing gh-wiki/")
    args = ap.parse_args()
    return publish(
        args.source,
        args.remote,
        dry_run=args.dry_run,
        message=args.message,
        no_export=args.no_export,
    )


if __name__ == "__main__":
    raise SystemExit(main())
