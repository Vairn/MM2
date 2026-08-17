#!/usr/bin/env python3
"""Small, dependency-free interface to OpenRouter (chat + image models).

Stdlib only (urllib) so it runs anywhere Python 3.9+ does. Pillow is optional
and only used by the `--show-info` helper.

Use as a library
----------------
    from openrouter_client import OpenRouterClient

    or = OpenRouterClient()                       # key from env / .env / ~/.openrouter
    print(or.chat("one sentence on the Amiga OCS"))

    imgs = or.generate_image("a red panda astronaut", aspect_ratio="1:1")
    imgs[0].save("panda.png")

    # image editing / style reference (goes through chat-completions)
    imgs = or.generate_image("recolour this icon to gold",
                             reference_images=["icons/use.png"])

Use as a CLI
------------
    python openrouter_client.py chat  "why is 68k fun?"
    python openrouter_client.py chat  "describe this" --image shot.png
    python openrouter_client.py image "a red panda astronaut" -o panda.png -n 2
    python openrouter_client.py image "make it gold" --ref icon.png -o gold.png
    python openrouter_client.py repl                       # interactive session
    python openrouter_client.py models --filter image
    python openrouter_client.py credits

API key resolution order
------------------------
    1. --api-key / OpenRouterClient(api_key=...)
    2. $OPENROUTER_API_KEY
    3. .env  (OPENROUTER_API_KEY=sk-or-...) in cwd, then each parent dir
    4. ~/.openrouter_key   (file containing just the key)
"""
from __future__ import annotations

import argparse
import base64
import json
import mimetypes
import os
import random
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Sequence

__all__ = [
    "OpenRouterClient",
    "OpenRouterError",
    "GeneratedImage",
    "DEFAULT_IMAGE_MODEL",
    "DEFAULT_TEXT_MODEL",
]

BASE_URL = "https://openrouter.ai/api/v1"
DEFAULT_IMAGE_MODEL = "google/gemini-3.1-flash-image"
DEFAULT_TEXT_MODEL = "google/gemini-3.1-flash-image"

# ---------------------------------------------------------------------------
# errors
# ---------------------------------------------------------------------------


class OpenRouterError(RuntimeError):
    """Any non-recoverable failure talking to OpenRouter."""

    def __init__(self, message: str, *, status: int | None = None, body: str = ""):
        super().__init__(message)
        self.status = status
        self.body = body


# ---------------------------------------------------------------------------
# key discovery
# ---------------------------------------------------------------------------


def _read_dotenv(start: Path | None = None) -> str | None:
    here = (start or Path.cwd()).resolve()
    for folder in [here, *here.parents]:
        env = folder / ".env"
        if not env.is_file():
            continue
        try:
            for raw in env.read_text(encoding="utf-8", errors="replace").splitlines():
                line = raw.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                name, _, value = line.partition("=")
                if name.strip() == "OPENROUTER_API_KEY":
                    return value.strip().strip("'\"")
        except OSError:
            pass
    return None


def resolve_api_key(explicit: str | None = None) -> str:
    if explicit:
        return explicit.strip()
    env = os.environ.get("OPENROUTER_API_KEY")
    if env and env.strip():
        return env.strip()
    dotenv = _read_dotenv()
    if dotenv:
        return dotenv
    for candidate in (Path.home() / ".openrouter_key", Path.home() / ".openrouter"):
        if candidate.is_file():
            text = candidate.read_text(encoding="utf-8").strip()
            if text:
                return text.splitlines()[0].strip()
    raise OpenRouterError(
        "No OpenRouter API key found.\n"
        "  set OPENROUTER_API_KEY=sk-or-...            (PowerShell: $env:OPENROUTER_API_KEY='sk-or-...')\n"
        "  or put OPENROUTER_API_KEY=sk-or-... in a .env beside the repo\n"
        "  or pass --api-key sk-or-..."
    )


# ---------------------------------------------------------------------------
# results
# ---------------------------------------------------------------------------


@dataclass
class GeneratedImage:
    """One image returned by the model."""

    data: bytes
    media_type: str = "image/png"
    index: int = 0

    @property
    def extension(self) -> str:
        return {
            "image/png": ".png",
            "image/jpeg": ".jpg",
            "image/webp": ".webp",
            "image/svg+xml": ".svg",
        }.get(self.media_type, ".png")

    def save(self, path: str | os.PathLike[str]) -> Path:
        dest = Path(path)
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(self.data)
        return dest

    def __len__(self) -> int:  # pragma: no cover - convenience
        return len(self.data)


