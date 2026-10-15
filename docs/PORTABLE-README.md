# Touchpad Studio 1.1.3 — Windows x64

Open **TouchpadStudio.exe** from this extracted, writable folder.
Read **USER-GUIDE.md** for controls, backups and the optional Mouse Properties
tab. Normal app use needs no administrator access.

Tested on Lenovo V155-15API, ELAN0628, driver 22.4.22.2 and Windows 11.
Live contacts require the installed System32 ETDApi.dll version 11.9.4.1.
Other models and driver stacks have not been verified.

For the optional Mouse Properties tab, run
**Install-Mouse-Properties-Launcher.ps1** from PowerShell. Use `-PlanOnly`
to inspect the intended registration first. Installation and removal need
Windows administrator approval. Keep this folder in place while registered,
including the generated registration backup. Run
**Remove-Mouse-Properties-Launcher.ps1** before moving or deleting it.

Five-finger shortcuts need the app running (minimized is fine). They start
disabled. Apply saves changes; Reset discards pending edits.

Source and release checksums:
<https://github.com/thestonefox/touchpad-settings-elan-win11>

Independent project, unaffiliated with Lenovo, ELAN or Microsoft.
MIT licence; see LICENSE. Vendor driver files are not included.
