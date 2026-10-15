param([string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'dist'))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
& "$root\build\FiveFingerRecognizerTest.exe"
if ($LASTEXITCODE -ne 0) { throw 'Five-finger recognizer tests failed.' }
$dll = (Resolve-Path -LiteralPath "$OutputDirectory\TouchpadStudio.MouseProperties.dll").Path
& "$root\build\MousePropertiesLauncherTest.exe" $dll
if ($LASTEXITCODE -ne 0) { throw 'Mouse Properties COM tests failed.' }
$files = Get-ChildItem -LiteralPath "$root\scripts" -Filter '*.ps1'
foreach ($file in $files) {
    $tokens = $null
    $parseErrors = $null
    $null = [Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count) { throw "PowerShell parse error in $($file.Name): $parseErrors" }
}
& "$OutputDirectory\Install-Mouse-Properties-Launcher.ps1" -PlanOnly | Out-Host
'PASS: script parsing and registration plan. No registration changed.'