@dataclass
class Usage:
    prompt_tokens: int = 0
    completion_tokens: int = 0
    total_tokens: int = 0
    cost: float = 0.0

    def add(self, other: "Usage") -> None:
        self.prompt_tokens += other.prompt_tokens
        self.completion_tokens += other.completion_tokens
        self.total_tokens += other.total_tokens
        self.cost += other.cost

    @classmethod
    def from_payload(cls, payload: Any) -> "Usage":
        if not isinstance(payload, dict):
            return cls()
        return cls(
            prompt_tokens=int(payload.get("prompt_tokens") or 0),
            completion_tokens=int(payload.get("completion_tokens") or 0),
            total_tokens=int(payload.get("total_tokens") or 0),
            cost=float(payload.get("cost") or 0.0),
        )

    def __str__(self) -> str:
        return (f"{self.total_tokens} tok "
                f"(in {self.prompt_tokens} / out {self.completion_tokens}) "
                f"${self.cost:.4f}")


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------


def encode_image_file(path: str | os.PathLike[str]) -> str:
    """Read a local image and return a `data:` URL suitable for image_url."""
    p = Path(path)
    if not p.is_file():
        raise OpenRouterError(f"reference image not found: {p}")
    mime = mimetypes.guess_type(p.name)[0] or "image/png"
    return f"data:{mime};base64," + base64.b64encode(p.read_bytes()).decode("ascii")


def _as_image_url(ref: str) -> str:
    """Accept a URL, an existing data: URL, or a local path."""
    if ref.startswith(("http://", "https://", "data:")):
        return ref
    return encode_image_file(ref)


def _decode_data_url(url: str) -> GeneratedImage | None:
    if not url.startswith("data:"):
        return None
    header, _, payload = url.partition(",")
    if not payload:
        return None
    media = header[5:].split(";")[0] or "image/png"
    if ";base64" in header:
        try:
            return GeneratedImage(base64.b64decode(payload), media)
        except (ValueError, TypeError):
            return None
    return GeneratedImage(payload.encode("utf-8"), media)


# ---------------------------------------------------------------------------
# client
# ---------------------------------------------------------------------------


