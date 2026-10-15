# Touchpad Studio 1.1.3

Patch release correcting two-finger scrolling/pinch after disabling the
three-/four-finger groups, and matching the five-finger swipe controls.

## Corrections and recovery

- Keep MultiFingerGesture_Enable on even when both higher-finger groups
  are off. The shared driver engine also gates two-finger scrolling and
  pinch; disabled groups remain suppressed through their own fields.
- If an earlier version switched that engine off, startup and Reset stage
  a repair. Click Apply when shown Apply to repair gestures. No driver
  values are changed until Apply; existing two-finger preferences and
  disabled three-/four-finger groups are preserved.
- The five-finger tab now has a separate Swipes switch, matching the
  three-/four-finger tabs. Swipes off preserves taps and swipe assignments.
  The switch persists in local settings and backups. Older settings retain
  the previous swipe behaviour. Five-finger shortcuts still need the app open.
- The v1.1.2 ELAN working-directory correction is retained.

Close the previous app before running this version. Keep private settings
and backups out of shared downloads. Follow USER-GUIDE.md for controls.

## Validation and distribution

Built with pinned Zig 0.14.1 on Windows x64.

- The new shared-engine regression assertion failed against v1.1.2 before
  the correction and passed afterward.
- 40 recognizer checks passed, including tap-only mode and swipe restoration.
- 221 settings/control checks passed, including two-finger preservation,
  staged recovery, group isolation, serialization and legacy settings.
- Real child-process working-directory regression test passed.
- Native COM/page-lifetime and unload tests passed.
- All PowerShell scripts parsed and the read-only registration plan passed.
- The seven-row five-finger layout was rendered and visually inspected
  using simulated contacts without accessing the installed ELAN driver.
- Package allowlist, EXE/DLL versions, checksums and privacy checks passed.

The ZIP contains the app, optional native launcher, registration scripts,
guide and MIT licence. Its EXE matches the standalone EXE. Verify both
against SHA256SUMS.txt. Prior release files are unchanged. Binaries remain
unsigned; vendor drivers, test fixtures and private state are not packaged.

Automated validation did not modify live driver settings. Scrolling/pinch
recovery and gestures still need verification on the affected laptop;
a new GitHub CI run also remains pending.
