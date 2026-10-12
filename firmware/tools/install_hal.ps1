# STM32F1xx HAL Driver download script
# Downloads HAL driver and CMSIS from ST's GitHub mirror into firmware/libs/
# Usage: .\tools\install_hal.ps1

param(
    [string]$FirmwareDir = "",    # defaults to the firmware/ dir next to tools/
    [switch]$Force,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Write-Sep  { Write-Host ("=" * 52) -ForegroundColor Blue }
function Write-Info { param($m) Write-Host "  $m"   -ForegroundColor Cyan   }
function Write-Ok   { param($m) Write-Host "  + $m" -ForegroundColor Green  }
function Write-Warn { param($m) Write-Host "  ! $m" -ForegroundColor Yellow }
function Write-Err  { param($m) Write-Host "  x $m" -ForegroundColor Red    }

if ($Help) {
    Write-Host "Usage: .\tools\install_hal.ps1 [options]"
    Write-Host ""
    Write-Host "  -FirmwareDir <path>   Firmware root (default: parent of tools/)"
    Write-Host "  -Force                Re-download even if already present"
    Write-Host "  -Help                 Show this help"
    exit 0
}

$ScriptDir   = Split-Path -Parent $PSCommandPath
if ($FirmwareDir -eq "") { $FirmwareDir = Split-Path -Parent $ScriptDir }
$LibsDir     = Join-Path $FirmwareDir "libs"

# ── Download targets ──────────────────────────────────────────────────────────
# ST's GitHub releases for STM32CubeF1 v1.8.6
$HAL_ZIP_URL  = "https://github.com/STMicroelectronics/STM32CubeF1/archive/refs/tags/v1.8.6.zip"
$HAL_ZIP_FILE = Join-Path $env:TEMP "STM32CubeF1-1.8.6.zip"
$HAL_EXTRACT  = Join-Path $env:TEMP "STM32CubeF1-extract"

$HAL_DEST   = Join-Path $LibsDir "STM32F1xx_HAL_Driver"
$CMSIS_DEST = Join-Path $LibsDir "CMSIS"

Write-Sep
Write-Info "STM32F1xx HAL Driver Installer"
Write-Sep
Write-Host "  Firmware dir : $FirmwareDir"
Write-Host "  Libs dir     : $LibsDir"
Write-Sep
Write-Host ""

# ── Check if already installed ────────────────────────────────────────────────
$halReady   = Test-Path (Join-Path $HAL_DEST  "Inc\stm32f1xx_hal.h")
$cmsiReady  = Test-Path (Join-Path $CMSIS_DEST "Include\cmsis_compiler.h")

if ($halReady -and $cmsiReady -and -not $Force) {
    Write-Ok "HAL driver already installed, skipping"
    Write-Ok "CMSIS already installed, skipping"
    exit 0
}

# ── Download ──────────────────────────────────────────────────────────────────
if (-not (Test-Path $HAL_ZIP_FILE) -or $Force) {
    Write-Info "Downloading STM32CubeF1 v1.8.6 (~80 MB)..."
    Write-Info "URL: $HAL_ZIP_URL"
    Write-Host ""
    try {
        $ProgressPreference = "Continue"
        Invoke-WebRequest -Uri $HAL_ZIP_URL -OutFile $HAL_ZIP_FILE -UseBasicParsing
    } catch {
        Write-Err "Download failed: $_"
        Write-Warn "Check network or download manually from:"
        Write-Warn $HAL_ZIP_URL
        exit 1
    }
    $sizeMB = [math]::Round((Get-Item $HAL_ZIP_FILE).Length / 1MB, 1)
    Write-Ok "Downloaded ($sizeMB MB)"
} else {
    Write-Warn "Archive already exists, skipping download"
}

# ── Extract ───────────────────────────────────────────────────────────────────
Write-Info "Extracting archive..."
if (Test-Path $HAL_EXTRACT) { Remove-Item -Recurse -Force $HAL_EXTRACT }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory($HAL_ZIP_FILE, $HAL_EXTRACT)
Write-Ok "Extracted to temp dir"

$cubeRoot = Get-ChildItem $HAL_EXTRACT -Directory | Select-Object -First 1
if (-not $cubeRoot) {
    Write-Err "Could not find extracted CubeF1 root"
    exit 1
}
$cubeRoot = $cubeRoot.FullName
Write-Info "CubeF1 root: $cubeRoot"

# ── Copy HAL driver ───────────────────────────────────────────────────────────
$halSrc = Join-Path $cubeRoot "Drivers\STM32F1xx_HAL_Driver"
if (-not (Test-Path $halSrc)) {
    Write-Err "HAL driver not found at: $halSrc"
    exit 1
}
if (Test-Path $HAL_DEST) { Remove-Item -Recurse -Force $HAL_DEST }
Copy-Item -Recurse $halSrc $HAL_DEST
Write-Ok "HAL driver -> $HAL_DEST"

# ── Copy CMSIS ────────────────────────────────────────────────────────────────
$cmsisSrc = Join-Path $cubeRoot "Drivers\CMSIS"
if (-not (Test-Path $cmsisSrc)) {
    Write-Err "CMSIS not found at: $cmsisSrc"
    exit 1
}
if (Test-Path $CMSIS_DEST) { Remove-Item -Recurse -Force $CMSIS_DEST }
Copy-Item -Recurse $cmsisSrc $CMSIS_DEST
Write-Ok "CMSIS -> $CMSIS_DEST"

# ── Cleanup ───────────────────────────────────────────────────────────────────
Remove-Item -Recurse -Force $HAL_EXTRACT
Remove-Item $HAL_ZIP_FILE -ErrorAction SilentlyContinue
Write-Ok "Cleaned up temp files"

# ── Verify ────────────────────────────────────────────────────────────────────
$halOk   = Test-Path (Join-Path $HAL_DEST  "Inc\stm32f1xx_hal.h")
$cmsiOk  = Test-Path (Join-Path $CMSIS_DEST "Include\cmsis_compiler.h")
$startOk = Test-Path (Join-Path $CMSIS_DEST "Device\ST\STM32F1xx\Source\Templates\gcc\startup_stm32f103xb.s")

if (-not $halOk)   { Write-Err "stm32f1xx_hal.h not found after install"; exit 1 }
if (-not $cmsiOk)  { Write-Err "cmsis_compiler.h not found after install"; exit 1 }
if (-not $startOk) { Write-Err "startup_stm32f103xb.s not found after install"; exit 1 }

Write-Host ""
Write-Sep
Write-Ok "HAL library installed successfully!"
Write-Host ""
Write-Host "  You can now build: .\tools\build.ps1" -ForegroundColor Cyan
Write-Sep
