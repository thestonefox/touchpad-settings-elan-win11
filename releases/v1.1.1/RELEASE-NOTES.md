# Touchpad Studio 1.1.1

Patch release correcting the disabling of three- and four-finger gestures.

- Tap Off explicitly writes the driver's No action value, preventing
  Search/Cortana from firing on drivers that ignore the tap enable flag.
- The shared Multi-finger gestures switch disables both 3/4-finger taps
  and swipes and restores previous choices when enabled again.
- Remembered choices persist in private local settings and typed backups.
  Individual controls are inactive while the shared switch is off.
- Two-finger settings and the separate five-finger switch are unchanged.

Close the previous app before running the new copy. If the shared switch
was already off in an older version, turn it on then off before clicking
Apply to stage the explicit disabling of tap actions.

The portable Windows x64 ZIP includes the app, optional Mouse Properties
launcher, registration scripts, user guide and MIT licence. Extract before
running. Binaries are unsigned; no vendor driver files are redistributed.
Verify SHA-256 hashes against SHA256SUMS.txt. The ZIP's application matches
the standalone EXE shipped beside it. The v1.1.0 artifacts are preserved.

## Validation

Built using the pinned Zig 0.14.1 compiler on Windows x64.

- 29 five-finger recognizer checks passed.
- 52 driver gesture control checks passed, covering tap Off, shared
  disable, restoration, serialization and rejected invalid metadata.
- Native COM/page-lifetime and unload tests passed.
- All PowerShell scripts parsed; registration plan ran without changes.
- ZIP allowlist, file identity, version metadata and checksums verified.
- Package scan found no known personal identifiers or recognizable secrets.

No live driver settings or launcher registrations were changed during
validation. The correction still needs physical laptop validation and a
new GitHub CI run. Compatibility remains limited to the documented ELAN
stack; this is not a generic Windows Precision Touchpad application.
