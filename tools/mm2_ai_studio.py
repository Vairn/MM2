#!/usr/bin/env python3
"""MM2 AI Studio — Tkinter front-end for the OpenRouter asset tools.

One window over the three CLI scripts, sharing their code rather than
reimplementing it:

    openrouter_client.py     transport, retries, key discovery
    gen_agui_item_icons.py   ITEM_ICON_PROMPT.md parsing, per-item generation
    check_agui_pixel_grid.py 12x12 grid + palette QA

Tabs
  Item Icons   browse all 255 ids parsed from ITEM_ICON_PROMPT.md, see which
               are generated / clean / failing, generate a selection in
               parallel, inspect any icon at 1x-16x with its QA report, and
               run ingest without leaving the app.
  Playground   free-form image prompts (optionally prefixed with the Agui
               master prompt), reference images, save-as.
  Chat         plain conversation with the same model; attach images.

Run:
    python tools/mm2_ai_studio.py

Requires Python's bundled tkinter (Windows/macOS installers include it;
on Debian/Ubuntu: apt install python3-tk) and Pillow.
"""
from __future__ import annotations

import argparse
import io
import os
import queue
import sys
import threading
import traceback
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

import tkinter as tk
from tkinter import filedialog, messagebox, ttk

sys.path.insert(0, str(Path(__file__).resolve().parent))

try:
    from PIL import Image, ImageTk
except ImportError:  # pragma: no cover
    print("pip install pillow", file=sys.stderr)
    raise

import gen_agui_item_icons as gen  # noqa: E402
import openrouter_client as orc  # noqa: E402
from check_agui_pixel_grid import (  # noqa: E402
    check_image, load_palette, quantize_to_logical,
)

ROOT = gen.ROOT
AGUI = gen.AGUI
APP_TITLE = "MM2 AI Studio"

# muted dark chrome so 12x12 art reads correctly against it
BG = "#1e1f22"
BG_PANEL = "#26282c"
FG = "#d8d8d0"
ACCENT = "#c0a020"
OK_COLOR = "#40c040"
BAD_COLOR = "#c04040"
WARN_COLOR = "#c0a020"


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------


