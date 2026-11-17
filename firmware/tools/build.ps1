# STM32 CMake build script (PowerShell)

param(
    [ValidateSet("ma03_h743", "ma01_l432", "ma02_f103")]
    [string]$Board = "ma03_h743",
    [switch]$Release,
    [switch]$Clean,
    [switch]$Verbose,
    [switch]$Help,
    [int]$Jobs = 8
)

$ErrorActionPreference = "Stop"

if ($Help) {
    Write-Host "Usage: .\tools\build.ps1 [options]"
    Write-Host ""
    Write-Host "  -Board B     ma03_h743 (default) | ma01_l432 | ma02_f103"
    Write-Host "  -Release     Release build (default: Debug)"
    Write-Host "  -Clean       Clean before build"
    Write-Host "  -Verbose     Verbose compiler output"
    Write-Host "  -Jobs N      Parallel jobs (default: 8)"
    Write-Host "  -Help        Show this help"
    exit 0
}

# Auto-install ARM toolchain if missing
$ScriptDir = Split-Path -Parent $PSCommandPath
. (Join-Path $ScriptDir "_ensure_toolchain.ps1")
Ensure-ArmToolchain

# Paths
$ProjectRoot = Split-Path -Parent $ScriptDir
# All boards share firmware/build. A marker file identifies which board the
# tree belongs to, so switching -Board (or branches) cleans it automatically.
$BuildDir    = Join-Path $ProjectRoot "build"
$BuildId     = $Board
$BuildType   = if ($Release) { "Release" } else { "Debug" }

Write-Host ("=" * 44) -ForegroundColor Blue
Write-Host "STM32 CMake Build" -ForegroundColor Blue
Write-Host ("=" * 44) -ForegroundColor Blue
Write-Host "  Project : $ProjectRoot"
Write-Host "  Board   : $Board"
Write-Host "  Build   : $BuildDir"
Write-Host "  Type    : $BuildType"
Write-Host "  Jobs    : $Jobs"
Write-Host ("=" * 44) -ForegroundColor Blue

# Stale build guard: one build directory, but never mix two boards' objects.
$Marker    = Join-Path $BuildDir ".build_board"
$CacheFile = Join-Path $BuildDir "CMakeCache.txt"
$stale     = $null
if (Test-Path $Marker) {
    $cachedId = (Get-Content -Path $Marker -Raw).Trim()
    if ($cachedId -ne $BuildId) { $stale = "$cachedId -> $BuildId" }
} elseif (Test-Path $CacheFile) {
    $stale = "legacy build tree without .build_board"
}
if (-not $stale -and (Test-Path $CacheFile)) {
    $m = Select-String -Path $CacheFile -Pattern '^BOARD:STRING=(.+)$' -ErrorAction SilentlyContinue |
         Select-Object -First 1
    if ($m) {
        $cachedBoard = $m.Matches[0].Groups[1].Value.Trim()
        if ($cachedBoard -ne $BuildId) { $stale = "CMakeCache BOARD $cachedBoard -> $BuildId" }
    }
}
if ($stale) {
    Write-Host ">> Build dir belongs to another board ($stale); cleaning..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

# Check required tools
Write-Host ">> Checking toolchain..." -ForegroundColor Yellow
$tools = @{
    "arm-none-eabi-gcc" = "ARM GNU Toolchain"
    "cmake"             = "CMake 3.22+"
    "ninja"             = "Ninja"
}
foreach ($tool in $tools.Keys) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Host "  x Not found: $tool  (install: $($tools[$tool]))" -ForegroundColor Red
        exit 1
    }
}
Write-Host "  + Toolchain OK" -ForegroundColor Green
Write-Host "    GCC   : $((arm-none-eabi-gcc --version | Select-Object -First 1))"
Write-Host "    CMake : $((cmake --version | Select-Object -First 1))"
Write-Host "    Ninja : $(ninja --version)"

# Clean
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host ">> Cleaning..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
    Write-Host "  + Clean done" -ForegroundColor Green
}

# Record which board owns this build tree (written after Clean so it survives).
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Content -Path $Marker -Value $BuildId -Encoding ASCII

# CMake configure
Write-Host ">> CMake configure..." -ForegroundColor Yellow

$cmakeArgs = @(
    "-B", $BuildDir,
    "-S", $ProjectRoot,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DBOARD=$Board",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "  x CMake configure failed" -ForegroundColor Red
    exit 1
}
Write-Host "  + CMake configure done" -ForegroundColor Green

# Build
Write-Host ">> Building..." -ForegroundColor Yellow
$buildArgs = @("--build", $BuildDir, "-j", $Jobs)
if ($Verbose) { $buildArgs += "--verbose" }
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "  x Build failed" -ForegroundColor Red
    exit 1
}
Write-Host "  + Build succeeded" -ForegroundColor Green

# Size report
Write-Host ("=" * 44) -ForegroundColor Blue
Write-Host ">> Firmware size:" -ForegroundColor Yellow
$elfFile = Join-Path $BuildDir "single_channel_sensor.elf"
if (Test-Path $elfFile) { & arm-none-eabi-size $elfFile }

# Output files
Write-Host ">> Output files:" -ForegroundColor Yellow
foreach ($ext in @("*.elf", "*.hex", "*.bin")) {
    Get-ChildItem -Path $BuildDir -Filter $ext -ErrorAction SilentlyContinue | ForEach-Object {
        $kb = "{0:N1} KB" -f ($_.Length / 1KB)
        Write-Host "    $($_.Name)  ($kb)"
    }
}

Write-Host "  + Build complete!" -ForegroundColor Green
Write-Host ("=" * 44) -ForegroundColor Blue
