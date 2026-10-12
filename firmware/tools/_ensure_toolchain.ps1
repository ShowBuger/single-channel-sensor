# Toolchain check and auto-install helper
# Dot-sourced by build.ps1 / flash.ps1 -- do not run directly
# Usage: . .\tools\_ensure_toolchain.ps1

# Known fixed paths for tools that conda activate may strip from PATH
$KNOWN_TOOL_PATHS = @(
    "C:\Program Files\CMake\bin",
    "C:\Program Files\ninja",
    "C:\Program Files (x86)\ninja",
    "$env:LOCALAPPDATA\Microsoft\WinGet\Links",
    "$env:LOCALAPPDATA\Programs\ninja",
    "C:\ProgramData\chocolatey\bin"
)

function Add-MissingToolPaths {
    # Rebuild PATH from registry (Machine + User) to restore anything conda stripped
    $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath    = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $fullPath    = ($machinePath + ";" + $userPath) -split ';' | Where-Object { $_ -ne "" } | Select-Object -Unique

    $current = $env:Path -split ';' | Where-Object { $_ -ne "" }
    $added = @()
    foreach ($p in $fullPath) {
        if ($p -notin $current -and (Test-Path $p)) {
            $added += $p
        }
    }
    if ($added.Count -gt 0) {
        $env:Path = ($current + $added) -join ';'
    }
}

function Ensure-ArmToolchain {
    # Restore any PATH entries that conda activation may have dropped
    Add-MissingToolPaths

    # Check cmake and ninja -- fail early with a clear message if missing
    foreach ($tool in @("cmake", "ninja")) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            Write-Host "  x Not found: $tool" -ForegroundColor Red
            Write-Host "    This is usually caused by conda activate overwriting PATH." -ForegroundColor Yellow
            Write-Host "    Run: `$env:Path += `";C:\Program Files\CMake\bin`"  (adjust path as needed)" -ForegroundColor Yellow
            exit 1
        }
    }

    # Check ARM toolchain
    if (Get-Command "arm-none-eabi-gcc" -ErrorAction SilentlyContinue) {
        return
    }

    Write-Host ""
    Write-Host "  ! ARM toolchain not in PATH, detecting..." -ForegroundColor Yellow

    $ScriptDir     = Split-Path -Parent $PSCommandPath
    $installScript = Join-Path $ScriptDir "install_toolchain.ps1"
    $envScript     = Join-Path $ScriptDir "env_setup.ps1"

    # Try to load from path recorded in env_setup.ps1
    if (Test-Path $envScript) {
        $match = Select-String -Path $envScript -Pattern '^\$ARM_TOOLCHAIN_PATH\s*=\s*"(.+)"'
        if ($match) {
            $toolchainBin = $match.Matches[0].Groups[1].Value
            if (Test-Path (Join-Path $toolchainBin "arm-none-eabi-gcc.exe")) {
                Write-Host "  + Loading toolchain from: $toolchainBin" -ForegroundColor Green
                $env:Path = "$toolchainBin;$env:Path"
                return
            }
        }
    }

    # Toolchain not installed -- run install script
    Write-Host "  ! Toolchain not found, starting auto-install..." -ForegroundColor Yellow
    Write-Host ""

    if (-not (Test-Path $installScript)) {
        Write-Host "  x Install script not found: $installScript" -ForegroundColor Red
        exit 1
    }

    & $installScript
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  x Toolchain install failed. Run manually: .\tools\install_toolchain.ps1" -ForegroundColor Red
        exit 1
    }

    # Reload path after install
    if (Test-Path $envScript) {
        $match = Select-String -Path $envScript -Pattern '^\$ARM_TOOLCHAIN_PATH\s*=\s*"(.+)"'
        if ($match) {
            $toolchainBin = $match.Matches[0].Groups[1].Value
            $env:Path = "$toolchainBin;$env:Path"
        }
    }

    if (-not (Get-Command "arm-none-eabi-gcc" -ErrorAction SilentlyContinue)) {
        Write-Host "  x Install succeeded but toolchain still not found. Reopen terminal and retry." -ForegroundColor Red
        exit 1
    }

    Write-Host "  + Toolchain loaded" -ForegroundColor Green
    Write-Host ""
}
