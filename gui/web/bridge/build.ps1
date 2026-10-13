$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskCompiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if (-not (Test-Path -LiteralPath $taskCompiler)) { $taskCompiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe' }
if (-not (Test-Path -LiteralPath $taskCompiler)) { throw 'Windows .NET Framework C# compiler not found.' }
& $taskCompiler /nologo /target:exe /optimize+ /reference:System.Web.Extensions.dll ("/out:" + (Join-Path $taskRoot 'FIELD.exe')) (Join-Path $PSScriptRoot 'Server.cs')
if ($LASTEXITCODE -ne 0) { throw 'FIELD build failed.' }
Write-Output "Built $taskRoot/FIELD.exe"
