$ErrorActionPreference = "Stop"
$codex = Get-Command codex -ErrorAction SilentlyContinue
if ($null -eq $codex) {
    throw "Codex CLI was not found on PATH."
}

& $codex.Source plugin remove desktopctl@desktopctl-local --json
if ($LASTEXITCODE -ne 0) {
    Write-Warning "The plugin was not installed or could not be removed."
}
& $codex.Source plugin marketplace remove desktopctl-local
if ($LASTEXITCODE -ne 0) {
    Write-Warning "The marketplace was not registered or could not be removed."
}

$installedFiles = Join-Path $env:LOCALAPPDATA "DesktopCtlPlugin\marketplace"
Write-Host "Desktop Control was unregistered. Installed files were left at: $installedFiles"
Write-Host "They can be removed manually after Codex Desktop is closed."