class OpenRouterClient:
    """Thin wrapper over the OpenRouter REST API.

    Parameters
    ----------
    api_key      explicit key; otherwise discovered (see module docstring)
    model        default model slug for every call
    referer/title optional HTTP-Referer / X-Title app attribution headers
    timeout      per-request socket timeout in seconds
    max_retries  retries for 429 / 5xx / transient network errors
    """

    def __init__(
        self,
        api_key: str | None = None,
        model: str = DEFAULT_IMAGE_MODEL,
        *,
        referer: str = "https://github.com/Vairn/MM2",
        title: str = "MM2 asset pipeline",
        timeout: float = 300.0,
        max_retries: int = 5,
        base_url: str = BASE_URL,
        verbose: bool = False,
    ) -> None:
        self.api_key = resolve_api_key(api_key)
        self.model = model
        self.referer = referer
        self.title = title
        self.timeout = timeout
        self.max_retries = max_retries
        self.base_url = base_url.rstrip("/")
        self.verbose = verbose
        self.usage = Usage()

    # -- transport ---------------------------------------------------------

    def _headers(self) -> dict[str, str]:
        h = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        if self.referer:
            h["HTTP-Referer"] = self.referer
        if self.title:
            h["X-Title"] = self.title
        return h

    def _log(self, *parts: object) -> None:
        if self.verbose:
            print("[openrouter]", *parts, file=sys.stderr)

    def request(self, method: str, path: str, payload: dict | None = None) -> dict:
        url = f"{self.base_url}/{path.lstrip('/')}"
        body = json.dumps(payload).encode("utf-8") if payload is not None else None
        last: Exception | None = None

        for attempt in range(self.max_retries + 1):
            req = urllib.request.Request(url, data=body, headers=self._headers(), method=method)
            try:
                self._log(method, url, f"attempt {attempt + 1}")
                with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                    raw = resp.read().decode("utf-8", errors="replace")
                data = json.loads(raw) if raw else {}
                # OpenRouter can return HTTP 200 with an error object inside.
                if isinstance(data, dict) and isinstance(data.get("error"), dict):
                    err = data["error"]
                    code = err.get("code")
                    message = err.get("message", "unknown error")
                    if code in (429, 500, 502, 503, 504) and attempt < self.max_retries:
                        self._sleep(attempt, None)
                        continue
                    raise OpenRouterError(f"{code}: {message}", status=code, body=raw)
                return data
            except urllib.error.HTTPError as exc:
                raw = exc.read().decode("utf-8", errors="replace")
                retryable = exc.code == 429 or 500 <= exc.code < 600
                if retryable and attempt < self.max_retries:
                    last = exc
                    self._sleep(attempt, exc.headers.get("Retry-After"))
                    continue
                raise OpenRouterError(
                    f"HTTP {exc.code} from {url}: {_short(raw)}", status=exc.code, body=raw
                ) from exc
            except (urllib.error.URLError, TimeoutError, json.JSONDecodeError, ConnectionError) as exc:
                last = exc
                if attempt < self.max_retries:
                    self._sleep(attempt, None)
                    continue
                raise OpenRouterError(f"network failure talking to {url}: {exc}") from exc

        raise OpenRouterError(f"exhausted retries for {url}: {last}")

    def _sleep(self, attempt: int, retry_after: str | None) -> None:
        if retry_after:
            try:
                delay = float(retry_after)
            except ValueError:
                delay = 2.0 ** attempt
        else:
            delay = 2.0 ** attempt
        delay = min(delay, 60.0) + random.uniform(0, 0.75)
        self._log(f"retrying in {delay:.1f}s")
        time.sleep(delay)

    # -- chat --------------------------------------------------------------

    def chat_raw(
        self,
        messages: list[dict],
        *,
        model: str | None = None,
        modalities: Sequence[str] | None = None,
        image_config: dict | None = None,
        temperature: float | None = None,
        max_tokens: int | None = None,
        extra: dict | None = None,
    ) -> dict:
        """POST /chat/completions and return the parsed response."""
        payload: dict[str, Any] = {"model": model or self.model, "messages": messages}
        if modalities:
            payload["modalities"] = list(modalities)
        if image_config:
            payload["image_config"] = image_config
        if temperature is not None:
            payload["temperature"] = temperature
        if max_tokens is not None:
            payload["max_tokens"] = max_tokens
        if extra:
            payload.update(extra)
        data = self.request("POST", "/chat/completions", payload)
        self.usage.add(Usage.from_payload(data.get("usage")))
        return data

    def chat(
        self,
        prompt: str,
        *,
        system: str | None = None,
        images: Iterable[str] | None = None,
        model: str | None = None,
        temperature: float | None = None,
        max_tokens: int | None = None,
        history: list[dict] | None = None,
    ) -> str:
        """Single-shot text chat. `images` may be paths, URLs or data URLs."""
        messages = list(history or [])
        if system and not any(m.get("role") == "system" for m in messages):
            messages.insert(0, {"role": "system", "content": system})
        messages.append(build_user_message(prompt, images))
        data = self.chat_raw(
            messages, model=model, temperature=temperature, max_tokens=max_tokens
        )
        return extract_text(data)

    # -- images ------------------------------------------------------------

    def generate_image(
        self,
        prompt: str,
        *,
        model: str | None = None,
        n: int = 1,
        aspect_ratio: str | None = None,
        output_format: str = "png",
        reference_images: Sequence[str] | None = None,
        transport: str = "auto",
        system: str | None = None,
        extra: dict | None = None,
    ) -> list[GeneratedImage]:
        """Generate (or edit) images.

        transport:
          "auto"   -> chat-completions when reference images are supplied,
                      otherwise try /images and fall back to chat.
          "images" -> POST /images  (no reference-image support)
          "chat"   -> POST /chat/completions with modalities=["image","text"]
        """
        model = model or self.model
        refs = list(reference_images or [])

        if transport == "images" and refs:
            raise OpenRouterError(
                "the /images endpoint takes no reference images; use transport='chat'"
            )
        if transport == "chat" or (transport == "auto" and refs):
            return self._image_via_chat(
                prompt, model=model, n=n, aspect_ratio=aspect_ratio,
                reference_images=refs, system=system, extra=extra,
            )
        if transport == "images":
            return self._image_via_images(
                prompt, model=model, n=n, aspect_ratio=aspect_ratio,
                output_format=output_format, extra=extra,
            )

        # auto, no refs: prefer the dedicated endpoint, fall back to chat.
        try:
            out = self._image_via_images(
                prompt, model=model, n=n, aspect_ratio=aspect_ratio,
                output_format=output_format, extra=extra,
            )
            if out:
                return out
            self._log("/images returned no image; falling back to chat")
        except OpenRouterError as exc:
            if exc.status is not None and exc.status not in (400, 404, 405, 415, 422, 501):
                raise
            self._log(f"/images unavailable ({exc}); falling back to chat")
        return self._image_via_chat(
            prompt, model=model, n=n, aspect_ratio=aspect_ratio,
            reference_images=refs, system=system, extra=extra,
        )

    def _image_via_images(
        self,
        prompt: str,
        *,
        model: str,
        n: int,
        aspect_ratio: str | None,
        output_format: str,
        extra: dict | None,
    ) -> list[GeneratedImage]:
        payload: dict[str, Any] = {"model": model, "prompt": prompt, "n": n}
        if aspect_ratio:
            payload["aspect_ratio"] = aspect_ratio
        if output_format:
            payload["output_format"] = output_format
        if extra:
            payload.update(extra)
        data = self.request("POST", "/images", payload)
        self.usage.add(Usage.from_payload(data.get("usage")))

        out: list[GeneratedImage] = []
        for i, entry in enumerate(data.get("data") or []):
            if not isinstance(entry, dict):
                continue
            media = entry.get("media_type") or entry.get("mime_type") or "image/png"
            if entry.get("b64_json"):
                try:
                    out.append(GeneratedImage(base64.b64decode(entry["b64_json"]), media, i))
                except (ValueError, TypeError):
                    continue
            elif entry.get("url"):
                img = _decode_data_url(entry["url"])
                if img:
                    img.index = i
                    out.append(img)
        return out

    def _image_via_chat(
        self,
        prompt: str,
        *,
        model: str,
        n: int,
        aspect_ratio: str | None,
        reference_images: Sequence[str],
        system: str | None,
        extra: dict | None,
    ) -> list[GeneratedImage]:
        messages: list[dict] = []
        if system:
            messages.append({"role": "system", "content": system})
        messages.append(build_user_message(prompt, reference_images))

        image_config = {"aspect_ratio": aspect_ratio} if aspect_ratio else None
        payload_extra = dict(extra or {})
        if n > 1:
            payload_extra.setdefault("n", n)

        data = self.chat_raw(
            messages,
            model=model,
            modalities=["image", "text"],
            image_config=image_config,
            extra=payload_extra or None,
        )
        return extract_images(data)

    # -- misc endpoints ----------------------------------------------------

    def models(self, *, output_modality: str | None = None) -> list[dict]:
        data = self.request("GET", "/models")
        items = data.get("data") or []
        if output_modality:
            items = [
                m for m in items
                if output_modality in (m.get("architecture", {}) or {}).get("output_modalities", [])
            ]
        return items

    def credits(self) -> dict:
        return self.request("GET", "/credits").get("data", {})


