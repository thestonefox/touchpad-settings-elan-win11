param([switch]$PlanOnly)
$ErrorActionPreference = 'Stop'
$action = if ($PlanOnly) { 'Plan' } else { 'Install' }
& (Join-Path $PSScriptRoot 'scripts\Manage-MouseProperties.ps1') -Action $action
