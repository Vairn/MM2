# MM2 console emulator installs (local)

Portable copies used for VRAM validation. Reinstall anytime with the commands below.

## Mesen 2 (SNES — recommended)

| Item | Path |
|------|------|
| Executable | `tools/emulators/Mesen2/Mesen.exe` |
| WinGet shim | `%LOCALAPPDATA%\Microsoft\WinGet\Links\Mesen.exe` → repo path above |
| Package ID | `SourMesen.Mesen2` (2.1.1) |

Install (portable into repo):

```powershell
winget install --id SourMesen.Mesen2 -e --accept-package-agreements --accept-source-agreements --location "$PWD\tools\emulators\Mesen2"
```

Or manual zip: https://github.com/SourMesen/Mesen2/releases/download/2.1.1/Mesen_2.1.1_Windows.zip

### Launch helper CLI — `tools/mesen_launch.ps1`

```powershell
powershell tools/mesen_launch.ps1 [-MesenExe <path>] [-Rom <path>] [-NoLaunch]
```

| Option | Default | Meaning |
|--------|---------|---------|
| `-MesenExe` | `tools/emulators/Mesen2/Mesen.exe` | Mesen 2 binary |
| `-Rom` | `EXTRACTED/SNES/Might and Magic II (Europe).sfc` | Source ROM (copied to short path) |
| `-NoLaunch` | off | Prepare ROM + print export steps only; do not start Mesen |

Copies the ROM to `EXTRACTED/snes/emulator/mm2.sfc` and prints Video RAM export steps.

VRAM export is **GUI only** (no stable CLI dump in 2.1.1). In Mesen 2, memory export is under **Debug**, not **Tools**:

1. `powershell tools/mesen_launch.ps1` (opens `mm2.sfc` windowed; prints full steps)
2. If no menu bar: **F11** (leave fullscreen), then **Alt** or mouse to top edge
3. Run to title or first-person field
4. **Debug → Memory Tools** (**Ctrl+M**)
5. Memory-type dropdown → **Video RAM** (not default **SNES Memory**)
6. Memory Tools window **File → Export** → `EXTRACTED/snes/emulator/dumps/field.vram` (65536 bytes)
7. `python tools/snes_vram_dump_stub.py --dump EXTRACTED/snes/emulator/dumps/field.vram --compare-staging`

Wrong export: 16 MiB `.dmp` from **SNES Memory** (saved under `%USERPROFILE%\Documents\Mesen2\Debugger\`) does not contain PPU VRAM.

## BlastEm 0.6.2 (Genesis — VRAM debug)

| Item | Path |
|------|------|
| Executable | `tools/emulators/BlastEm/blastem-win32-0.6.2/blastem.exe` |
| Debugger | `blastem.exe -d -m gen -r E "<rom.gen>"` |
| VRAM UI | Map `ui.vram_debug` (see `default.cfg`) |

Install (portable):

```powershell
$dest = "tools/emulators/BlastEm"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Invoke-WebRequest -Uri "https://www.retrodev.com/blastem/blastem-win32-0.6.2.zip" -OutFile "$env:TEMP\blastem-win32-0.6.2.zip"
Expand-Archive "$env:TEMP\blastem-win32-0.6.2.zip" -DestinationPath $dest -Force
```

### BlastEm CLI (used by the helper)

```text
blastem.exe -m gen -r E [-d] "<rom.gen>"
```

| Flag | Meaning |
|------|---------|
| `-m gen` | Genesis / Mega Drive |
| `-r E` | Region Europe |
| `-d` | Start in debugger |
| `<rom>` | ROM path (helper uses short `EXTRACTED/genesis/emulator/mm2.gen`) |

### Launch helper CLI — `tools/blastem_launch.ps1`

```powershell
powershell tools/blastem_launch.ps1 [-BlastEmDir <path>] [-Rom <path>] [-Debugger] [-Seconds <n>]
```

| Option | Default | Meaning |
|--------|---------|---------|
| `-BlastEmDir` | `tools/emulators/BlastEm/blastem-win32-0.6.2` | Folder containing `blastem.exe` |
| `-Rom` | `EXTRACTED/Genesis/Might and Magic - Gates to Another World (USA, Europe).gen` | Source ROM |
| `-Debugger` | off | Pass `-d` (start in debugger) |
| `-Seconds` | `0` | If &gt; 0, auto-kill BlastEm after N seconds |

VRAM: toggle `ui.vram_debug` in-game, or use debugger (`-d`) with `p` / `di`. Save states: **F5** (`.gst`); compare via `tools/genesis_vram_dump_stub.py`.

Alternate Genesis emulators (manual):

- **Regen** — debug builds had VRAM viewers; no official Windows package on winget.
- **Kega Fusion** (user): `C:\Users\Adam Templeton\Downloads\Genesis + Roms\New Folder\Fusion.exe` — no VRAM export.

## ZSNES (user — save-state VRAM only)

`C:\Users\Adam Templeton\Downloads\SNES Gems - The Ultimate ROM Collection\SNES Gems - The Ultimate ROM Collection\zsnes1-40\zsnesw.exe`

### Helper CLI — `tools/snes_zsnes_dump.ps1`

```powershell
powershell tools/snes_zsnes_dump.ps1 [-Pal] [-Seconds <n>]
```

See the script header for current switches. Keyboard automation is unreliable on Win11; prefer manual **F2** save-state then `tools/snes_zst_extract.py`.
