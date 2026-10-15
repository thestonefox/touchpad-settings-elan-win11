# Using Touchpad Studio

Extract the portable ZIP into a writable folder, such as a folder under
Documents. Open TouchpadStudio.exe as your normal Windows user. Keep
settings and backup files with that copy of the application. Avoid
Program Files, Windows system folders and running directly inside a ZIP.

## Changing gestures

Choose the 2-, 3-, 4- or 5-finger tab, change the settings and click **Apply**.
Changes are staged until applied. **Reset** discards pending edits and
rereads saved values; it is not a factory reset.

Choosing a three- or four-finger action enables the required driver
master settings. The master switches control actual gesture behaviour.
Unavailable or unsupported registry values are not invented by the app.
Legacy actions are labelled because the driver predates Windows 11.

Each tab has its own **Enable 3-finger gestures** or **Enable 4-finger
gestures** switch. Turning one off and clicking Apply disables only that
count's taps and swipes. The other count and five-finger shortcuts remain
independent. Tap actions are explicitly set to No action because some
ELAN drivers ignore their enable flags. Previous assignments and enable
states are remembered in TouchpadStudio.settings across app restarts and
restored when that count is enabled and applied again. Individual controls
are inactive while their count is off. The Swipes switch affects only that
count's swipes; choosing Off for an individual tap also clears its action.

The driver still has a shared multi-finger gate internally. The app keeps
it enabled whenever either three- or four-finger gestures are enabled,
while disabling each count's fields separately. Older shared-off settings
initialize both switches to off; either count can then be restored alone.

Driver changes restart only the verified System32 ETDCtrl.exe process in
the current Windows session. The contact connection pauses and reconnects.
A successful reload normally avoids restarting Windows. If reload fails,
the app reports that settings were saved but a reload failed; restarting
Windows can make the saved driver settings take effect.

Closing Touchpad Studio exits the app. The existing ELAN Control process
continues handling driver gestures; it is restarted with a System32 working
directory so it does not keep the extracted application folder in use.
If an older version already restarted ELAN Control in that folder, restart
Windows after closing the app before trying to move or delete the folder.

The five-finger master switch and action choices are local app settings.
They require the app open or minimized. The shortcut fires after all five
fingers lift, with timing and movement checks to reduce accidental actions.
Held keyboard modifiers suppress shortcuts. Windows may reject input into
an elevated application. There is no background service or startup entry.

## Live view

The left panel displays contact slots and trails from the installed ELAN
API. A slot number is a driver contact ID, not identification of a specific
finger. The panel shows sensor positions, not screen coordinates.
Reports/sec counts sensor records, not display frames. Contact data stays
local; the app has no network client or analytics.

The live view requires ETDApi.dll 11.9.4.1. A missing or different version
disables contact capture and custom five-finger gestures. Compatible
registry settings can still be edited. Use **â€¦ â†’ Reconnect live view**
if necessary. Do not replace vendor DLLs just to match this app.

## Backups and restore

Apply first saves the current SmartPad registry values with their original
types and bytes, plus applied five-finger app settings, into **ELAN-backups/**.
Each `.elanbackup` is tied to the Windows user who created it.

Use **â€¦ â†’ Load a backupâ€¦** to stage supported values from a backup, then
click Apply to restore them. Loading alone does not change saved settings.
The app writes only its supported gesture allowlist; a full snapshot does
not mean arbitrary registry values can be restored through the UI.

Keep backups private: they include your Windows user identifier and local
configuration. Do not commit them to Git or include them in bug reports.
Older PowerShell `.clixml` backups are not read by this native application.

Five-finger choices and temporarily disabled 3/4-finger assignments are
saved to **TouchpadStudio.settings** beside the EXE. This file also contains
your Windows user identifier; keep it private along with registration JSON
files and registration-plan output, which can include local paths.
Copying that file to a different Windows account will fail user validation.
Deleting it while the app is closed returns custom shortcuts to disabled
and loses remembered 3/4-finger assignments. It does not reset driver values.

## Mouse Properties launcher (optional)

Keep TouchpadStudio.exe, TouchpadStudio.MouseProperties.dll, both wrapper
scripts and the scripts/ folder together. The extracted ZIP supplies them.

In PowerShell opened in that folder:

```powershell
.\Install-Mouse-Properties-Launcher.ps1 -PlanOnly
.\Install-Mouse-Properties-Launcher.ps1
```

The first command prints the intended paths without changing registration.
The second requests Windows administrator approval and adds a native
launcher tab to classic Mouse Properties (`main.cpl`). It registers one
machine handler and a COM class for the initiating user. Other accounts
are not automatically configured. It does not alter driver registrations,
keyboard filters or gesture values.

Close every Mouse Properties window and reopen it to see the new tab.
Click **Open Touchpad Studio** on that tab. The launcher uses the EXE beside
its DLL; keep the complete extracted folder at its registered location.

To uninstall the launcher:

```powershell
.\Remove-Mouse-Properties-Launcher.ps1
```

Approve the Windows administrator prompt. Removal checks the saved
registration record and deletes only the two owned registration subtrees.
It leaves the app and gesture settings in place. Keep the generated
**TouchpadStudio-registration-backup.json** until removal completes.
If a previous installation exists, remove it from its original folder
before registering a new copy. Registration refuses to overwrite conflicts.

The modern Windows Settings page cannot host this portable app as a new
native touchpad section. Its additional-settings link may lead to Mouse
Properties; the exact link and wording depend on the Windows/driver setup.

## Troubleshooting

| Symptom | What to do |
| --- | --- |
| No SmartPad settings found | Check the compatibility list and that the existing ELAN driver is installed for this user. |
| Controls unchanged after clicking | Click Apply; changing a control only stages it. If saved/reload failed appears, follow that error. |
| Settings changed elsewhere | Click Reset and retry your edits. |
| Cannot save backup or app settings | Extract to a writable folder and run as your normal user. |
| Live view unavailable | Check the displayed reason and installed API version; try Reconnect. |
| Five-finger actions stop after closing | Keep the app running or minimized. |
| Launcher tab missing | Close all Mouse Properties windows and reopen; check installation succeeded for this account. |
| Script execution is blocked | For a trusted, inspected download, unblock the files or use a process-only PowerShell execution-policy override. No machine-wide policy change is required. |

Keyboard: Tab/Shift+Tab moves focus, Space/Enter activates the focused
control, Ctrl+S applies, and Escape resets pending edits.
