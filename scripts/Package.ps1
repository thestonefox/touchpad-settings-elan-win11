param([string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'dist'))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $root 'releases\v1.1.0'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$staging = Join-Path $root ('build\package-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path "$staging\scripts" | Out-Null
# Explicit allowlist: never package personal settings or registry backups.
$files = @('TouchpadStudio.exe', 'TouchpadStudio.MouseProperties.dll',
    'Install-Mouse-Properties-Launcher.ps1', 'Remove-Mouse-Properties-Launcher.ps1',
    'LICENSE', 'README.md', 'USER-GUIDE.md', 'scripts\Manage-MouseProperties.ps1')
foreach ($file in $files) {
    Copy-Item -LiteralPath (Join-Path $OutputDirectory $file) -Destination (Join-Path $staging $file)
}
$archive = Join-Path $destination 'TouchpadStudio-v1.1.0-windows-x64.zip'
Compress-Archive -Path "$staging\*" -DestinationPath $archive -Force
Copy-Item -LiteralPath "$OutputDirectory\TouchpadStudio.exe" -Destination $destination
$lines = foreach ($file in @($archive, "$destination\TouchpadStudio.exe")) {
    $hash = Get-FileHash -LiteralPath $file -Algorithm SHA256
    '{0}  {1}' -f $hash.Hash.ToLowerInvariant(), [IO.Path]::GetFileName($file)
}
$lines | Set-Content -LiteralPath "$destination\SHA256SUMS.txt" -Encoding ASCII
Write-Output "Packaged release: $archive"
