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

Alternate Genesis emulators (manual):

- **Regen** — debug builds had VRAM viewers; no official Windows package on winget. Search homebrew / archive sites.
- **Kega Fusion** (user): `C:\Users\Adam Templeton\Downloads\Genesis + Roms\New Folder\Fusion.exe` — no VRAM export.

Launch helper: `powershell tools/blastem_launch.ps1`

## ZSNES (user — save-state VRAM only)

`C:\Users\Adam Templeton\Downloads\SNES Gems - The Ultimate ROM Collection\SNES Gems - The Ultimate ROM Collection\zsnes1-40\zsnesw.exe`

See `tools/snes_zsnes_dump.ps1` (manual F2; keyboard automation unreliable on Win11).
