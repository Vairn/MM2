#!/usr/bin/env python3
"""Interactive GUI to tune throw.32 compositing vs WinUAE refs.

  python tools/tune_throw_gui.py
  python tools/tune_throw_gui.py --ref-dir "path/to/174.png..205.png"

Controls:
  Left/Right     nudge hand X (Shift = ±8)
  Up/Down        nudge die X when roll phase enabled
  [ / ]          hand frame −/+
  , / .          die frame −/+ (roll only)
  PgUp/PgDn      previous / next ref
  R              auto-tune current ref (~0.5s)
  S              save session JSON
  Ctrl+S         apply all refs -> preview_throw_anim.py
"""
from __future__ import annotations

import json
import sys
import tkinter as tk
from dataclasses import asdict
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import numpy as np
from PIL import Image, ImageDraw, ImageTk

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from compare_throw_refs import (  # noqa: E402
    content_mask,
    crop_game_viewport,
    mae as mae_masked,
)
from preview_throw_anim import (  # noqa: E402
    DEFAULT_REF_DIR,
    REF_BLIT,
    W,
    BlitSpec,
    crop_tableau,
    load_throw,
    spec_label,
)
from tune_throw_bruteforce import (  # noqa: E402
    TuneResult,
    apply_to_preview,
    compose_raw,
    tune_one,
)

SESSION_PATH = ROOT / "game" / "build" / "throw_preview" / "gui_session.json"
PREVIEW_PATH = ROOT / "tools" / "preview_throw_anim.py"
SCALE = 2
REF_LIST = sorted(REF_BLIT.keys())


def spec_to_dict(s: BlitSpec) -> dict:
    d = {"hand_fi": s.hand_fi, "hand_x": s.hand_x}
    if s.die_fi is not None:
        d["die_fi"] = s.die_fi
        d["die_x"] = s.die_x
    return d


def dict_to_spec(d: dict) -> BlitSpec:
    return BlitSpec(
        int(d["hand_fi"]),
        int(d["hand_x"]),
        d.get("die_fi"),
        d.get("die_x"),
    )


def make_diff(prev: Image.Image, ref: Image.Image) -> Image.Image:
    pa = np.array(prev.convert("RGB"), dtype=np.int16)
    pr = np.array(ref.convert("RGB"), dtype=np.int16)
    diff = np.clip(np.abs(pa - pr) * 3, 0, 255).astype(np.uint8)
    mask = content_mask(pr)
    diff[~mask] = 0
    out = Image.fromarray(diff)
    ImageDraw.Draw(out).text((2, 2), "diff x3", fill=(255, 80, 255))
    return out


def blend_overlay(prev: Image.Image, ref: Image.Image, alpha: float = 0.5) -> Image.Image:
    a = prev.convert("RGBA")
    b = ref.convert("RGBA").resize(a.size)
    return Image.blend(b, a, alpha)


