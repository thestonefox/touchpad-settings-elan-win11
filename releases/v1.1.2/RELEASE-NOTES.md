# Touchpad Studio 1.1.2

Patch release correcting per-count gesture controls and a folder lock after
restarting ELAN Control.

## Corrections

- Three- and four-finger tabs now have independent enable switches.
  Disabling one count neutralizes its taps and disables its swipes without
  changing the other count or the separate five-finger shortcuts.
- Each count remembers its own assignments and enable states across app
  restarts and in typed backups. Older shared-off configurations initialize
  both switches off and allow either count to be restored independently.
- ELAN Control restarts with an explicit System32 working directory,
  avoiding the inherited app-folder lock after Touchpad Studio exits.

Close the previous application before running the new copy. If an older
version has already left ELAN Control holding an extracted folder open,
restart Windows after closing the app to release that existing lock.

## Package and validation

The portable Windows x64 ZIP contains the application, optional Mouse
Properties launcher, registration scripts, user guide and MIT licence.
Extract before running. Binaries are unsigned; vendor driver files are
not redistributed. Existing v1.1.0 and v1.1.1 artifacts are unchanged.

Built using the pinned Zig 0.14.1 compiler on Windows x64.

- 29 five-finger recognizer checks passed.
- 174 driver gesture control checks passed, covering independent switches,
  sibling isolation, restoration, serialization and legacy migration.
- Real child-process test passed using a harmless fixture, checking its
  explicit working directory and the unchanged parent working directory.
- Native COM/page-lifetime and unload tests passed.
- All PowerShell scripts parsed; registration plan ran without changes.
- ZIP allowlist, file identity, binary version metadata and checksums verified.
- Package scan found no known personal identifiers or recognizable secrets.

The ZIP application matches the standalone EXE. Verify both distribution
files against SHA256SUMS.txt before use. Test fixture executables, local
settings and backups are excluded from the portable package.

Validation did not change live driver settings or launcher registrations.
Physical validation on the affected laptop and a new GitHub CI run remain
pending. Compatibility remains limited to the documented ELAN stack.
