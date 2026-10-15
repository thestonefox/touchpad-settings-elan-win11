param(
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'dist')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$compiler = (Resolve-Path -LiteralPath $ZigPath).Path
$version = (& $compiler version | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $version -ne '0.14.1') {
    throw 'This build is tested with Zig 0.14.1. Supply its zig.exe with -ZigPath.'
}
$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root 'build'
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $build, $output | Out-Null
$oldGlobal = $env:ZIG_GLOBAL_CACHE_DIR
$oldLocal = $env:ZIG_LOCAL_CACHE_DIR
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $build 'zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $build 'zig-local-cache'
    function Invoke-Compiler {
        & $compiler @args
        if ($LASTEXITCODE -ne 0) { throw "Compiler failed with exit code $LASTEXITCODE" }
    }
    $common = @('-target', 'x86_64-windows-gnu', '-std=c++17', '-O2', '-static', '-s')
    Invoke-Compiler rc /i "$root\resources" /fo "$build\AppResources.obj" "$root\resources\TouchpadStudio.rc"
    Invoke-Compiler c++ "$root\src\TouchpadStudio.cpp" "$build\AppResources.obj" @common -municode -fms-extensions '-Wl,--subsystem,windows' -o "$output\TouchpadStudio.exe" -ld2d1 -ldwrite -lwindowscodecs -lole32 -lcomdlg32 -ldwmapi -ladvapi32 -luser32 -lgdi32 -lshell32 -lversion -luuid
    Invoke-Compiler rc /fo "$build\LauncherResources.obj" "$root\resources\MousePropertiesLauncher.rc"
    Invoke-Compiler c++ "$root\src\MousePropertiesLauncher.cpp" "$root\src\MousePropertiesLauncher.def" "$build\LauncherResources.obj" @common -shared -o "$output\TouchpadStudio.MouseProperties.dll" -lole32 -lshell32 -lcomctl32 -luser32 -luuid
    Invoke-Compiler c++ "$root\tests\FiveFingerRecognizerTest.cpp" @common -o "$build\FiveFingerRecognizerTest.exe"
    Invoke-Compiler c++ "$root\tests\DriverGestureControlsTest.cpp" @common -fms-extensions -o "$build\DriverGestureControlsTest.exe" -ld2d1 -ldwrite -lwindowscodecs -lole32 -lcomdlg32 -ldwmapi -ladvapi32 -luser32 -lgdi32 -lshell32 -lversion -luuid
    Invoke-Compiler c++ "$root\tests\MousePropertiesLauncherTest.cpp" @common -municode -o "$build\MousePropertiesLauncherTest.exe" -lole32 -lshell32 -lcomctl32 -luser32 -luuid
    New-Item -ItemType Directory -Force -Path "$output\scripts" | Out-Null
    Copy-Item -LiteralPath "$root\scripts\Manage-MouseProperties.ps1" -Destination "$output\scripts"
    foreach ($name in @('Install-Mouse-Properties-Launcher.ps1', 'Remove-Mouse-Properties-Launcher.ps1')) {
        Copy-Item -LiteralPath "$root\scripts\$name" -Destination $output
    }
    Copy-Item -LiteralPath "$root\LICENSE" -Destination $output
    Copy-Item -LiteralPath "$root\docs\PORTABLE-README.md" -Destination "$output\README.md"
    Copy-Item -LiteralPath "$root\docs\USER-GUIDE.md" -Destination $output
    Write-Output "Built Windows x64 package: $output"
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR = $oldLocal
}
