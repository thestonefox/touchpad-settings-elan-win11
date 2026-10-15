# Build from source

Use 64-bit Windows 11 and PowerShell. The source is C++17; Zig supplies
Clang, a linker and Windows headers. No Zig-language source is used.
There is no runtime dependency on Zig, Python, .NET or PowerShell for
running the application.

1. Download the official **Zig 0.14.1 Windows x86_64** archive:
   <https://ziglang.org/download/0.14.1/zig-x86_64-windows-0.14.1.zip>
2. Verify the archive before extracting it:

   ```powershell
   Get-FileHash .\zig-x86_64-windows-0.14.1.zip -Algorithm SHA256
   ```

   Expected SHA-256:
   `554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c`
3. From the repository root:

   ```powershell
   .\scripts\Build.ps1 -ZigPath C:\Tools\zig-0.14.1\zig.exe
   .\scripts\Test.ps1
   ```

The portable application and optional registration scripts appear in
`dist/`. Objects, test executables and compiler caches stay in `build/`.
Both folders are ignored by Git. Close an app built into `dist/` before
rebuilding it. Build.ps1 restores any existing Zig cache environment
variables after the build. It does not download anything or register COM.

To create the distributable ZIP, standalone EXE and SHA256SUMS.txt:

```powershell
.\scripts\Package.ps1
```

Package.ps1 copies an explicit allowlist of release files. It excludes
personal settings, gesture backups and registration records even if they
exist in `dist/`. Build and test before packaging. Commit the resulting
`releases/v1.1.2/` files when intentionally updating this release.
The build procedure is repeatable; byte-identical PE/ZIP output is not
promised because toolchain timestamps and ZIP metadata may vary.

## Tests

`Test.ps1` runs entirely without ELAN hardware or registration changes:

- Real recognizer code: tap, four swipes, release timing, repeat suppression,
  disabled state, four contacts, long holds, retouches, spreads and resets.
  Five-finger tap-only mode suppresses all swipes and restores assignments.
- Driver gesture controls: explicit tap neutralization, per-count disable,
  independent 3/4-finger switches, exact restoration, legacy migration and
  missing driver fields.
- Native launcher: COM factory, interfaces, added page, standard page
  preservation, page lifetime and unload. It loads the built DLL directly.
- ELAN reload launch: a harmless fixture executable checks that the child
  uses its explicit directory and the parent directory is unchanged. The
  test never stops, launches or connects to the installed ELAN driver.
- PowerShell syntax and read-only installation plan.

On the supported laptop, a further **read-only** integration check is available:

```powershell
$log = Join-Path $PWD 'build\hardware-self-test.txt'
$p = Start-Process .\dist\TouchpadStudio.exe -ArgumentList "--self-test `"$log`"" -Wait -PassThru
Get-Content $log
$p.ExitCode
```

This reads the current user's SmartPad settings and briefly opens the
installed contact API. It tests mapping/staging and backup serialization.
It does not apply settings, restart ETDCtrl or send shortcuts. It requires
that user's compatible driver settings; it is not a generic CI test.
Physical gestures still need manual verification on each supported setup.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/TouchpadStudio.cpp` | Win32 UI, settings, backups, ELAN contact API |
| `src/FiveFingerRecognizer.h` | Pure five-contact recognizer |
| `src/MousePropertiesLauncher.cpp` | Optional native COM launcher tab |
| `resources/` | Icon, manifests, dialogs and version resources |
| `scripts/` | Build, tests, packaging and reversible registration |
| `tests/` | Hardware-independent C++ checks |
| `docs/` | User guide and compatibility notes |
| `releases/` | Ready-to-run release artifacts and checksums |

C++ formatting follows `.clang-format` (clang-format 19). Preserve the
registry allowlist and API version guard when extending compatibility.
Use scoped conventional commits and keep every commit-message line at
72 characters or fewer.

## Continuous integration

The Windows workflow builds and runs hardware-independent checks on pushes
to main, pull requests and manual dispatch. It downloads the pinned Zig
archive and verifies its SHA-256 before use. Repository access is read-only;
the workflow neither pushes commits nor publishes GitHub releases.
