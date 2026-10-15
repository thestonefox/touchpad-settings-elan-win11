# Touchpad Studio 1.1.0

The portable Windows x64 ZIP includes the app, optional native Mouse
Properties launcher, reversible registration scripts, user guide and MIT
licence. Extract before running. The standalone EXE also works by itself.

- Compact 720 × 470 client layout with 2/3/4/5-finger tabs.
- Live contact positions and trails using the installed ELAN API 11.9.4.1.
- Driver-backed two-, three- and four-finger settings.
- Opt-in five-finger desktop, window and media shortcuts while the app runs.
- Typed backups, direct Apply and Reset to saved state.
- Optional native launcher tab in classic Windows Mouse Properties.

Tested stack: Lenovo V155-15API, ELAN0628, driver 22.4.22.2,
Windows 11 25H2 x64. Not a generic ELAN/Precision Touchpad application.
Pinch toggling and five-contact tracking are physically confirmed;
automated checks cover mappings and recognizer logic. Every gesture/action
combination still needs hardware testing. Legacy actions may differ on
Windows 11. Binaries are unsigned. No vendor driver files are redistributed.

Verify SHA-256 hashes against SHA256SUMS.txt using Get-FileHash.
The ZIP contains the exact standalone EXE shipped beside it.

## Release validation

Built from the repository source using Zig 0.14.1 on Windows x64.

- 29 hardware-independent five-finger recognizer assertions passed.
- Direct native COM/page-lifetime and unload tests passed.
- All PowerShell scripts parsed; registration plan ran without changes.
- Read-only laptop self-test passed for six toggles, 3/4-finger mappings,
  typed backups and five-finger recognition; live registry stayed unchanged.
- Installed ELAN API connection succeeded and reported a 3209 × 2097 sensor.
- Offscreen preview generated successfully and was visually checked.
- ZIP allowlist, file identity and SHA-256 checksums verified.

No driver setting changes or new launcher registration were performed as
part of packaging. The GitHub CI workflow has not yet run remotely.
