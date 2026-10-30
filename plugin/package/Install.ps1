param(
    [string]$InstallRoot = ""
)

$ErrorActionPreference = "Stop"
$packageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    $InstallRoot = Join-Path $env:LOCALAPPDATA "DesktopCtlPlugin\marketplace"
}
$InstallRoot = [System.IO.Path]::GetFullPath($InstallRoot)

$sourceMarketplace = Join-Path $packageRoot ".agents\plugins\marketplace.json"
$sourcePlugin = Join-Path $packageRoot "plugins\desktopctl"
$sourceExecutable = Join-Path $sourcePlugin "bin\desktopctl.exe"
if (-not (Test-Path -LiteralPath $sourceMarketplace -PathType Leaf)) {
    throw "The package marketplace file is missing: $sourceMarketplace"
}
if (-not (Test-Path -LiteralPath $sourceExecutable -PathType Leaf)) {
    throw "The packaged executable is missing: $sourceExecutable"
}

$destinationMarketplace = Join-Path $InstallRoot ".agents\plugins"
$destinationPlugin = Join-Path $InstallRoot "plugins\desktopctl"
New-Item -ItemType Directory -Force -Path $destinationMarketplace | Out-Null
New-Item -ItemType Directory -Force -Path $destinationPlugin | Out-Null
Copy-Item -LiteralPath $sourceMarketplace -Destination (Join-Path $destinationMarketplace "marketplace.json") -Force
Copy-Item -Path (Join-Path $sourcePlugin "*") -Destination $destinationPlugin -Recurse -Force

$codex = Get-Command codex -ErrorAction SilentlyContinue
if ($null -eq $codex) {
    Write-Host "Desktop Control files were installed to: $InstallRoot"
    Write-Host "Codex CLI was not found on PATH. After installing Codex Desktop, run:"
    Write-Host "  codex plugin marketplace add `"$InstallRoot`""
    Write-Host "  codex plugin add desktopctl@desktopctl-local"
    exit 0
}

$marketplaceData = (& $codex.Source plugin marketplace list --json | Out-String | ConvertFrom-Json)
$existingMarketplace = $marketplaceData.marketplaces |
    Where-Object { $_.name -eq "desktopctl-local" } |
    Select-Object -First 1
if ($null -ne $existingMarketplace -and
    [System.IO.Path]::GetFullPath($existingMarketplace.root) -ne $InstallRoot) {
    & $codex.Source plugin marketplace remove desktopctl-local | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Could not replace the existing desktopctl-local marketplace."
    }
    $existingMarketplace = $null
}
if ($null -eq $existingMarketplace) {
    & $codex.Source plugin marketplace add $InstallRoot --json
    if ($LASTEXITCODE -ne 0) {
        throw "Could not register the Desktop Control marketplace."
    }
}

& $codex.Source plugin remove desktopctl@desktopctl-local --json 2>$null | Out-Null
& $codex.Source plugin add desktopctl@desktopctl-local --json
if ($LASTEXITCODE -ne 0) {
    throw "Could not install the Desktop Control plugin."
}

Write-Host "Desktop Control was installed from: $InstallRoot"
Write-Host "Restart Codex Desktop, open a new local chat, and select Desktop Control."