# ---------------------------------------------------------------------------
# response parsing (module-level so callers can reuse them)
# ---------------------------------------------------------------------------


def build_user_message(prompt: str, images: Iterable[str] | None = None) -> dict:
    refs = [r for r in (images or []) if r]
    if not refs:
        return {"role": "user", "content": prompt}
    content: list[dict] = [{"type": "text", "text": prompt}]
    for ref in refs:
        content.append({"type": "image_url", "image_url": {"url": _as_image_url(ref)}})
    return {"role": "user", "content": content}


def extract_text(response: dict) -> str:
    parts: list[str] = []
    for choice in response.get("choices") or []:
        message = choice.get("message") or {}
        content = message.get("content")
        if isinstance(content, str):
            parts.append(content)
        elif isinstance(content, list):
            for piece in content:
                if isinstance(piece, dict) and piece.get("type") == "text":
                    parts.append(piece.get("text", ""))
                elif isinstance(piece, str):
                    parts.append(piece)
    return "\n".join(p for p in parts if p).strip()


def extract_images(response: dict) -> list[GeneratedImage]:
    """Pull images out of a chat-completions response.

    Handles the documented `message.images[].image_url.url` shape plus the
    variants providers occasionally emit (inline content parts, b64_json).
    """
    out: list[GeneratedImage] = []

    def take(url: str | None) -> None:
        if not url:
            return
        img = _decode_data_url(url)
        if img:
            img.index = len(out)
            out.append(img)

    for choice in response.get("choices") or []:
        message = choice.get("message") or {}
        for entry in message.get("images") or []:
            if isinstance(entry, str):
                take(entry)
            elif isinstance(entry, dict):
                if isinstance(entry.get("image_url"), dict):
                    take(entry["image_url"].get("url"))
                elif isinstance(entry.get("image_url"), str):
                    take(entry["image_url"])
                elif entry.get("b64_json"):
                    try:
                        out.append(GeneratedImage(
                            base64.b64decode(entry["b64_json"]),
                            entry.get("media_type", "image/png"),
                            len(out),
                        ))
                    except (ValueError, TypeError):
                        pass
                else:
                    take(entry.get("url"))
        content = message.get("content")
        if isinstance(content, list):
            for piece in content:
                if not isinstance(piece, dict):
                    continue
                if piece.get("type") in ("image_url", "output_image", "image"):
                    url = piece.get("image_url")
                    if isinstance(url, dict):
                        take(url.get("url"))
                    elif isinstance(url, str):
                        take(url)
                    elif piece.get("data"):
                        try:
                            out.append(GeneratedImage(
                                base64.b64decode(piece["data"]),
                                piece.get("media_type", "image/png"),
                                len(out),
                            ))
                        except (ValueError, TypeError):
                            pass
    return out