class ThrowTuneApp:
    def __init__(self, frames: list[Image.Image], ref_dir: Path) -> None:
        self.frames = frames
        self.ref_dir = ref_dir
        self.specs: dict[int, dict] = {r: spec_to_dict(REF_BLIT[r]) for r in REF_LIST}
        self._load_session()

        self.root = tk.Tk()
        self.root.title("throw.32 tuner")
        self.root.geometry("1380x520")
        self._build_ui()
        self._bind_keys()
        self._refresh_after_id: str | None = None
        self._photo: ImageTk.PhotoImage | None = None
        self._on_ref_change()
        self.root.mainloop()

    def _load_session(self) -> None:
        if not SESSION_PATH.is_file():
            return
        try:
            data = json.loads(SESSION_PATH.read_text(encoding="utf-8"))
            for k, v in data.get("specs", {}).items():
                self.specs[int(k)] = v
            if data.get("ref_dir") and Path(data["ref_dir"]).is_dir():
                self.ref_dir = Path(data["ref_dir"])
        except (json.JSONDecodeError, KeyError, ValueError):
            pass

    def _save_session(self) -> None:
        SESSION_PATH.parent.mkdir(parents=True, exist_ok=True)
        SESSION_PATH.write_text(
            json.dumps(
                {"ref_dir": str(self.ref_dir), "specs": self.specs},
                indent=2,
            ),
            encoding="utf-8",
        )

    def _build_ui(self) -> None:
        top = ttk.Frame(self.root, padding=4)
        top.pack(fill=tk.X)

        ttk.Label(top, text="Ref:").pack(side=tk.LEFT)
        self.ref_var = tk.StringVar(value=str(REF_LIST[0]))
        self.ref_combo = ttk.Combobox(
            top, textvariable=self.ref_var, values=[str(r) for r in REF_LIST], width=6
        )
        self.ref_combo.pack(side=tk.LEFT, padx=4)
        self.ref_combo.bind("<<ComboboxSelected>>", lambda _: self._on_ref_change())

        ttk.Button(top, text="Prev", command=self._prev_ref).pack(side=tk.LEFT, padx=2)
        ttk.Button(top, text="Next", command=self._next_ref).pack(side=tk.LEFT, padx=2)
        ttk.Button(top, text="Ref dir...", command=self._pick_ref_dir).pack(side=tk.LEFT, padx=8)
        ttk.Button(top, text="Auto-tune (R)", command=self._auto_tune).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Save JSON (S)", command=self._save_session).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Apply -> preview (Ctrl+S)", command=self._apply_preview).pack(
            side=tk.LEFT, padx=4
        )
        ttk.Button(top, text="Regen validation", command=self._regen_validation).pack(
            side=tk.LEFT, padx=4
        )

        self.mae_var = tk.StringVar(value="MAE: —")
        ttk.Label(top, textvariable=self.mae_var, font=("Segoe UI", 10, "bold")).pack(
            side=tk.RIGHT, padx=8
        )
        self.spec_var = tk.StringVar(value="")
        ttk.Label(top, textvariable=self.spec_var, font=("Consolas", 9)).pack(
            side=tk.RIGHT, padx=4
        )

        ctrl = ttk.LabelFrame(self.root, text="Compositing", padding=6)
        ctrl.pack(fill=tk.X, padx=4, pady=2)

        self.orange_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            ctrl, text="Orange bar under sprites", variable=self.orange_var, command=self._schedule_refresh
        ).grid(row=0, column=0, columnspan=2, sticky=tk.W)

        self.roll_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            ctrl, text="Roll phase (hand + die)", variable=self.roll_var, command=self._on_roll_toggle
        ).grid(row=0, column=2, columnspan=2, sticky=tk.W, padx=12)

        self.overlay_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            ctrl, text="50% overlay on ref", variable=self.overlay_var, command=self._schedule_refresh
        ).grid(row=0, column=4, sticky=tk.W)

        self.diff_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            ctrl, text="Show diff panel", variable=self.diff_var, command=self._schedule_refresh
        ).grid(row=0, column=5, sticky=tk.W, padx=8)

        def add_slider(row: int, label: str, from_: int, to: int, var: tk.IntVar) -> ttk.Scale:
            ttk.Label(ctrl, text=label, width=12).grid(row=row, column=0, sticky=tk.W)
            sc = ttk.Scale(
                ctrl,
                from_=from_,
                to=to,
                orient=tk.HORIZONTAL,
                variable=var,
                command=lambda _: self._schedule_refresh(),
            )
            sc.grid(row=row, column=1, columnspan=3, sticky=tk.EW, padx=4)
            sp = ttk.Spinbox(ctrl, from_=from_, to=to, width=5, textvariable=var)
            sp.grid(row=row, column=4)
            var.trace_add("write", lambda *_: self._schedule_refresh())
            sp.bind("<Return>", lambda _: self._schedule_refresh())
            sp.bind("<FocusOut>", lambda _: self._schedule_refresh())
            return sc

        self.hand_fi = tk.IntVar(value=0)
        self.hand_x = tk.IntVar(value=0)
        self.die_fi = tk.IntVar(value=3)
        self.die_x = tk.IntVar(value=140)

        add_slider(1, "Hand frame", 0, 10, self.hand_fi)
        add_slider(2, "Hand X", 0, 280, self.hand_x)
        self._die_fi_scale = add_slider(3, "Die frame", 3, 10, self.die_fi)
        self._die_x_scale = add_slider(4, "Die X", 0, 280, self.die_x)
        ctrl.columnconfigure(1, weight=1)

        hint = ttk.Label(
            self.root,
            text="Keys: ←→ hand X | ↑↓ die X | [ ] hand frame | , . die | PgUp/Dn ref | R auto | S save | Ctrl+S apply",
            font=("Segoe UI", 8),
        )
        hint.pack(fill=tk.X, padx=6)

        self.canvas = tk.Label(self.root, bg="#202020")
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

    def _bind_keys(self) -> None:
        self.root.bind("<Left>", lambda e: self._nudge("hand_x", -1))
        self.root.bind("<Right>", lambda e: self._nudge("hand_x", 1))
        self.root.bind("<Shift-Left>", lambda e: self._nudge("hand_x", -8))
        self.root.bind("<Shift-Right>", lambda e: self._nudge("hand_x", 8))
        self.root.bind("<Up>", lambda e: self._nudge("die_x", -1))
        self.root.bind("<Down>", lambda e: self._nudge("die_x", 1))
        self.root.bind("<Shift-Up>", lambda e: self._nudge("die_x", -8))
        self.root.bind("<Shift-Down>", lambda e: self._nudge("die_x", 8))
        self.root.bind("[", lambda e: self._nudge("hand_fi", -1))
        self.root.bind("]", lambda e: self._nudge("hand_fi", 1))
        self.root.bind(",", lambda e: self._nudge("die_fi", -1))
        self.root.bind(".", lambda e: self._nudge("die_fi", 1))
        self.root.bind("<Prior>", lambda e: self._prev_ref())
        self.root.bind("<Next>", lambda e: self._next_ref())
        self.root.bind("r", lambda e: self._auto_tune())
        self.root.bind("R", lambda e: self._auto_tune())
        self.root.bind("s", lambda e: self._save_session())
        self.root.bind("S", lambda e: self._save_session())
        self.root.bind("<Control-s>", lambda e: self._apply_preview())

    def _current_ref(self) -> int:
        return int(self.ref_var.get())

    def _on_roll_toggle(self) -> None:
        roll = self.roll_var.get()
        state = "normal" if roll else "disabled"
        for w in (self._die_fi_scale, self._die_x_scale):
            w.configure(state=state)
        self._schedule_refresh()

    def _on_ref_change(self) -> None:
        ref = self._current_ref()
        d = self.specs[ref]
        self.hand_fi.set(d["hand_fi"])
        self.hand_x.set(d["hand_x"])
        has_die = "die_fi" in d and d["die_fi"] is not None
        self.roll_var.set(has_die or ref >= 183)
        if has_die:
            self.die_fi.set(d["die_fi"])
            self.die_x.set(d["die_x"])
        else:
            guess = {183: 3, 186: 4, 189: 5, 192: 6, 194: 7, 197: 8, 199: 9, 202: 10}.get(
                ref, 6
            )
            self.die_fi.set(guess)
            self.die_x.set(160)
        self._on_roll_toggle()
        self._schedule_refresh()

    def _store_current_spec(self) -> None:
        ref = self._current_ref()
        d = {"hand_fi": self.hand_fi.get(), "hand_x": self.hand_x.get()}
        if self.roll_var.get():
            d["die_fi"] = self.die_fi.get()
            d["die_x"] = self.die_x.get()
        self.specs[ref] = d

    def _nudge(self, field: str, delta: int) -> None:
        var = {
            "hand_fi": self.hand_fi,
            "hand_x": self.hand_x,
            "die_fi": self.die_fi,
            "die_x": self.die_x,
        }[field]
        lo, hi = (0, 10) if "fi" in field and field.startswith("hand") else (0, 280)
        if field == "die_fi":
            lo, hi = 3, 10
        if field.startswith("die") and not self.roll_var.get():
            return
        var.set(max(lo, min(hi, var.get() + delta)))
        self._schedule_refresh()

    def _prev_ref(self) -> None:
        i = REF_LIST.index(self._current_ref())
        self.ref_var.set(str(REF_LIST[(i - 1) % len(REF_LIST)]))
        self._on_ref_change()

    def _next_ref(self) -> None:
        i = REF_LIST.index(self._current_ref())
        self.ref_var.set(str(REF_LIST[(i + 1) % len(REF_LIST)]))
        self._on_ref_change()

    def _pick_ref_dir(self) -> None:
        p = filedialog.askdirectory(initialdir=str(self.ref_dir))
        if p:
            self.ref_dir = Path(p)
            self._schedule_refresh()

    def _schedule_refresh(self) -> None:
        self._store_current_spec()
        if self._refresh_after_id:
            self.root.after_cancel(self._refresh_after_id)
        self._refresh_after_id = self.root.after(30, self._refresh)

    def _compose(self) -> Image.Image:
        orange = self.orange_var.get()
        hf, hx = self.hand_fi.get(), self.hand_x.get()
        if self.roll_var.get():
            return compose_raw(
                self.frames, orange, hf, hx, self.die_fi.get(), self.die_x.get()
            )
        return compose_raw(self.frames, orange, hf, hx)

    def _load_ref_tab(self, ref: int) -> Image.Image | None:
        p = self.ref_dir / f"{ref}.png"
        if not p.is_file():
            return None
        return crop_tableau(crop_game_viewport(Image.open(p)))

    def _refresh(self) -> None:
        self._refresh_after_id = None
        ref = self._current_ref()
        prev_full = self._compose()
        prev = crop_tableau(prev_full)
        ref_im = self._load_ref_tab(ref)

        panels: list[tuple[str, Image.Image]] = [("preview", prev)]
        if ref_im is not None:
            panels.append(("ref", ref_im))
            if self.overlay_var.get():
                panels.append(("overlay", blend_overlay(prev, ref_im)))
            if self.diff_var.get():
                panels.append(("diff", make_diff(prev, ref_im)))
            mask = content_mask(np.array(ref_im.convert("RGB")))
            err = mae_masked(prev, ref_im, mask)
            self.mae_var.set(f"MAE: {err:.1f}")
        else:
            self.mae_var.set(f"MAE: (no {ref}.png)")

        sp = dict_to_spec(self.specs[ref])
        self.spec_var.set(f"{ref}: {spec_label(sp)}")

        row_h = max(p.size[1] for _, p in panels)
        row_w = sum(p.size[0] for _, p in panels)
        row = Image.new("RGB", (row_w, row_h + 14), (32, 32, 32))
        x = 0
        for title, im in panels:
            rgb = im.convert("RGB")
            row.paste(rgb, (x, 14))
            ImageDraw.Draw(row).text((x + 4, 0), title, fill=(200, 200, 200))
            x += rgb.width

        if SCALE != 1:
            row = row.resize((row.width * SCALE, row.height * SCALE), Image.Resampling.NEAREST)
        self._photo = ImageTk.PhotoImage(row)
        self.canvas.configure(image=self._photo)

    def _auto_tune(self) -> None:
        ref = self._current_ref()
        out = SESSION_PATH.parent
        try:
            r = tune_one(self.frames, ref, self.ref_dir, out, verbose=False)
        except Exception as exc:
            messagebox.showerror("Auto-tune", str(exc))
            return
        d = {
            "hand_fi": r.hand_fi,
            "hand_x": r.hand_x,
            "orange": r.orange,
        }
        if r.die_fi is not None:
            d["die_fi"] = r.die_fi
            d["die_x"] = r.die_x
        self.specs[ref] = d
        self.orange_var.set(r.orange)
        self._on_ref_change()
        messagebox.showinfo("Auto-tune", f"ref {ref}: MAE {r.mae:.1f}\n{spec_label(dict_to_spec(d))}")

    def _apply_preview(self, *, silent: bool = False) -> bool:
        self._store_current_spec()
        results = []
        for ref in REF_LIST:
            d = self.specs[ref]
            results.append(
                TuneResult(
                    ref=ref,
                    mae=0.0,
                    orange=d.get("orange", True),
                    hand_fi=d["hand_fi"],
                    hand_x=d["hand_x"],
                    die_fi=d.get("die_fi"),
                    die_x=d.get("die_x"),
                )
            )
        try:
            apply_to_preview(results, PREVIEW_PATH)
        except Exception as exc:
            messagebox.showerror("Apply", str(exc))
            return False
        self._save_session()
        if not silent:
            messagebox.showinfo("Apply", f"Updated {PREVIEW_PATH.name}")
        return True

    def _regen_validation(self) -> None:
        if not self._apply_preview(silent=True):
            return
        try:
            import subprocess

            subprocess.run(
                [sys.executable, str(ROOT / "tools" / "preview_throw_anim.py")],
                cwd=ROOT,
                check=True,
            )
            messagebox.showinfo(
                "Done",
                f"Wrote {ROOT / 'game' / 'build' / 'throw_preview' / 'refs_validation.png'}",
            )
        except Exception as exc:
            messagebox.showerror("Regen", str(exc))


def main() -> None:
    import argparse

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ref-dir", type=Path, default=DEFAULT_REF_DIR)
    ap.add_argument("--data-dir", type=Path, default=ROOT)
    args = ap.parse_args()

    frames = load_throw(args.data_dir)
    app_ref_dir = args.ref_dir
    if not app_ref_dir.is_dir():
        print(f"warning: ref dir missing: {app_ref_dir}", file=sys.stderr)
    ThrowTuneApp(frames, app_ref_dir)


if __name__ == "__main__":
    main()
