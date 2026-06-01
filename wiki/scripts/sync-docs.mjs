#!/usr/bin/env node
/**
 * Copies reverse-engineering markdown from the repo into wiki/docs/ before
 * VitePress dev/build. Source of truth stays in EXTRACTED/docs/, tools/, etc.
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const wikiRoot = path.resolve(__dirname, '..');
const repoRoot = path.resolve(wikiRoot, '..');

const copies = [
  {
    from: path.join(repoRoot, 'EXTRACTED', 'docs'),
    to: path.join(wikiRoot, 'docs', 'reverse-engineering'),
    glob: '.md',
  },
  {
    from: path.join(repoRoot, 'EXTRACTED', 'mm2-ANALYSIS.md'),
    to: path.join(wikiRoot, 'docs', 'reverse-engineering', 'mm2-ANALYSIS.md'),
    single: true,
  },
  {
    from: path.join(repoRoot, 'tools', 'RE-TOOLS.md'),
    to: path.join(wikiRoot, 'docs', 'tools', 're-tools.md'),
    single: true,
  },
  {
    from: path.join(repoRoot, 'editor', 'README.md'),
    to: path.join(wikiRoot, 'docs', 'editor', 'mm2ed.md'),
    single: true,
  },
  {
    from: path.join(repoRoot, 'EXTRACTED', 'decomp', 'README.md'),
    to: path.join(wikiRoot, 'docs', 'decomp', 'readme.md'),
    single: true,
  },
  {
    from: path.join(repoRoot, 'CLAUDE.md'),
    to: path.join(wikiRoot, 'docs', 'reference', 'workspace-notes.md'),
    single: true,
  },
];

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function copyFile(src, dest) {
  ensureDir(path.dirname(dest));
  let text = fs.readFileSync(src, 'utf8');
  if (!text.startsWith('---\n')) {
    const title = path.basename(dest, '.md').replace(/^\d+-/, '').replace(/-/g, ' ');
    text = `---\ntitle: ${title}\n---\n\n${text}`;
  }
  fs.writeFileSync(dest, text, 'utf8');
}

function copyDir(srcDir, destDir, ext) {
  ensureDir(destDir);
  for (const name of fs.readdirSync(srcDir)) {
    if (!name.endsWith(ext)) continue;
    copyFile(path.join(srcDir, name), path.join(destDir, name));
  }
}

// Wipe generated docs each sync so stale pages disappear.
const docsRoot = path.join(wikiRoot, 'docs');
if (fs.existsSync(docsRoot)) {
  fs.rmSync(docsRoot, { recursive: true, force: true });
}

for (const entry of copies) {
  if (entry.single) {
    if (!fs.existsSync(entry.from)) {
      console.warn(`skip (missing): ${entry.from}`);
      continue;
    }
    copyFile(entry.from, entry.to);
    continue;
  }
  if (!fs.existsSync(entry.from)) {
    console.warn(`skip (missing): ${entry.from}`);
    continue;
  }
  copyDir(entry.from, entry.to, entry.glob);
}

// MM2 monospace bitmap font (same file as MM2ED).
const fontSrc = path.join(repoRoot, 'editor', 'assets', 'fonts', 'mm2_bitmap_monospace.ttf');
const fontDest = path.join(wikiRoot, 'public', 'fonts', 'mm2_bitmap_monospace.ttf');
if (fs.existsSync(fontSrc)) {
  ensureDir(path.dirname(fontDest));
  fs.copyFileSync(fontSrc, fontDest);
} else {
  console.warn(`skip (missing): ${fontSrc}`);
}

console.log('Synced docs -> wiki/docs/');