def _short(text: str, limit: int = 600) -> str:
    text = " ".join(text.split())
    return text if len(text) <= limit else text[:limit] + "…"


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _client_from_args(args: argparse.Namespace) -> OpenRouterClient:
    return OpenRouterClient(
        api_key=getattr(args, "api_key", None),
        model=getattr(args, "model", None) or DEFAULT_IMAGE_MODEL,
        timeout=getattr(args, "timeout", 300.0),
        verbose=getattr(args, "verbose", False),
    )


def _cmd_chat(args: argparse.Namespace) -> int:
    client = _client_from_args(args)
    text = client.chat(
        args.prompt,
        system=args.system,
        images=args.image,
        temperature=args.temperature,
        max_tokens=args.max_tokens,
    )
    print(text)
    if args.verbose:
        print(f"\n-- {client.usage}", file=sys.stderr)
    return 0


def _cmd_image(args: argparse.Namespace) -> int:
    client = _client_from_args(args)
    prompt = args.prompt
    if args.negative:
        prompt = f"{prompt}\n\nAVOID (negative prompt): {args.negative}"
    images = client.generate_image(
        prompt,
        n=args.n,
        aspect_ratio=args.aspect,
        reference_images=args.ref,
        transport=args.transport,
        system=args.system,
    )
    if not images:
        print("model returned no image", file=sys.stderr)
        return 1

    out = Path(args.out)
    for i, img in enumerate(images):
        dest = out if len(images) == 1 else out.with_name(f"{out.stem}_{i + 1}{out.suffix or img.extension}")
        if not dest.suffix:
            dest = dest.with_suffix(img.extension)
        img.save(dest)
        print(f"wrote {dest}  ({len(img.data):,} bytes, {img.media_type})")
    print(f"-- {client.usage}", file=sys.stderr)
    return 0


def _cmd_models(args: argparse.Namespace) -> int:
    client = _client_from_args(args)
    items = client.models(output_modality="image" if args.filter == "image" else None)
    needle = (args.grep or "").lower()
    for m in items:
        slug = m.get("id", "")
        if needle and needle not in slug.lower() and needle not in (m.get("name", "").lower()):
            continue
        pricing = m.get("pricing", {}) or {}
        print(f"{slug:<52} in={pricing.get('prompt', '?')} out={pricing.get('completion', '?')}")
    return 0


def _cmd_credits(args: argparse.Namespace) -> int:
    client = _client_from_args(args)
    data = client.credits()
    total = data.get("total_credits")
    used = data.get("total_usage")
    print(json.dumps(data, indent=2))
    if isinstance(total, (int, float)) and isinstance(used, (int, float)):
        print(f"\nremaining: ${total - used:.4f}")
    return 0


REPL_HELP = """\
commands:
  /image <prompt>     generate an image (saved to ./or_image_NN.png)
  /attach <path>      attach a local image to the next message
  /clear-attach       drop pending attachments
  /system <text>      set the system prompt (resets history)
  /model <slug>       switch model
  /reset              clear conversation history
  /save <path>        write the transcript to a file
  /usage              show tokens + cost so far
  /help  /quit
anything else is sent as a chat message.\
"""


