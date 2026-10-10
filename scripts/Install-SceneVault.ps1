param([string]$PluginRoot = "$env:ProgramData\obs-studio\plugins")
$ErrorActionPreference = 'Stop'
if (Get-Process -Name obs64 -ErrorAction SilentlyContinue) { throw 'Close OBS before installing Scene Vault.' }
$source = Join-Path $PSScriptRoot 'scene-vault'
if (!(Test-Path (Join-Path $source 'bin\64bit\scene-vault.dll'))) {
    throw 'The compiled Windows package is required. This source ZIP cannot be installed directly.'
}
$destination = Join-Path $PluginRoot 'scene-vault'
$backup = "$destination.backup"
New-Item -ItemType Directory -Path $PluginRoot -Force | Out-Null
if (Test-Path $backup) { throw "An earlier backup exists at $backup. Move it before installing again." }
if (Test-Path $destination) { Move-Item -LiteralPath $destination -Destination $backup }
try { Copy-Item -LiteralPath $source -Destination $destination -Recurse }
catch {
    if (Test-Path $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
    if (Test-Path $backup) { Move-Item -LiteralPath $backup -Destination $destination }
    throw
}
Write-Host 'Scene Vault installed. Open OBS, then Tools > Scene Vault.'
if (Test-Path $backup) { Write-Host "Previous plugin retained at $backup" }
