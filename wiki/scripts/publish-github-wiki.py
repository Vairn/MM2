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

# Pages/assets produced by export-github-wiki.py when game .anm/.32 assets are present.
# Docs-only (--skip-gallery) exports omit these; publishing must not wipe them from the live wiki.
AMIGA_GALLERY_PAGES = (
    "Monster-Sprites.md",
    "World-Sprites.md",
    "Monsters.md",
    "Items-Catalog.md",
    "Tilesets.md",
    "Map-Cartography.md",
    "Map-dat-Grids.md",
    "3D-View-Graphics.md",
    "Monster-Variants.md",
    "Platform-Graphics-Index.md",
    "Platform-Monsters-01-74.md",
)
AMIGA_GALLERY_IMAGE_DIRS = (
    "monsters",
    "world",
    "tilesets",
    "maps",
    "mapdat",
    "views",
    "carto",
    "monster-variants",
    "platform-compare",
)


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


def has_amiga_sprite_gallery(root: Path) -> bool:
    """True when Monster-Sprites + decoded monster PNGs are present."""
    monsters = root / "images" / "gallery" / "monsters"
    return (root / "Monster-Sprites.md").is_file() and monsters.is_dir() and any(
        monsters.glob("*.png")
    )


def is_gallery_stub(gallery_md: Path) -> bool:
    if not gallery_md.is_file():
        return True
    text = gallery_md.read_text(encoding="utf-8", errors="replace")
    return (
        "Gallery images are generated from original game assets" in text
        or "This export used **`--skip-gallery`**" in text
        or "This export used `--skip-gallery`" in text
    )


def backup_amiga_gallery(clone_dir: Path, backup_dir: Path) -> bool:
    """Copy Amiga gallery pages/images from an existing wiki clone. Returns True if anything saved."""
    saved = False
    backup_dir.mkdir(parents=True, exist_ok=True)

    for name in AMIGA_GALLERY_PAGES:
        src = clone_dir / name
        if src.is_file():
            shutil.copy2(src, backup_dir / name)
            saved = True

    gallery_md = clone_dir / "Gallery.md"
    if gallery_md.is_file() and not is_gallery_stub(gallery_md):
        shutil.copy2(gallery_md, backup_dir / "Gallery.md")
        saved = True

    img_root = clone_dir / "images" / "gallery"
    if img_root.is_dir():
        for sub in AMIGA_GALLERY_IMAGE_DIRS:
            src_dir = img_root / sub
            if not src_dir.is_dir():
                continue
            dest = backup_dir / "images" / "gallery" / sub
            shutil.copytree(src_dir, dest)
            saved = True
    return saved


def restore_amiga_gallery(backup_dir: Path, clone_dir: Path, *, replace_stub_gallery: bool) -> None:
    """Merge preserved Amiga gallery into the to-be-pushed tree (source wins when present)."""
    for name in AMIGA_GALLERY_PAGES:
        src = backup_dir / name
        dest = clone_dir / name
        if src.is_file() and not dest.exists():
            shutil.copy2(src, dest)
            print(f"  preserved {name}")

    stub = clone_dir / "Gallery.md"
    backed = backup_dir / "Gallery.md"
    if backed.is_file() and (replace_stub_gallery or not stub.exists()):
        shutil.copy2(backed, stub)
        print("  preserved Gallery.md (non-stub)")

    img_backup = backup_dir / "images" / "gallery"
    if not img_backup.is_dir():
        return
    for sub in AMIGA_GALLERY_IMAGE_DIRS:
        src_dir = img_backup / sub
        dest_dir = clone_dir / "images" / "gallery" / sub
        if not src_dir.is_dir():
            continue
        if dest_dir.exists() and any(dest_dir.rglob("*")):
            continue
        dest_dir.parent.mkdir(parents=True, exist_ok=True)
        if dest_dir.exists():
            shutil.rmtree(dest_dir)
        shutil.copytree(src_dir, dest_dir)
        print(f"  preserved images/gallery/{sub}/")


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

    source_has_gallery = has_amiga_sprite_gallery(source)
    if not source_has_gallery:
        print(
            "Source export is docs-only (no Monster-Sprites / monsters PNGs). "
            "Will preserve Amiga gallery pages/images from the live wiki if present."
        )

    with tempfile.TemporaryDirectory(prefix="mm2-wiki-") as tmp:
        clone_dir = Path(tmp) / "wiki"
        backup_dir = Path(tmp) / "gallery-backup"
        try:
            run(["git", "clone", remote, str(clone_dir)], cwd=Path(tmp))
        except subprocess.CalledProcessError:
            print("Wiki repo empty or missing — initializing fresh clone")
            clone_dir.mkdir()
            run(["git", "init"], cwd=clone_dir)
            run(["git", "remote", "add", "origin", remote], cwd=clone_dir)
            run(["git", "checkout", "-b", "master"], cwd=clone_dir, check=False)

        preserved = False
        if not source_has_gallery and has_amiga_sprite_gallery(clone_dir):
            preserved = backup_amiga_gallery(clone_dir, backup_dir)
            if preserved:
                print("Backed up Amiga sprite gallery from live wiki before replace.")
        elif not source_has_gallery:
            print(
                "WARNING: docs-only export and live wiki also lacks Monster-Sprites. "
                "Publish will leave gallery links dead until a full export is run:\n"
                "  python wiki/scripts/export-github-wiki.py\n"
                "  python wiki/scripts/publish-github-wiki.py --no-export",
                file=sys.stderr,
            )

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

        if preserved:
            restore_amiga_gallery(
                backup_dir,
                clone_dir,
                replace_stub_gallery=is_gallery_stub(clone_dir / "Gallery.md"),
            )

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
