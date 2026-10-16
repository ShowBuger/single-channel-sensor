param([switch]$SkipNpmInstall)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskBuild = Join-Path $taskRoot 'build'
$taskSdk = Join-Path $taskBuild 'webview2'
$taskDist = Join-Path (Split-Path -Parent $taskRoot) 'dist'
$taskOutput = Join-Path $taskDist 'FIELD-Release-win-x64'
$taskCompiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $taskCompiler)) { throw 'Windows .NET Framework compiler not found.' }
New-Item -ItemType Directory -Force -Path $taskBuild,$taskSdk,$taskOutput | Out-Null

if (-not $SkipNpmInstall) {
    Push-Location $taskRoot
    try { & npm.cmd ci --ignore-scripts --no-audit --no-fund; if ($LASTEXITCODE -ne 0) { throw 'Release build dependency install failed.' } }
    finally { Pop-Location }
}
& node (Join-Path $taskRoot 'build-assets.cjs')
if ($LASTEXITCODE -ne 0) { throw 'Release asset generation failed.' }

$taskPackage = Join-Path $taskSdk 'webview2.nupkg'
if (-not (Test-Path -LiteralPath $taskPackage)) {
    Invoke-WebRequest -UseBasicParsing -Uri 'https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.3800.47/microsoft.web.webview2.1.0.3800.47.nupkg' -OutFile $taskPackage
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $taskPackage).Hash -ne '56C9F26BDD07916A2D1949FB58A5C7E434DFA1173577DCA879206050C4E718DB') { throw 'WebView2 SDK package checksum mismatch.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (-not (Test-Path -LiteralPath (Join-Path $taskSdk 'lib'))) { [IO.Compression.ZipFile]::ExtractToDirectory($taskPackage,$taskSdk) }

function Compress-Resource([string]$taskInput,[string]$taskTarget) {
    $taskSourceStream = [IO.File]::OpenRead($taskInput)
    try {
        $taskTargetStream = [IO.File]::Create($taskTarget)
        try {
            $taskGzip = New-Object IO.Compression.GZipStream($taskTargetStream,[IO.Compression.CompressionLevel]::Optimal)
            try { $taskSourceStream.CopyTo($taskGzip) } finally { $taskGzip.Dispose() }
        } finally { $taskTargetStream.Dispose() }
    } finally { $taskSourceStream.Dispose() }
}
$taskRefs = @('System.Web.Extensions.dll','System.Windows.Forms.dll','System.Drawing.dll',
    (Join-Path $taskSdk 'lib\net462\Microsoft.Web.WebView2.Core.dll'),
    (Join-Path $taskSdk 'lib\net462\Microsoft.Web.WebView2.WinForms.dll'))
$taskArgs = @('/nologo','/target:winexe','/platform:x64','/optimize+','/debug-',
    '/define:FIELD_RELEASE','/main:ReleaseProgram',('/out:' + (Join-Path $taskOutput 'FIELD.exe')))
foreach ($taskRef in $taskRefs) { $taskArgs += '/reference:' + $taskRef }
foreach ($taskName in @('index.html','bundle.js')) { $taskArgs += '/resource:' + (Join-Path $taskBuild ('assets\' + $taskName + '.gz')) + ',FIELD.' + $taskName }
foreach ($taskLibrary in @('Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll','WebView2Loader.dll')) {
    $taskInput = if ($taskLibrary -eq 'WebView2Loader.dll') { Join-Path $taskSdk ('runtimes\win-x64\native\' + $taskLibrary) } else { Join-Path $taskSdk ('lib\net462\' + $taskLibrary) }
    $taskCompressed = Join-Path $taskBuild ($taskLibrary + '.gz')
    Compress-Resource $taskInput $taskCompressed
    $taskArgs += '/resource:' + $taskCompressed + ',FIELD.' + $taskLibrary
}
$taskArgs += Join-Path $taskRoot 'ReleaseHost.cs'
$taskArgs += Join-Path (Split-Path -Parent $taskRoot) 'bridge\Server.cs'
& $taskCompiler @taskArgs
if ($LASTEXITCODE -ne 0) { throw 'Release executable build failed.' }
Copy-Item -LiteralPath (Join-Path $taskRoot 'README.txt') -Destination $taskOutput -Force
Copy-Item -LiteralPath (Join-Path $taskSdk 'LICENSE.txt') -Destination (Join-Path $taskOutput 'WebView2-LICENSE.txt') -Force
$taskDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $taskOutput 'FIELD.exe')).Hash
[IO.File]::WriteAllText((Join-Path $taskOutput 'SHA256SUMS.txt'), $taskDigest + '  FIELD.exe' + [Environment]::NewLine)
$taskZip = Join-Path $taskDist 'FIELD-Release-win-x64.zip'
Compress-Archive -LiteralPath $taskOutput -DestinationPath $taskZip -Force
Write-Output ('Release package: ' + $taskZip)
Write-Output ('SHA256 FIELD.exe: ' + $taskDigest)
