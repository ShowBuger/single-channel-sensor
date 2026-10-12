# STM32 flash script (PowerShell)
# Supports HEX flashing via OpenOCD / DAPLink / ST-Link / J-Link

param(
    [ValidateSet("openocd", "daplink", "stlink", "jlink")]
    [string]$Method = "daplink",
    [string]$HexFile = "",
    [switch]$NoVerify,
    [switch]$NoReset,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Write-Sep  { Write-Host ("=" * 44) -ForegroundColor Blue  }
function Write-Info { param($m) Write-Host "  $m"   -ForegroundColor Cyan   }
function Write-Ok   { param($m) Write-Host "  + $m" -ForegroundColor Green  }
function Write-Warn { param($m) Write-Host "  ! $m" -ForegroundColor Yellow }
function Write-Err  { param($m) Write-Host "  x $m" -ForegroundColor Red    }

if ($Help) {
    Write-Host "Usage: .\tools\flash.ps1 [options]"
    Write-Host ""
    Write-Host "  -Method   openocd|daplink|stlink|jlink   Flash tool (default: daplink)"
    Write-Host "  -HexFile  <path>                         HEX file path (default: build\)"
    Write-Host "  -NoVerify                                Skip post-flash verify"
    Write-Host "  -NoReset                                 Do not reset after flash"
    Write-Host "  -Help                                    Show this help"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\tools\flash.ps1                    # DAPLink (default)"
    Write-Host "  .\tools\flash.ps1 -Method openocd    # OpenOCD + ST-Link"
    Write-Host "  .\tools\flash.ps1 -Method stlink"
    Write-Host "  .\tools\flash.ps1 -Method jlink"
    Write-Host "  .\tools\flash.ps1 -HexFile D:\my.hex"
    Write-Host "  .\tools\flash.ps1 -NoVerify"
    exit 0
}

# Paths
$ScriptDir   = Split-Path -Parent $PSCommandPath
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir    = Join-Path $ProjectRoot "build"

if ($HexFile -eq "") {
    $HexFile = Join-Path $BuildDir "single_channel_sensor.hex"
}
$HexFile = [System.IO.Path]::GetFullPath($HexFile)

Write-Sep
Write-Info "STM32 Flash Tool"
Write-Sep
Write-Host "  Method   : $Method"
Write-Host "  HEX file : $HexFile"
Write-Host "  Verify   : $(if ($NoVerify) { 'skip' } else { 'yes' })"
Write-Host "  Reset    : $(if ($NoReset)  { 'skip' } else { 'yes' })"
Write-Sep

# Check HEX file
if (-not (Test-Path $HexFile)) {
    Write-Err "HEX file not found: $HexFile"
    Write-Warn "Build first: .\tools\build.ps1"
    exit 1
}
$hexInfo = Get-Item $HexFile
Write-Ok "HEX file: $($hexInfo.Name)  ($([math]::Round($hexInfo.Length/1KB, 1)) KB)"
Write-Host ""

# Auto-install ARM toolchain if missing (needed for objcopy in stlink mode)
. (Join-Path $ScriptDir "_ensure_toolchain.ps1")
Ensure-ArmToolchain

# Tool check helper
function Assert-Tool {
    param([string]$Name, [string]$Hint)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Write-Err "Not found: $Name"
        if ($Hint) { Write-Warn "Install: $Hint" }
        exit 1
    }
    Write-Ok "Found $Name : $((Get-Command $Name).Source)"
}

# Flash
switch ($Method) {

    "daplink" {
        Assert-Tool "openocd" "winget install xpack-dev-tools.openocd-xpack"

        $verify  = if ($NoVerify) { "" } else { "verify" }
        $reset   = if ($NoReset)  { "" } else { "reset"  }
        $hexUnix = $HexFile -replace '\\', '/'
        $cmd     = "program `"$hexUnix`" $verify $reset exit"

        Write-Warn ">> Running OpenOCD (DAPLink/CMSIS-DAP)..."
        Write-Host "   openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c `"$cmd`""
        Write-Host ""

        openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c $cmd

        if ($LASTEXITCODE -ne 0) {
            Write-Err "OpenOCD failed (exit $LASTEXITCODE)"
            exit 1
        }
    }

    "openocd" {
        Assert-Tool "openocd" "https://openocd.org  or  winget install xpack-dev-tools.openocd-xpack"

        $verify  = if ($NoVerify) { "" } else { "verify" }
        $reset   = if ($NoReset)  { "" } else { "reset"  }
        $hexUnix = $HexFile -replace '\\', '/'
        $cmd     = "program `"$hexUnix`" $verify $reset exit"

        Write-Warn ">> Running OpenOCD (ST-Link)..."
        Write-Host "   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c `"$cmd`""
        Write-Host ""

        openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c $cmd

        if ($LASTEXITCODE -ne 0) {
            Write-Err "OpenOCD failed (exit $LASTEXITCODE)"
            exit 1
        }
    }

    "stlink" {
        Assert-Tool "st-flash" "https://github.com/stlink-org/stlink"

        $binFile = [System.IO.Path]::ChangeExtension($HexFile, ".bin")

        if (Get-Command "arm-none-eabi-objcopy" -ErrorAction SilentlyContinue) {
            Write-Warn ">> Converting HEX to BIN..."
            arm-none-eabi-objcopy -I ihex -O binary $HexFile $binFile
            if ($LASTEXITCODE -ne 0) { Write-Err "HEX to BIN conversion failed"; exit 1 }
            Write-Ok "Generated: $binFile"
        } elseif (Test-Path $binFile) {
            Write-Warn "objcopy not found, using existing BIN: $binFile"
        } else {
            Write-Err "arm-none-eabi-objcopy not found, cannot convert HEX to BIN"
            Write-Warn "Load toolchain: . .\tools\env_setup.ps1"
            exit 1
        }

        Write-Warn ">> Running st-flash..."
        Write-Host "   st-flash write `"$binFile`" 0x8000000"
        Write-Host ""

        if ($NoVerify) {
            st-flash write $binFile 0x8000000
        } else {
            st-flash --verify write $binFile 0x8000000
        }

        if ($LASTEXITCODE -ne 0) { Write-Err "st-flash failed (exit $LASTEXITCODE)"; exit 1 }
    }

    "jlink" {
        Assert-Tool "JLink" "https://www.segger.com/downloads/jlink/"

        $jlinkScript = Join-Path $env:TEMP "jlink_flash_$(Get-Date -Format 'yyyyMMddHHmmss').jlink"
        $hexUnix     = $HexFile -replace '\\', '/'
        $resetCmd    = if ($NoReset) { "" } else { "r`ng" }

        @"
si SWD
speed 4000
device STM32F103C8
loadfile "$hexUnix"
$resetCmd
qc
"@ | Set-Content -Encoding ASCII $jlinkScript

        Write-Warn ">> Running J-Link..."
        Write-Host "   script: $jlinkScript"
        Write-Host ""

        JLink -CommanderScript $jlinkScript
        $rc = $LASTEXITCODE
        Remove-Item $jlinkScript -ErrorAction SilentlyContinue
        if ($rc -ne 0) { Write-Err "J-Link failed (exit $rc)"; exit 1 }
    }
}

# Done
Write-Host ""
Write-Sep
Write-Ok "Flash complete!"
Write-Host "  File   : $($hexInfo.Name)"
Write-Host "  Method : $Method"
Write-Host "  Time   : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Sep
