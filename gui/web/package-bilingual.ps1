$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
& powershell.exe -NoProfile -File (Join-Path $taskRoot 'bridge/build.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Bridge compilation failed.' }
& node --test (Join-Path $taskRoot 'tests/core.test.cjs') (Join-Path $taskRoot 'tests/import.test.cjs') (Join-Path $taskRoot 'tests/bridge.test.cjs')
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
$taskOutput = Join-Path $taskRoot 'dist/FIELD-Internal-Bilingual-win-x64'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
foreach ($taskName in @('FIELD.exe','index.html','core.js','app.js','bridge-client.js','import-ui.js','i18n.js','Start FIELD.cmd','启动上位机.cmd','README-Bilingual.txt')) {
    Copy-Item -LiteralPath (Join-Path $taskRoot $taskName) -Destination $taskOutput -Force
}
$taskManifest = Get-ChildItem -LiteralPath $taskOutput -File | Where-Object Name -ne 'SHA256SUMS.txt' | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash + '  ' + $_.Name }
$taskManifest | Set-Content -Encoding ASCII -LiteralPath (Join-Path $taskOutput 'SHA256SUMS.txt')
$taskZip = Join-Path $taskRoot 'dist/FIELD-Internal-Bilingual-win-x64.zip'
Compress-Archive -LiteralPath $taskOutput -DestinationPath $taskZip -Force
Write-Output ('Package: ' + $taskZip)
