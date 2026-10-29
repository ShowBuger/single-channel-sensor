# STM32H7xx HAL Driver download script
# Downloads HAL driver and CMSIS device pack from ST's GitHub repos into firmware/libs/
# (component repos instead of the ~1 GB STM32CubeH7 bundle)
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
$HAL_VERSION   = "v1.11.6"
$CMSIS_VERSION = "v1.10.7"
$HAL_ZIP_URL   = "https://github.com/STMicroelectronics/stm32h7xx-hal-driver/archive/refs/tags/$HAL_VERSION.zip"
$CMSIS_ZIP_URL = "https://github.com/STMicroelectronics/cmsis-device-h7/archive/refs/tags/$CMSIS_VERSION.zip"

$HAL_DEST   = Join-Path $LibsDir "STM32H7xx_HAL_Driver"
$CMSIS_DEST = Join-Path $LibsDir "CMSIS\Device\ST\STM32H7xx"
$CMSIS_CORE = Join-Path $LibsDir "CMSIS\Include\cmsis_compiler.h"

Write-Sep
Write-Info "STM32H7xx HAL Driver Installer"
Write-Sep
Write-Host "  Firmware dir : $FirmwareDir"
Write-Host "  Libs dir     : $LibsDir"
Write-Host "  HAL          : $HAL_VERSION"
Write-Host "  CMSIS device : $CMSIS_VERSION"
Write-Sep
Write-Host ""

# CMSIS Core (core_cm7.h etc.) is shared by all series and already in libs/CMSIS
if (-not (Test-Path $CMSIS_CORE)) {
    Write-Err "CMSIS Core not found: $CMSIS_CORE"
    exit 1
}

# ── Check if already installed ────────────────────────────────────────────────
$halReady  = Test-Path (Join-Path $HAL_DEST   "Inc\stm32h7xx_hal.h")
$cmsiReady = Test-Path (Join-Path $CMSIS_DEST "Include\stm32h743xx.h")

if ($halReady -and $cmsiReady -and -not $Force) {
    Write-Ok "HAL driver already installed, skipping"
    Write-Ok "CMSIS device already installed, skipping"
    exit 0
}

# ── Download + extract one GitHub archive into $Dest ──────────────────────────
function Install-Archive {
    param([string]$Name, [string]$Url, [string]$Dest)

    $zip     = Join-Path $env:TEMP "$Name.zip"
    $extract = Join-Path $env:TEMP "$Name-extract"

    Write-Info "Downloading $Name..."
    Write-Info "URL: $Url"
    try {
        $ProgressPreference = "Continue"
        Invoke-WebRequest -Uri $Url -OutFile $zip -UseBasicParsing
    } catch {
        Write-Err "Download failed: $_"
        Write-Warn "Check network or download manually from:"
        Write-Warn $Url
        exit 1
    }
    $sizeMB = [math]::Round((Get-Item $zip).Length / 1MB, 1)
    Write-Ok "Downloaded ($sizeMB MB)"

    if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $extract)

    $root = Get-ChildItem $extract -Directory | Select-Object -First 1
    if (-not $root) {
        Write-Err "Could not find extracted $Name root"
        exit 1
    }

    if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
    Copy-Item -Recurse $root.FullName $Dest
    Write-Ok "$Name -> $Dest"

    Remove-Item -Recurse -Force $extract
    Remove-Item $zip -ErrorAction SilentlyContinue
}

Install-Archive "stm32h7xx-hal-driver" $HAL_ZIP_URL   $HAL_DEST
Install-Archive "cmsis-device-h7"      $CMSIS_ZIP_URL $CMSIS_DEST

# ── Verify ────────────────────────────────────────────────────────────────────
$halOk   = Test-Path (Join-Path $HAL_DEST   "Inc\stm32h7xx_hal.h")
$cmsiOk  = Test-Path (Join-Path $CMSIS_DEST "Include\stm32h743xx.h")
$startOk = Test-Path (Join-Path $CMSIS_DEST "Source\Templates\gcc\startup_stm32h743xx.s")

if (-not $halOk)   { Write-Err "stm32h7xx_hal.h not found after install"; exit 1 }
if (-not $cmsiOk)  { Write-Err "stm32h743xx.h not found after install"; exit 1 }
if (-not $startOk) { Write-Err "startup_stm32h743xx.s not found after install"; exit 1 }

Write-Host ""
Write-Sep
Write-Ok "HAL library installed successfully!"
Write-Host ""
Write-Host "  You can now build: .\tools\build.ps1" -ForegroundColor Cyan
Write-Sep
