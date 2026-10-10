param([string]$PluginRoot = "$env:ProgramData\obs-studio\plugins")
$ErrorActionPreference = 'Stop'
if (Get-Process -Name obs64 -ErrorAction SilentlyContinue) { throw 'Close OBS before uninstalling Scene Vault.' }
$destination = Join-Path $PluginRoot 'scene-vault'
if (Test-Path $destination) {
    Remove-Item -LiteralPath $destination -Recurse -Force
    Write-Host 'Scene Vault removed. All scenes will be visible when OBS next opens.'
} else { Write-Host 'Scene Vault is not installed at this location.' }
