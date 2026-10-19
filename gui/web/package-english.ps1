# Compatibility entry point. The GUI now ships with both languages.
$ErrorActionPreference = 'Stop'
& powershell.exe -NoProfile -File (Join-Path $PSScriptRoot 'package-bilingual.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Bilingual package build failed.' }
