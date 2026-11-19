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

# Source documents have Chinese filenames; frontmatter name is the stable ID.
# Validate all IDs before copying anything to avoid a partially installed set.
$skillEntries = @()
$skillIds = @{}
foreach ($skill in $skills) {
    $skillContent = Get-Content -LiteralPath $skill.FullName -Raw -Encoding UTF8
    if ($skillContent -notmatch '(?ms)\A---\r?\n(?<metadata>.*?)\r?\n---(?:\r?\n|\z)') {
        throw "Missing skill frontmatter: $($skill.Name)"
    }
    $skillMetadata = $Matches['metadata']
    if ($skillMetadata -notmatch '(?m)^name:\s*([a-z][a-z0-9_-]*)\s*$') {
        throw "Invalid or missing frontmatter name: $($skill.Name)"
    }
    $skillId = $Matches[1]
    if ($skillIds.ContainsKey($skillId)) { throw "Duplicate skill name: $skillId" }
    $skillIds[$skillId] = $true
    $skillEntries += [PSCustomObject]@{ File = $skill; Id = $skillId }
}

# ── Claude Code: .claude/agents/ ─────────────────────────────────────────────
$claudeDir = Join-Path $RepoRoot ".claude\agents"
Write-Host "  Claude Code  ->  $claudeDir" -ForegroundColor White
if (-not (Test-Path $claudeDir)) {
    New-Item -ItemType Directory -Force -Path $claudeDir | Out-Null
}
foreach ($skillEntry in $skillEntries) {
    Copy-Item -LiteralPath $skillEntry.File.FullName -Destination (Join-Path $claudeDir ($skillEntry.Id + ".md")) -Force
    Write-Host "    + /$($skillEntry.Id)" -ForegroundColor Green
}

# ── Codex CLI: .agents/skills/<skill-name>/SKILL.md ──────────────────────────
# Codex expects each skill in its own subdirectory named after the skill,
# with the instruction file named SKILL.md.
$codexBase = Join-Path $RepoRoot ".agents\skills"
Write-Host ""
Write-Host "  Codex CLI    ->  $codexBase" -ForegroundColor White
foreach ($skillEntry in $skillEntries) {
    $skillSubDir = Join-Path $codexBase $skillEntry.Id
    if (-not (Test-Path $skillSubDir)) {
        New-Item -ItemType Directory -Force -Path $skillSubDir | Out-Null
    }
    Copy-Item -LiteralPath $skillEntry.File.FullName -Destination (Join-Path $skillSubDir "SKILL.md") -Force
    Write-Host "    + /$($skillEntry.Id)" -ForegroundColor Green
}

Write-Sep
Write-Host "  Done! Installed $($skills.Count) skill(s)." -ForegroundColor Cyan
Write-Host ""
Write-Host "  Claude Code : type /commit, /flash or /hwtest in Claude Code" -ForegroundColor DarkGray
Write-Host "  Codex CLI   : type /commit, /flash or /hwtest in Codex"       -ForegroundColor DarkGray
Write-Host ""
