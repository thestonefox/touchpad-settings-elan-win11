# Compatibility and implementation

The tested machine is a Lenovo V155-15API with an ELAN0628 touchpad and
ELAN driver 22.4.22.2 (26 March 2020), on Windows 11 25H2 x64 build
26200.9711. This is a legacy ELAN stack, not a generic Windows Precision
Touchpad settings replacement. Other models and driver versions need
separate testing. The application does not install or update drivers.

## Settings and process reload

The driver stores per-user gesture configuration under
`HKCU\Software\Elantech\SmartPad`. The app validates existing DWORD types,
allows only a fixed set of known fields, checks for conflicting edits,
saves a typed backup, verifies writes and attempts rollback on write
failure. The tested driver INF supplies the action constants used in the
three- and four-finger action menus. Published `oemNN.inf` names vary by
machine; the original package name is `etd.inf`.

The six basic fields are Tap_Two_Finger_Enable, Zoom_Enable, SC_Enable,
SC_V_Enable, SC_H_Enable and SC_Reverse_Enable. Two-finger context tap
also uses Tap_Two_Finger (1 = right click, 7 = no action). Extended fields
cover three/four-finger tap, per-direction swipe functions and enables,
plus existing multi-finger and per-finger-count master enables. Action
codes are explicit in src/TouchpadStudio.cpp; unknown values are rejected.

The three- and four-finger groups have independent app switches. Each
explicitly disables only its supported tap/swipe enable fields and replaces
its tap action with 7 (No action). The common MultiFingerGesture_Enable
field stays on while either group is enabled. Per-count switch states and
remembered choices are stored in private local settings and typed backups;
reenabling one group restores only that group's choices. Older shared-off
files initialize both groups as disabled and retain their saved choices.
Individual tap Off also writes action 7.

ELAN Control caches settings. Apply verifies the current-session process
path against System32 ETDCtrl.exe before restarting that process. It does
not restart ETDService, the touchpad device or Windows. A reload failure
can leave valid saved settings waiting for a later Windows restart.

The restarted ETDCtrl.exe receives an explicit System32 working directory.
It does not inherit the portable app folder, which would otherwise remain
locked while the ELAN process runs even after Touchpad Studio closes.

## Contact API

Live capture loads the **installed System32 ETDApi.dll** only after checking
its file version is **11.9.4.1**. Its interface was inspected for the tested
x64 build; this is an undocumented vendor ABI, not a published stable SDK.
The version check limits accidental use with other versions, but is not
a cryptographic guarantee that all files with that version are identical.
Vendor binaries, INF files and disassemblies are not included here.

Only these exports are used: EstablishConnection, GetTouchPadSize,
GetCurrentReportIndex, AcquireFingerReport and TerminateConnection.
The inspected contact record is 64 bytes with five 12-byte contact slots.
The tested sensor reports 3209 Ã— 2097 coordinates. The app polls contact
reports, preserves intervening frames and normalizes sensor positions.
A different/missing API disables capture and the custom recognizer.

## Five-finger shortcuts

The tested driver exposes no five-finger action registry settings. The
application's separate recognizer uses the real contact stream and Windows
SendInput. It fires once, on full release, after stable contact or coherent
movement. Four contacts, long holds, spreads and retouches are rejected.
Shortcuts may coexist with the driver's own lower-finger gestures; this
is not an exclusive gesture filter. Input delivery is subject to Windows
integrity levels and focused-application behaviour.

## Optional launcher registration

The extension is a native IShellExtInit/IShellPropSheetExt COM DLL. Its
single button launches a sibling EXE; it does not load the vendor API or
write gesture configuration. ReplacePage returns S_FALSE to preserve all
standard Windows pages. Page lifetime keeps the DLL loaded appropriately.

Owned class: `{DDBD1A52-717F-43AC-AC2A-73262E8A31B8}`.

The installer creates:

- Initiating user's `Software\Classes\CLSID\{DDBD1A52-717F-43AC-AC2A-73262E8A31B8}`,
  with InprocServer32 pointing to the launcher and Apartment threading.
- Machine `SOFTWARE\Microsoft\Windows\CurrentVersion\Controls Folder\Mouse\shellex\PropertySheetHandlers\TouchpadStudio`.

Windows 11 on the tested machine needed the machine handler entry to load
the extension. That entry requires administrator approval. A per-user-only
handler registration was insufficient. The installer passes the original
user SID through elevation so alternate administrator credentials do not
change the target account. It refuses pre-existing owned keys and records
their absent state before writing. Uninstall checks that record and the
current paths before removing its own keys.

The standalone EXE is intentional: a `.cpl` is a Control Panel DLL host
format, not a requirement for a native C++ settings application. The optional
COM DLL provides the classic Mouse Properties entry point.
