# Install skills for Claude Code and/or Codex CLI
# Usage: .\docs\skills\install_skills.ps1
# Run once after cloning.

$ErrorActionPreference = "Stop"

$RepoRoot  = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSCommandPath))
$SkillsDir = Join-Path $RepoRoot "docs\skills"

function Write-Sep { Write-Host ("-" * 44) -ForegroundColor DarkGray }

Write-Host ""
Write-Host "  Skills Installer" -ForegroundColor Cyan
Write-Sep

# Collect skill .md files (exclude README and this script)
$skills = Get-ChildItem -Path $SkillsDir -Filter "*.md" |
          Where-Object { $_.Name -ne "README.md" }

if ($skills.Count -eq 0) {
    Write-Host "  No skill files found in $SkillsDir" -ForegroundColor Yellow
    exit 0
}

# ── Claude Code: .claude/agents/ ─────────────────────────────────────────────
$claudeDir = Join-Path $RepoRoot ".claude\agents"
Write-Host "  Claude Code  ->  $claudeDir" -ForegroundColor White
if (-not (Test-Path $claudeDir)) {
    New-Item -ItemType Directory -Force -Path $claudeDir | Out-Null
}
foreach ($skill in $skills) {
    Copy-Item -Path $skill.FullName -Destination (Join-Path $claudeDir $skill.Name) -Force
    Write-Host "    + /$($skill.BaseName)" -ForegroundColor Green
}

# ── Codex CLI: .agents/skills/<skill-name>/SKILL.md ──────────────────────────
# Codex expects each skill in its own subdirectory named after the skill,
# with the instruction file named SKILL.md.
$codexBase = Join-Path $RepoRoot ".agents\skills"
Write-Host ""
Write-Host "  Codex CLI    ->  $codexBase" -ForegroundColor White
foreach ($skill in $skills) {
    $skillSubDir = Join-Path $codexBase $skill.BaseName
    if (-not (Test-Path $skillSubDir)) {
        New-Item -ItemType Directory -Force -Path $skillSubDir | Out-Null
    }
    Copy-Item -Path $skill.FullName -Destination (Join-Path $skillSubDir "SKILL.md") -Force
    Write-Host "    + /$($skill.BaseName)" -ForegroundColor Green
}

Write-Sep
Write-Host "  Done! Installed $($skills.Count) skill(s)." -ForegroundColor Cyan
Write-Host ""
Write-Host "  Claude Code : type /commit or /flash in Claude Code" -ForegroundColor DarkGray
Write-Host "  Codex CLI   : type /commit or /flash in Codex"       -ForegroundColor DarkGray
Write-Host ""
