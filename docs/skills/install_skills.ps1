# Install Claude Code Skills
# Usage: .\docs\skills\install_skills.ps1
# Run this after cloning to install /commit and /flash skills for Claude Code.

$ErrorActionPreference = "Stop"

$RepoRoot   = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
$SkillsDir  = Join-Path $RepoRoot "docs\skills"
$AgentsDir  = Join-Path $RepoRoot ".claude\agents"

Write-Host "Installing Claude Code skills..." -ForegroundColor Cyan
Write-Host "  Source : $SkillsDir"
Write-Host "  Target : $AgentsDir"
Write-Host ""

# Create .claude/agents if it doesn't exist
if (-not (Test-Path $AgentsDir)) {
    New-Item -ItemType Directory -Force -Path $AgentsDir | Out-Null
}

# Copy all .md files from docs/skills to .claude/agents
$skills = Get-ChildItem -Path $SkillsDir -Filter "*.md" |
          Where-Object { $_.Name -ne "README.md" -and $_.Name -ne "install_skills.ps1" }

foreach ($skill in $skills) {
    $dest = Join-Path $AgentsDir $skill.Name
    Copy-Item -Path $skill.FullName -Destination $dest -Force
    Write-Host "  + Installed: /$($skill.BaseName)" -ForegroundColor Green
}

Write-Host ""
Write-Host "Done! Available skills in Claude Code:" -ForegroundColor Cyan
foreach ($skill in $skills) {
    Write-Host "  /$($skill.BaseName)"
}