def checkerboard(size: tuple[int, int], cell: int = 8) -> Image.Image:
    """Transparency backdrop so alpha reads as alpha, not black."""
    img = Image.new("RGBA", size, (58, 60, 64, 255))
    dark = Image.new("RGBA", (cell, cell), (44, 46, 50, 255))
    for y in range(0, size[1], cell):
        for x in range(0, size[0], cell):
            if (x // cell + y // cell) % 2:
                img.paste(dark, (x, y))
    return img


def render_icon(path: Path, zoom: int, box: int = 196) -> Image.Image:
    """Preview an icon. Exact 12x/96x/192x files are shown at their true logical
    pixels; a raw model canvas (e.g. 1024x1024) is just fitted into the box."""
    src = Image.open(path).convert("RGBA")
    if src.width == src.height and src.width >= 12 and src.width % 12 == 0:
        native = src.resize((12, 12), Image.NEAREST)
        shown = native.resize((12 * zoom, 12 * zoom), Image.NEAREST)
    else:
        scale = min(box / src.width, box / src.height)
        shown = src.resize((max(1, int(src.width * scale)),
                            max(1, int(src.height * scale))), Image.LANCZOS)
    shown = shown.crop((0, 0, min(shown.width, box), min(shown.height, box)))
    bg = checkerboard(shown.size)
    bg.alpha_composite(shown)
    return bg


@dataclass
class Job:
    kind: str          # "log" | "item" | "done" | "image" | "chat" | "error"
    payload: object = None


# ---------------------------------------------------------------------------
# app
# ---------------------------------------------------------------------------


class Studio(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("1220x860")
        self.minsize(980, 700)
        self.configure(bg=BG)

        self.events: queue.Queue[Job] = queue.Queue()
        self.cancel = threading.Event()
        self.busy = False
        self._preview_ref: ImageTk.PhotoImage | None = None
        self._play_ref: ImageTk.PhotoImage | None = None
        self._play_images: list[orc.GeneratedImage] = []
        self.chat_history: list[dict] = []

        try:
            self.spec = gen.load_spec()
        except SystemExit as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            raise
        try:
            self.palette = load_palette()
        except SystemExit:
            self.palette = None

        # settings
        self.var_key = tk.StringVar(value=self._discover_key())
        self.var_model = tk.StringVar(value=orc.DEFAULT_IMAGE_MODEL)
        self.var_out = tk.StringVar(value=str(gen.DEFAULT_OUT))
        self.var_transport = tk.StringVar(value="auto")
        self.var_aspect = tk.StringVar(value="1:1")
        self.var_jobs = tk.IntVar(value=3)
        self.var_retries = tk.IntVar(value=1)
        self.var_strict = tk.BooleanVar(value=False)
        self.var_force = tk.BooleanVar(value=False)
        self.var_styleref = tk.BooleanVar(value=False)
        self.var_zoom = tk.IntVar(value=16)
        self.var_emit = tk.IntVar(value=96)
        self.var_content_cells = tk.IntVar(value=10)
        self.var_min_purity = tk.DoubleVar(value=0.55)
        self.var_show_raw = tk.BooleanVar(value=False)
        self.var_mode = tk.StringVar(value="per-item")
        self.var_sheet_cols = tk.StringVar(value="auto")
        self.var_status = tk.StringVar(value="ready")
        self.var_cost = tk.StringVar(value="$0.0000  ·  0 tok")

        self._style()
        self._build()
        self.refresh_tree()
        self.after(80, self._drain)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # -- chrome ------------------------------------------------------------

    def _style(self) -> None:
        s = ttk.Style(self)
        try:
            s.theme_use("clam")
        except tk.TclError:
            pass
        s.configure(".", background=BG, foreground=FG, fieldbackground=BG_PANEL,
                    bordercolor="#3a3d42", lightcolor=BG_PANEL, darkcolor=BG)
        s.configure("TNotebook", background=BG, borderwidth=0)
        s.configure("TNotebook.Tab", background=BG_PANEL, foreground=FG, padding=(16, 7))
        s.map("TNotebook.Tab", background=[("selected", "#33363b")],
              foreground=[("selected", ACCENT)])
        s.configure("TFrame", background=BG)
        s.configure("TLabelframe", background=BG, foreground=ACCENT)
        s.configure("TLabelframe.Label", background=BG, foreground=ACCENT)
        s.configure("TLabel", background=BG, foreground=FG)
        s.configure("TButton", background="#33363b", foreground=FG, padding=(10, 5))
        s.map("TButton", background=[("active", "#3f434a"), ("disabled", "#2a2c30")])
        s.configure("Accent.TButton", background=ACCENT, foreground="#1b1b18")
        s.map("Accent.TButton", background=[("active", "#d8b432"), ("disabled", "#6b5c19")])
        s.configure("TCheckbutton", background=BG, foreground=FG)
        s.configure("Treeview", background=BG_PANEL, fieldbackground=BG_PANEL,
                    foreground=FG, rowheight=21, borderwidth=0)
        s.configure("Treeview.Heading", background="#33363b", foreground=ACCENT)
        s.map("Treeview", background=[("selected", "#44484f")])
        s.configure("TEntry", insertcolor=FG)
        s.configure("Horizontal.TProgressbar", background=ACCENT, troughcolor=BG_PANEL)
        # clam renders readonly comboboxes with an unreadable field by default
        s.configure("TCombobox", arrowcolor=FG)
        s.map("TCombobox",
              fieldbackground=[("readonly", BG_PANEL), ("!disabled", BG_PANEL)],
              foreground=[("readonly", FG), ("!disabled", FG)],
              background=[("readonly", "#33363b")],
              selectbackground=[("readonly", BG_PANEL)],
              selectforeground=[("readonly", FG)])
        self.option_add("*TCombobox*Listbox.background", BG_PANEL)
        self.option_add("*TCombobox*Listbox.foreground", FG)
        self.option_add("*TCombobox*Listbox.selectBackground", ACCENT)
        self.option_add("*TCombobox*Listbox.selectForeground", "#1b1b18")

    def _build(self) -> None:
        self._build_topbar()
        self._build_statusbar()          # bottom-anchored: claim its row first
        nb = ttk.Notebook(self)
        nb.pack(side="top", fill="both", expand=True, padx=8, pady=(0, 4))
        self.tab_icons = ttk.Frame(nb)
        self.tab_play = ttk.Frame(nb)
        self.tab_chat = ttk.Frame(nb)
        nb.add(self.tab_icons, text="Item Icons")
        nb.add(self.tab_play, text="Playground")
        nb.add(self.tab_chat, text="Chat")
        self._build_icons(self.tab_icons)
        self._build_play(self.tab_play)
        self._build_chat(self.tab_chat)

    def _build_topbar(self) -> None:
        bar = ttk.Frame(self)
        bar.pack(fill="x", padx=8, pady=8)

        ttk.Button(bar, text="Check credits", command=self.check_credits).pack(side="right")
        ttk.Combobox(bar, textvariable=self.var_transport, values=["auto", "images", "chat"],
                     width=7, state="readonly").pack(side="right", padx=(6, 12))
        ttk.Label(bar, text="Transport").pack(side="right")

        ttk.Label(bar, text="API key").pack(side="left")
        self.entry_key = ttk.Entry(bar, textvariable=self.var_key, show="•", width=22)
        self.entry_key.pack(side="left", padx=(6, 2))
        ttk.Button(bar, text="show", width=5, command=self._toggle_key).pack(side="left")
        ttk.Button(bar, text="Save to .env", command=self._save_key).pack(side="left", padx=(4, 14))

        ttk.Label(bar, text="Model").pack(side="left")
        models = [
            orc.DEFAULT_IMAGE_MODEL,
            "google/gemini-2.5-flash-image",
            "openai/gpt-image-1",
            "black-forest-labs/flux-1.1-pro",
        ]
        ttk.Combobox(bar, textvariable=self.var_model, values=models,
                     width=32).pack(side="left", padx=(6, 12))

    def _build_statusbar(self) -> None:
        bar = ttk.Frame(self)
        bar.pack(side="bottom", fill="x", padx=8, pady=(0, 8))
        self.progress = ttk.Progressbar(bar, mode="determinate", length=220)
        self.progress.pack(side="left", padx=(0, 10))
        ttk.Label(bar, textvariable=self.var_status).pack(side="left")
        ttk.Label(bar, textvariable=self.var_cost, foreground=ACCENT).pack(side="right")

    # -- tab 1: item icons -------------------------------------------------

    def _build_icons(self, parent: ttk.Frame) -> None:
        # Pack the fixed-height panels against the bottom FIRST so the
        # expanding pane can only claim what is genuinely left over —
        # otherwise the pane's requested size squeezes them off-screen.
        logf = ttk.LabelFrame(parent, text="Log")
        logf.pack(side="bottom", fill="x", padx=6, pady=(0, 6))
        self.log_text = tk.Text(logf, height=5, bg="#161719", fg="#b9bcc0", wrap="none",
                                relief="flat", insertbackground=FG)
        self.log_text.pack(fill="both", expand=True, padx=8, pady=8)

        opts = ttk.LabelFrame(parent, text="Generation")
        opts.pack(side="bottom", fill="x", padx=6, pady=(0, 6))

        pane = ttk.PanedWindow(parent, orient="horizontal")
        pane.pack(side="top", fill="both", expand=True, padx=6, pady=6)

        left = ttk.Frame(pane)
        pane.add(left, weight=3)

        sel = ttk.Frame(left)
        sel.pack(fill="x", pady=(0, 6))
        ttk.Label(sel, text="Select:").pack(side="left", padx=(0, 6))
        for label, fn in (
            ("All", lambda: self.select_where(lambda s: True)),
            ("Missing", lambda: self.select_where(lambda s: s == "missing")),
            ("Failing", lambda: self.select_where(lambda s: s in ("qa-fail", "error"))),
            ("None", lambda: self.tree.selection_set(())),
        ):
            ttk.Button(sel, text=label, command=fn, width=8).pack(side="left", padx=2)
        ttk.Button(sel, text="Rescan", command=self.refresh_tree, width=8).pack(side="left", padx=(12, 2))

        cols = ("id", "name", "status", "note")
        self.tree = ttk.Treeview(left, columns=cols, show="tree headings", selectmode="extended")
        self.tree.heading("#0", text="Batch", anchor="w")
        self.tree.heading("id", text="Id", anchor="center")
        self.tree.heading("name", text="Item", anchor="w")
        self.tree.heading("status", text="State", anchor="center")
        self.tree.heading("note", text="QA", anchor="w")
        self.tree.column("#0", width=170, minwidth=110, stretch=False)
        self.tree.column("id", width=40, anchor="center", stretch=False)
        self.tree.column("name", width=170, minwidth=110, stretch=False)
        self.tree.column("status", width=70, anchor="center", stretch=False)
        self.tree.column("note", width=170, minwidth=80, anchor="w", stretch=True)
        self.tree.tag_configure("ok", foreground=OK_COLOR)
        self.tree.tag_configure("bad", foreground=BAD_COLOR)
        self.tree.tag_configure("warn", foreground=WARN_COLOR)
        self.tree.tag_configure("missing", foreground="#8a8d92")
        sb = ttk.Scrollbar(left, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=sb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        self.tree.bind("<<TreeviewSelect>>", self.on_tree_select)

        right = ttk.Frame(pane)
        pane.add(right, weight=2)

        qa = ttk.LabelFrame(right, text="QA report")
        qa.pack(side="bottom", fill="both", expand=True, pady=(8, 0))
        self.qa_text = tk.Text(qa, height=8, bg=BG_PANEL, fg=FG, wrap="word",
                               relief="flat", insertbackground=FG)
        self.qa_text.pack(fill="both", expand=True, padx=8, pady=8)

        prev = ttk.LabelFrame(right, text="Preview")
        prev.pack(side="top", fill="x")
        self.canvas = tk.Canvas(prev, width=336, height=192, bg=BG_PANEL,
                                highlightthickness=0)
        self.canvas.pack(padx=8, pady=8)
        zoomrow = ttk.Frame(prev)
        zoomrow.pack(fill="x", padx=8, pady=(0, 8))
        ttk.Label(zoomrow, text="Zoom").pack(side="left")
        for z in (1, 4, 8, 12, 16):
            ttk.Radiobutton(zoomrow, text=f"{z}x", value=z, variable=self.var_zoom,
                            command=self.on_tree_select).pack(side="left", padx=2)
        ttk.Checkbutton(zoomrow, text="raw", variable=self.var_show_raw,
                        command=self.on_tree_select).pack(side="right")

        row = ttk.Frame(opts)
        row.pack(fill="x", padx=8, pady=8)
        ttk.Label(row, text="Out dir").pack(side="left")
        ttk.Entry(row, textvariable=self.var_out, width=46).pack(side="left", padx=6)
        ttk.Button(row, text="…", width=3, command=self._pick_out).pack(side="left")
        ttk.Label(row, text="Mode").pack(side="left", padx=(14, 4))
        ttk.Combobox(row, textvariable=self.var_mode, values=["per-item", "sheet"],
                     width=9, state="readonly").pack(side="left")
        ttk.Label(row, text="Jobs").pack(side="left", padx=(14, 4))
        ttk.Spinbox(row, from_=1, to=8, textvariable=self.var_jobs, width=4).pack(side="left")
        ttk.Label(row, text="Retries").pack(side="left", padx=(10, 4))
        ttk.Spinbox(row, from_=0, to=5, textvariable=self.var_retries, width=4).pack(side="left")
        ttk.Label(row, text="Aspect").pack(side="left", padx=(10, 4))
        ttk.Entry(row, textvariable=self.var_aspect, width=6).pack(side="left")
        ttk.Label(row, text="Min purity").pack(side="left", padx=(10, 4))
        ttk.Spinbox(row, from_=0.0, to=1.0, increment=0.05, format="%.2f",
                    textvariable=self.var_min_purity, width=5).pack(side="left")
        ttk.Label(row, text="Cells").pack(side="left", padx=(10, 4))
        ttk.Spinbox(row, from_=6, to=12, textvariable=self.var_content_cells,
                    width=4).pack(side="left")

        act = ttk.Frame(opts)
        act.pack(fill="x", padx=8, pady=(0, 8))
        self.btn_gen = ttk.Button(act, text="Generate selected", style="Accent.TButton",
                                  command=self.generate_selected)
        self.btn_gen.pack(side="left")
        self.btn_stop = ttk.Button(act, text="Stop", command=self.stop, state="disabled")
        self.btn_stop.pack(side="left", padx=6)
        ttk.Button(act, text="Show prompt", command=self.show_prompt).pack(side="left", padx=6)
        ttk.Button(act, text="Re-check QA", command=self.recheck_all).pack(side="left", padx=6)
        ttk.Button(act, text="Ingest → items/", command=self.run_ingest).pack(side="left", padx=6)
        ttk.Button(act, text="Open folder", command=self.open_out).pack(side="left", padx=6)
        ttk.Checkbutton(act, text="Style refs", variable=self.var_styleref).pack(side="right", padx=(8, 0))
        ttk.Checkbutton(act, text="Overwrite", variable=self.var_force).pack(side="right", padx=(8, 0))
        ttk.Checkbutton(act, text="Strict QA", variable=self.var_strict,
                        command=self.refresh_tree).pack(side="right", padx=(8, 0))

    # -- tab 2: playground -------------------------------------------------

    def _build_play(self, parent: ttk.Frame) -> None:
        left = ttk.Frame(parent)
        left.pack(side="left", fill="both", expand=True, padx=(8, 4), pady=8)
        right = ttk.Frame(parent)
        right.pack(side="right", fill="both", padx=(4, 8), pady=8)

        ttk.Label(left, text="Prompt").pack(anchor="w")
        self.play_prompt = tk.Text(left, height=12, bg=BG_PANEL, fg=FG, wrap="word",
                                   relief="flat", insertbackground=FG)
        self.play_prompt.pack(fill="both", expand=True, pady=(4, 8))

        self.var_use_master = tk.BooleanVar(value=True)
        opts = ttk.Frame(left)
        opts.pack(fill="x")
        ttk.Checkbutton(opts, text="Prefix Agui master prompt + negative prompt",
                        variable=self.var_use_master).pack(side="left")
        ttk.Label(opts, text="n").pack(side="left", padx=(16, 4))
        self.var_play_n = tk.IntVar(value=1)
        ttk.Spinbox(opts, from_=1, to=4, textvariable=self.var_play_n, width=4).pack(side="left")

        refs = ttk.Frame(left)
        refs.pack(fill="x", pady=(8, 0))
        ttk.Label(refs, text="Reference images").pack(side="left")
        ttk.Button(refs, text="Add…", command=self._add_ref, width=7).pack(side="left", padx=6)
        ttk.Button(refs, text="Clear", command=self._clear_refs, width=7).pack(side="left")
        self.ref_list = tk.Listbox(left, height=3, bg=BG_PANEL, fg=FG, relief="flat",
                                   highlightthickness=0)
        self.ref_list.pack(fill="x", pady=(4, 8))

        act = ttk.Frame(left)
        act.pack(fill="x")
        self.btn_play = ttk.Button(act, text="Generate", style="Accent.TButton",
                                   command=self.play_generate)
        self.btn_play.pack(side="left")
        ttk.Button(act, text="Save as…", command=self.play_save).pack(side="left", padx=6)

        ttk.Label(right, text="Result").pack(anchor="w")
        self.play_canvas = tk.Canvas(right, width=420, height=420, bg=BG_PANEL,
                                     highlightthickness=0)
        self.play_canvas.pack(pady=(4, 6))
        self.play_info = ttk.Label(right, text="—")
        self.play_info.pack(anchor="w")

    # -- tab 3: chat -------------------------------------------------------

    def _build_chat(self, parent: ttk.Frame) -> None:
        self.chat_text = tk.Text(parent, bg=BG_PANEL, fg=FG, wrap="word", relief="flat",
                                 insertbackground=FG, state="disabled")
        self.chat_text.pack(fill="both", expand=True, padx=8, pady=(8, 4))
        self.chat_text.tag_configure("user", foreground=ACCENT)
        self.chat_text.tag_configure("err", foreground=BAD_COLOR)

        row = ttk.Frame(parent)
        row.pack(fill="x", padx=8, pady=(0, 8))
        self.chat_entry = tk.Text(row, height=3, bg=BG_PANEL, fg=FG, wrap="word",
                                  relief="flat", insertbackground=FG)
        self.chat_entry.pack(side="left", fill="both", expand=True)
        self.chat_entry.bind("<Control-Return>", lambda e: (self.chat_send(), "break")[1])
        side = ttk.Frame(row)
        side.pack(side="right", padx=(8, 0))
        ttk.Button(side, text="Send  (Ctrl+↵)", style="Accent.TButton",
                   command=self.chat_send).pack(fill="x")
        ttk.Button(side, text="Attach image…", command=self._chat_attach).pack(fill="x", pady=4)
        ttk.Button(side, text="Reset", command=self.chat_reset).pack(fill="x")
        self.chat_attachments: list[str] = []

    # -- state -------------------------------------------------------------

    def _discover_key(self) -> str:
        try:
            return orc.resolve_api_key()
        except orc.OpenRouterError:
            return ""

    def out_dir(self) -> Path:
        return Path(self.var_out.get()).expanduser()

    def client(self) -> orc.OpenRouterClient:
        key = self.var_key.get().strip()
        if not key:
            raise orc.OpenRouterError("no API key — paste one in the top bar")
        return orc.OpenRouterClient(api_key=key, model=self.var_model.get().strip(),
                                    timeout=300.0)

    def refs(self) -> list[str]:
        out = list(self.ref_list.get(0, "end")) if hasattr(self, "ref_list") else []
        if self.var_styleref.get():
            out += [str(p) for p in gen.STYLE_REFS if p.is_file()]
        return out

    def _gen_args(self) -> argparse.Namespace:
        return argparse.Namespace(
            force=self.var_force.get(),
            dry_run=False,
            retries=int(self.var_retries.get()),
            aspect=self.var_aspect.get().strip() or None,
            transport=self.var_transport.get(),
            strict_qa=self.var_strict.get(),
            emit=int(self.var_emit.get()),
            content_cells=int(self.var_content_cells.get()),
            min_purity=float(self.var_min_purity.get()),
            sheet_cols=self.var_sheet_cols.get(),
            canvas=gen.SHEET_CANVAS,
            split_sheet=None,
        )

    # -- tree --------------------------------------------------------------

    def refresh_tree(self) -> None:
        self.tree.delete(*self.tree.get_children())
        out = self.out_dir()
        self.item_by_iid: dict[str, gen.Item] = {}
        self.status_by_iid: dict[str, str] = {}
        counts = {"ok": 0, "missing": 0, "qa-fail": 0}
        for b in self.spec.batches:
            parent = self.tree.insert(
                "", "end", text=f"{b.number:02d}  {b.title}",
                values=("", f"0x{b.first:02X}–0x{b.last:02X}", "", ""), open=(b.number == 1))
            for it in b.items:
                path = out / it.filename
                if not path.is_file():
                    state, note, tag = "missing", "", "missing"
                else:
                    ok, note = self._qa(path)
                    state = "ok" if ok else "qa-fail"
                    tag = "ok" if ok else "bad"
                counts[state] = counts.get(state, 0) + 1
                iid = self.tree.insert(parent, "end", text="",
                                       values=(it.hex, it.name, state, note[:90]), tags=(tag,))
                self.item_by_iid[iid] = it
                self.status_by_iid[iid] = state
        self.var_status.set(
            f"{counts['ok']} clean · {counts.get('qa-fail', 0)} need work · "
            f"{counts['missing']} missing   ({self.out_dir()})")

    def _qa(self, path: Path) -> tuple[bool, str]:
        if self.palette is None:
            return True, "palette.json missing — QA skipped"
        try:
            rep = check_image(path, self.palette,
                              max_soft=0 if self.var_strict.get() else 8,
                              max_off=0 if self.var_strict.get() else 12,
                              min_purity=float(self.var_min_purity.get()),
                              require_margin=False)
        except Exception as exc:  # noqa: BLE001
            return True, f"qa error: {exc}"
        return rep.ok, "; ".join(rep.errors) or "; ".join(rep.warnings) or "clean"

    def raw_path(self, item) -> Path:
        return self.out_dir() / "raw" / item.filename

    def select_where(self, pred) -> None:
        picks = [iid for iid, st in self.status_by_iid.items() if pred(st)]
        self.tree.selection_set(picks)
        if picks:
            self.tree.see(picks[0])
        self.log(f"selected {len(picks)} item(s)")

    def selected_items(self) -> list[gen.Item]:
        out: list[gen.Item] = []
        for iid in self.tree.selection():
            if iid in self.item_by_iid:
                out.append(self.item_by_iid[iid])
            else:  # a batch row: take its children
                for child in self.tree.get_children(iid):
                    if child in self.item_by_iid:
                        out.append(self.item_by_iid[child])
        seen, uniq = set(), []
        for it in out:
            if it.id not in seen:
                seen.add(it.id)
                uniq.append(it)
        return uniq

    def on_tree_select(self, _event=None) -> None:
        items = self.selected_items()
        self.qa_text.delete("1.0", "end")
        self.canvas.delete("all")
        if len(items) != 1:
            if items:
                self.qa_text.insert("1.0", f"{len(items)} items selected.")
            return
        it = items[0]
        clean = self.out_dir() / it.filename
        raw = self.raw_path(it)
        path = raw if (self.var_show_raw.get() and raw.is_file()) else clean
        self.qa_text.insert("1.0", f"{it.hex}  {it.name}\n{path}\n\n")
        if not path.is_file():
            self.qa_text.insert("end",
                                "raw model output not kept for this icon"
                                if self.var_show_raw.get() else "not generated yet")
            return
        if raw.is_file() and self.palette is not None:
            try:
                _, st = quantize_to_logical(
                    Image.open(raw), self.palette,
                    content_cells=int(self.var_content_cells.get()))
                self.qa_text.insert("end", "\n".join([
                    f"raw source     {st.source_size[0]}x{st.source_size[1]}"
                    + ("  (bg keyed)" if st.bg_stripped else "")
                    + ("  (cropped)" if st.cropped else ""),
                    f"purity         {st.purity:.2f}   "
                    f"(1.00 = flat blocks, < {float(self.var_min_purity.get()):.2f} rejected)",
                    f"palette drift  {st.mean_pen_dist:.0f}",
                    "", "-- quantized icon --", "",
                ]))
            except Exception as exc:  # noqa: BLE001
                self.qa_text.insert("end", f"quantize preview failed: {exc}\n\n")
        if self.palette is not None:
            try:
                rep = check_image(path, self.palette, max_soft=0, max_off=0,
                                  require_margin=False)
                self.qa_text.insert("end", "\n".join([
                    f"size            {rep.size[0]}x{rep.size[1]}  (scale {rep.scale}x)",
                    f"soft blocks     {rep.soft_blocks}",
                    f"off-palette     {rep.off_palette_blocks}  (worst {rep.worst_palette_dist:.0f})",
                    f"distinct pens   {rep.distinct_pens}",
                    f"filled blocks   {rep.filled_blocks} / 144",
                    f"border used     {rep.margin_violations}",
                    "",
                    *(f"ERROR  {e}" for e in rep.errors),
                    *(f"warn   {w}" for w in rep.warnings),
                ]))
            except Exception as exc:  # noqa: BLE001
                self.qa_text.insert("end", f"qa failed: {exc}")
        try:
            img = render_icon(path, self.var_zoom.get())
        except Exception as exc:  # noqa: BLE001
            self.log(f"preview failed: {exc}")
            return
        self._preview_ref = ImageTk.PhotoImage(img)
        self.canvas.create_image(168, 96, image=self._preview_ref)

    # -- actions -----------------------------------------------------------

    def show_prompt(self) -> None:
        items = self.selected_items()
        if not items:
            messagebox.showinfo(APP_TITLE, "select an item first")
            return
        if self.var_mode.get() == "sheet":
            b = self.spec.batch(items[0].batch)
            cols, rows, cell = gen.sheet_layout(len(b.items), gen.SHEET_CANVAS,
                                                self.var_sheet_cols.get())
            text = gen.sheet_prompt(self.spec, b, cols, rows, cell)
            title = f"Sheet prompt — batch {b.number:02d} ({cols}x{cols}, {cell}px cells)"
        else:
            text = gen.item_prompt(self.spec, items[0])
            title = f"Prompt — {items[0]}"
        win = tk.Toplevel(self)
        win.title(title)
        win.configure(bg=BG)
        box = tk.Text(win, width=100, height=40, bg=BG_PANEL, fg=FG, wrap="word", relief="flat")
        box.pack(fill="both", expand=True, padx=8, pady=8)
        box.insert("1.0", text)
        ttk.Button(win, text="Copy", command=lambda: (
            self.clipboard_clear(), self.clipboard_append(text))).pack(pady=(0, 8))

    def generate_selected(self) -> None:
        if self.busy:
            return
        items = self.selected_items()
        if not items:
            messagebox.showinfo(APP_TITLE, "nothing selected")
            return
        try:
            client = self.client()
        except orc.OpenRouterError as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            return
        if not self.var_force.get():
            items = [it for it in items if not (self.out_dir() / it.filename).is_file()]
            if not items:
                messagebox.showinfo(APP_TITLE,
                                    "all selected icons already exist — tick Overwrite to redo them")
                return

        out = self.out_dir()
        out.mkdir(parents=True, exist_ok=True)
        args = self._gen_args()
        refs = self.refs()
        jobs = max(1, int(self.var_jobs.get()))   # read Tk vars on this thread only
        sheet_mode = self.var_mode.get() == "sheet"

        if sheet_mode:
            numbers = sorted({it.batch for it in items})
            batches = [self.spec.batch(n) for n in numbers]
            total = sum(len(b.items) for b in batches)
            self._set_busy(True, total)
            self.log(f"sheet mode: {len(batches)} batch(es), {total} icons, "
                     f"{len(batches)} request(s)")
            for b in batches:
                cols, rows, cell = gen.sheet_layout(len(b.items), gen.SHEET_CANVAS,
                                                    args.sheet_cols)
                self.log(f"  batch {b.number:02d}: {len(b.items)} items on a "
                         f"{cols}x{cols} grid, {cell}px cells "
                         f"({cell / 12:.1f}px per logical pixel)")

            def sheet_worker() -> None:
                gen.say = lambda *p: self.events.put(Job("log", " ".join(str(x) for x in p)))
                done = 0
                try:
                    for b in batches:
                        if self.cancel.is_set():
                            break
                        try:
                            for res in gen.generate_sheet(client, self.spec, b, out,
                                                          args, self.palette, refs):
                                done += 1
                                self.events.put(Job("item", (res, done, client.usage)))
                        except Exception as exc:  # noqa: BLE001
                            self.events.put(Job("log", f"batch {b.number}: {exc}"))
                finally:
                    self.events.put(Job("done", client.usage))

            threading.Thread(target=sheet_worker, daemon=True).start()
            return

        self._set_busy(True, len(items))
        self.log(f"generating {len(items)} icon(s) with {self.var_model.get()}")

        def worker() -> None:
            gen.say = lambda *p: self.events.put(Job("log", " ".join(str(x) for x in p)))
            done = 0
            try:
                with ThreadPoolExecutor(max_workers=jobs) as pool:
                    futures = [pool.submit(gen.generate_item, client, self.spec, it,
                                           out, args, self.palette, refs) for it in items]
                    for fut in futures:
                        if self.cancel.is_set():
                            fut.cancel()
                            continue
                        try:
                            res = fut.result()
                        except Exception as exc:  # noqa: BLE001
                            self.events.put(Job("log", f"error: {exc}"))
                            continue
                        done += 1
                        self.events.put(Job("item", (res, done, client.usage)))
            finally:
                self.events.put(Job("done", client.usage))

        threading.Thread(target=worker, daemon=True).start()

    def stop(self) -> None:
        self.cancel.set()
        self.log("stop requested — finishing in-flight requests")

    def recheck_all(self) -> None:
        self.refresh_tree()
        self.log("QA re-checked")

    def run_ingest(self) -> None:
        script = ROOT / "tools" / "ingest_agui_item_icons.py"
        if not script.is_file():
            messagebox.showerror(APP_TITLE, f"not found: {script}")
            return
        import subprocess
        cmd = [sys.executable, str(script), "--from-dir", str(self.out_dir())]
        self.log("$ " + " ".join(cmd))
        try:
            p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=300)
        except Exception as exc:  # noqa: BLE001
            messagebox.showerror(APP_TITLE, str(exc))
            return
        for line in (p.stdout + p.stderr).splitlines():
            self.log(line)
        self.log(f"ingest exited {p.returncode}")

    def open_out(self) -> None:
        path = self.out_dir()
        path.mkdir(parents=True, exist_ok=True)
        try:
            if sys.platform.startswith("win"):
                os.startfile(path)  # type: ignore[attr-defined]
            elif sys.platform == "darwin":
                os.system(f'open "{path}"')
            else:
                os.system(f'xdg-open "{path}"')
        except Exception as exc:  # noqa: BLE001
            messagebox.showinfo(APP_TITLE, f"{path}\n\n({exc})")

    def check_credits(self) -> None:
        try:
            client = self.client()          # built here: touches Tk vars
        except orc.OpenRouterError as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            return

        def worker() -> None:
            try:
                data = client.credits()
                total = data.get("total_credits")
                used = data.get("total_usage")
                if isinstance(total, (int, float)) and isinstance(used, (int, float)):
                    msg = f"credits: ${total - used:.4f} remaining (${used:.4f} used)"
                else:
                    msg = f"credits: {data}"
            except orc.OpenRouterError as exc:
                msg = f"credits check failed: {exc}"
            self.events.put(Job("log", msg))
        threading.Thread(target=worker, daemon=True).start()

    # -- playground --------------------------------------------------------

    def _add_ref(self) -> None:
        for p in filedialog.askopenfilenames(
                title="Reference images",
                filetypes=[("Images", "*.png *.jpg *.jpeg *.webp *.gif"), ("All", "*.*")]):
            self.ref_list.insert("end", p)

    def _clear_refs(self) -> None:
        self.ref_list.delete(0, "end")

    def play_generate(self) -> None:
        if self.busy:
            return
        prompt = self.play_prompt.get("1.0", "end").strip()
        if not prompt:
            messagebox.showinfo(APP_TITLE, "type a prompt")
            return
        if self.var_use_master.get():
            prompt = (f"{self.spec.master}\n\n{prompt}\n\n"
                      f"AVOID (negative prompt): {self.spec.negative}")
        try:
            client = self.client()
        except orc.OpenRouterError as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            return
        refs = self.refs()
        n = self.var_play_n.get()
        aspect = self.var_aspect.get().strip() or None
        transport = self.var_transport.get()
        self._set_busy(True, 1)
        self.log("playground: generating…")

        def worker() -> None:
            try:
                imgs = client.generate_image(prompt, n=n, aspect_ratio=aspect,
                                             reference_images=refs, transport=transport)
                self.events.put(Job("image", (imgs, client.usage)))
            except orc.OpenRouterError as exc:
                self.events.put(Job("log", f"error: {exc}"))
            finally:
                self.events.put(Job("done", client.usage))

        threading.Thread(target=worker, daemon=True).start()

    def play_save(self) -> None:
        if not self._play_images:
            messagebox.showinfo(APP_TITLE, "nothing to save")
            return
        dest = filedialog.asksaveasfilename(defaultextension=".png",
                                            filetypes=[("PNG", "*.png")])
        if not dest:
            return
        base = Path(dest)
        for i, img in enumerate(self._play_images):
            target = base if len(self._play_images) == 1 else \
                base.with_name(f"{base.stem}_{i + 1}{base.suffix}")
            img.save(target)
            self.log(f"wrote {target}")

    # -- chat --------------------------------------------------------------

    def _chat_attach(self) -> None:
        for p in filedialog.askopenfilenames(
                filetypes=[("Images", "*.png *.jpg *.jpeg *.webp"), ("All", "*.*")]):
            self.chat_attachments.append(p)
            self._chat_write(f"[attached {Path(p).name}]\n")

    def chat_reset(self) -> None:
        self.chat_history.clear()
        self.chat_attachments.clear()
        self.chat_text.configure(state="normal")
        self.chat_text.delete("1.0", "end")
        self.chat_text.configure(state="disabled")

    def _chat_write(self, text: str, tag: str | None = None) -> None:
        self.chat_text.configure(state="normal")
        self.chat_text.insert("end", text, tag or ())
        self.chat_text.see("end")
        self.chat_text.configure(state="disabled")

    def chat_send(self) -> None:
        if self.busy:
            return
        msg = self.chat_entry.get("1.0", "end").strip()
        if not msg:
            return
        try:
            client = self.client()
        except orc.OpenRouterError as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            return
        self.chat_entry.delete("1.0", "end")
        self._chat_write(f"\nyou\n", "user")
        self._chat_write(f"{msg}\n")
        self.chat_history.append(orc.build_user_message(msg, self.chat_attachments))
        self.chat_attachments.clear()
        history = list(self.chat_history)
        self._set_busy(True, 1)

        def worker() -> None:
            try:
                data = client.chat_raw(history)
                self.events.put(Job("chat", (orc.extract_text(data),
                                             orc.extract_images(data), client.usage)))
            except orc.OpenRouterError as exc:
                self.events.put(Job("chat", (f"[error] {exc}", [], client.usage)))
            finally:
                self.events.put(Job("done", client.usage))

        threading.Thread(target=worker, daemon=True).start()

    # -- plumbing ----------------------------------------------------------

    def _set_busy(self, busy: bool, total: int = 0) -> None:
        self.busy = busy
        if busy:
            self.cancel.clear()
            self.progress.configure(maximum=max(total, 1), value=0)
        else:
            self.progress.configure(value=0)
        state = "disabled" if busy else "normal"
        self.btn_gen.configure(state=state)
        self.btn_play.configure(state=state)
        self.btn_stop.configure(state="normal" if busy else "disabled")

    def log(self, text: str) -> None:
        self.log_text.insert("end", text.rstrip() + "\n")
        self.log_text.see("end")

    def _drain(self) -> None:
        try:
            while True:
                job = self.events.get_nowait()
                if job.kind == "log":
                    self.log(str(job.payload))
                elif job.kind == "item":
                    res, done, usage = job.payload  # type: ignore[misc]
                    self.progress.configure(value=done)
                    self._update_cost(usage)
                    # generate_item already logged the per-icon line via gen.say;
                    # only surface failures again so they stand out in the log.
                    if not res.ok:
                        label = str(res.item) if res.item else "?"
                        self.log(f"FAIL {label}  {res.note}")
                elif job.kind == "image":
                    imgs, usage = job.payload  # type: ignore[misc]
                    self._update_cost(usage)
                    self._show_play(imgs)
                elif job.kind == "chat":
                    text, imgs, usage = job.payload  # type: ignore[misc]
                    self._update_cost(usage)
                    self._chat_write(f"\n{self.var_model.get()}\n",
                                     "err" if text.startswith("[error]") else None)
                    self._chat_write(f"{text}\n")
                    if not text.startswith("[error]"):
                        self.chat_history.append({"role": "assistant", "content": text})
                    for i, img in enumerate(imgs):
                        dest = self.out_dir() / f"chat_{len(self.chat_history):02d}_{i}{img.extension}"
                        img.save(dest)
                        self._chat_write(f"[image -> {dest}]\n")
                elif job.kind == "done":
                    self._update_cost(job.payload)
                    self._set_busy(False)
                    self.refresh_tree()
        except queue.Empty:
            pass
        self.after(80, self._drain)

    def _update_cost(self, usage) -> None:
        if usage is None:
            return
        self.var_cost.set(f"${usage.cost:.4f}  ·  {usage.total_tokens:,} tok")

    def _show_play(self, imgs: list[orc.GeneratedImage]) -> None:
        self._play_images = list(imgs)
        self.play_canvas.delete("all")
        if not imgs:
            self.play_info.configure(text="model returned no image")
            return
        try:
            src = Image.open(io.BytesIO(imgs[0].data)).convert("RGBA")
        except Exception as exc:  # noqa: BLE001
            self.play_info.configure(text=f"undecodable: {exc}")
            return
        scale = min(420 / src.width, 420 / src.height)
        resample = Image.NEAREST if scale >= 1 else Image.LANCZOS
        shown = src.resize((max(1, int(src.width * scale)), max(1, int(src.height * scale))),
                           resample)
        bg = checkerboard(shown.size)
        bg.alpha_composite(shown)
        self._play_ref = ImageTk.PhotoImage(bg)
        self.play_canvas.create_image(210, 210, image=self._play_ref)
        self.play_info.configure(
            text=f"{len(imgs)} image(s) · {src.width}x{src.height} · "
                 f"{len(imgs[0].data):,} bytes · {imgs[0].media_type}")

    # -- misc --------------------------------------------------------------

    def _toggle_key(self) -> None:
        self.entry_key.configure(show="" if self.entry_key.cget("show") else "•")

    def _save_key(self) -> None:
        key = self.var_key.get().strip()
        if not key:
            return
        dest = ROOT / ".env"
        lines = []
        if dest.is_file():
            lines = [ln for ln in dest.read_text(encoding="utf-8").splitlines()
                     if not ln.startswith("OPENROUTER_API_KEY=")]
        lines.append(f"OPENROUTER_API_KEY={key}")
        dest.write_text("\n".join(lines) + "\n", encoding="utf-8")
        self.log(f"key written to {dest} — make sure .env is gitignored")

    def _pick_out(self) -> None:
        d = filedialog.askdirectory(initialdir=self.var_out.get() or str(ROOT))
        if d:
            self.var_out.set(d)
            self.refresh_tree()

    def _on_close(self) -> None:
        self.cancel.set()
        self.destroy()


def main() -> int:
    try:
        app = Studio()
    except tk.TclError as exc:
        print(f"cannot open a window: {exc}\n"
              "On Linux install python3-tk and run with a display.", file=sys.stderr)
        return 1
    except SystemExit:
        return 1
    except Exception:  # noqa: BLE001
        traceback.print_exc()
        return 1
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