def _cmd_repl(args: argparse.Namespace) -> int:
    client = _client_from_args(args)
    history: list[dict] = []
    attachments: list[str] = []
    system: str | None = args.system
    if system:
        history.append({"role": "system", "content": system})
    saved = 0

    print(f"OpenRouter REPL — model {client.model}. /help for commands, /quit to exit.")
    while True:
        try:
            line = input("\n> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not line:
            continue

        if line in ("/quit", "/exit", "/q"):
            break
        if line in ("/help", "/?"):
            print(REPL_HELP)
            continue
        if line == "/usage":
            print(client.usage)
            continue
        if line == "/reset":
            history = [m for m in history if m.get("role") == "system"]
            print("history cleared")
            continue
        if line == "/clear-attach":
            attachments.clear()
            print("attachments cleared")
            continue
        if line.startswith("/attach "):
            path = line[8:].strip().strip('"')
            if Path(path).is_file():
                attachments.append(path)
                print(f"attached {path} ({len(attachments)} pending)")
            else:
                print(f"no such file: {path}")
            continue
        if line.startswith("/model "):
            client.model = line[7:].strip()
            print(f"model -> {client.model}")
            continue
        if line.startswith("/system "):
            system = line[8:].strip()
            history = [{"role": "system", "content": system}]
            print("system prompt set, history reset")
            continue
        if line.startswith("/save "):
            dest = Path(line[6:].strip().strip('"'))
            dest.write_text(json.dumps(history, indent=2), encoding="utf-8")
            print(f"transcript -> {dest}")
            continue
        if line.startswith("/image "):
            prompt = line[7:].strip()
            try:
                images = client.generate_image(prompt, reference_images=attachments)
            except OpenRouterError as exc:
                print(f"error: {exc}")
                continue
            if not images:
                print("no image returned")
                continue
            for img in images:
                saved += 1
                dest = Path(f"or_image_{saved:02d}{img.extension}")
                img.save(dest)
                print(f"wrote {dest} ({len(img.data):,} bytes)")
            attachments.clear()
            continue

        history.append(build_user_message(line, attachments))
        attachments.clear()
        try:
            data = client.chat_raw(history)
        except OpenRouterError as exc:
            print(f"error: {exc}")
            history.pop()
            continue
        reply = extract_text(data)
        images = extract_images(data)
        print(reply or "(no text)")
        for img in images:
            saved += 1
            dest = Path(f"or_image_{saved:02d}{img.extension}")
            img.save(dest)
            print(f"[wrote {dest}]")
        history.append({"role": "assistant", "content": reply})

    print(f"\n{client.usage}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        prog="openrouter_client.py",
        description="Interface to OpenRouter chat + image models.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("API key resolution")[0].split("Use as a CLI")[-1],
    )
    ap.add_argument("--api-key", help="override key discovery")
    ap.add_argument("--model", default=DEFAULT_IMAGE_MODEL, help=f"model slug (default {DEFAULT_IMAGE_MODEL})")
    ap.add_argument("--timeout", type=float, default=300.0)
    ap.add_argument("-v", "--verbose", action="store_true")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("chat", help="one-shot text completion")
    p.add_argument("prompt")
    p.add_argument("--system")
    p.add_argument("--image", action="append", default=[], help="attach an image (repeatable)")
    p.add_argument("--temperature", type=float)
    p.add_argument("--max-tokens", type=int)
    p.set_defaults(func=_cmd_chat)

    p = sub.add_parser("image", help="generate or edit an image")
    p.add_argument("prompt")
    p.add_argument("-o", "--out", default="out.png")
    p.add_argument("-n", type=int, default=1, help="how many images")
    p.add_argument("--aspect", help='aspect ratio, e.g. "1:1" or "16:9"')
    p.add_argument("--negative", help="appended to the prompt as an AVOID clause")
    p.add_argument("--ref", action="append", default=[], help="reference/source image (repeatable)")
    p.add_argument("--system")
    p.add_argument("--transport", choices=["auto", "images", "chat"], default="auto")
    p.set_defaults(func=_cmd_image)

    p = sub.add_parser("repl", help="interactive session")
    p.add_argument("--system")
    p.set_defaults(func=_cmd_repl)

    p = sub.add_parser("models", help="list available models")
    p.add_argument("--filter", choices=["image", "all"], default="all")
    p.add_argument("--grep", help="substring match on slug/name")
    p.set_defaults(func=_cmd_models)

    p = sub.add_parser("credits", help="show account credit balance")
    p.set_defaults(func=_cmd_credits)
    return ap


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except OpenRouterError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\ninterrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
