param(
    [string]$ExecutablePath = "",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = Split-Path -Parent $scriptRoot
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $repositoryRoot "build-vs2019\install\bin\desktopctl.exe"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot "build-vs2019\package"
}
$ExecutablePath = [System.IO.Path]::GetFullPath($ExecutablePath)
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "Executable not found: $ExecutablePath"
}

$pluginManifestPath = Join-Path $repositoryRoot "plugin\desktopctl\plugin.json"
$pluginManifest = Get-Content -LiteralPath $pluginManifestPath -Raw | ConvertFrom-Json
$packageVersion = [string]$pluginManifest.version
if ([string]::IsNullOrWhiteSpace($packageVersion)) {
    throw "Plugin version is missing from: $pluginManifestPath"
}
$cmakeContents = Get-Content -LiteralPath (Join-Path $repositoryRoot "CMakeLists.txt") -Raw
if ($cmakeContents -notmatch 'project\(desktopctl VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)') {
    throw "Unable to read the desktopctl version from CMakeLists.txt"
}
if ($Matches[1] -ne $packageVersion) {
    throw "Version mismatch: CMake reports $($Matches[1]), plugin.json reports $packageVersion"
}

$packageName = "desktopctl-plugin-$packageVersion-windows-x64"
$stageRoot = Join-Path $OutputDirectory $packageName
$zipPath = Join-Path $OutputDirectory "$packageName.zip"
$uploadZipPath = Join-Path $OutputDirectory "desktopctl-plugin-$packageVersion-upload.zip"
if (Test-Path -LiteralPath $stageRoot) {
    $stageLeaf = Split-Path -Leaf $stageRoot
    if ($stageLeaf -ne $packageName) {
        throw "Refusing to replace an unexpected staging directory: $stageRoot"
    }
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

$pluginDestination = Join-Path $stageRoot "plugins\desktopctl"
$marketplaceDestination = Join-Path $stageRoot ".agents\plugins"
New-Item -ItemType Directory -Force -Path $pluginDestination | Out-Null
New-Item -ItemType Directory -Force -Path $marketplaceDestination | Out-Null
Copy-Item -Path (Join-Path $repositoryRoot "plugin\desktopctl\*") -Destination $pluginDestination -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $pluginDestination "bin") | Out-Null
Copy-Item -LiteralPath $ExecutablePath -Destination (Join-Path $pluginDestination "bin\desktopctl.exe") -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot "plugin\marketplace.json") -Destination (Join-Path $marketplaceDestination "marketplace.json") -Force
Copy-Item -Path (Join-Path $repositoryRoot "plugin\package\*") -Destination $stageRoot -Recurse -Force

$checksums = Get-ChildItem -LiteralPath $stageRoot -File -Recurse |
    Where-Object { $_.Name -ne "checksums.sha256" } |
    Sort-Object FullName |
    ForEach-Object {
        $relativePath = [System.IO.Path]::GetRelativePath($stageRoot, $_.FullName).Replace("\", "/")
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relativePath"
    }
Set-Content -LiteralPath (Join-Path $stageRoot "checksums.sha256") -Value $checksums -Encoding ascii

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal

if (Test-Path -LiteralPath $uploadZipPath) {
    Remove-Item -LiteralPath $uploadZipPath -Force
}
Compress-Archive -Path (Join-Path $pluginDestination "*") -DestinationPath $uploadZipPath -CompressionLevel Optimal

Write-Host "Package directory: $stageRoot"
Write-Host "Package archive: $zipPath"
Write-Host "Plugin upload archive: $uploadZipPath"
Write-Host "Runtime SHA256: $((Get-FileHash -LiteralPath $ExecutablePath -Algorithm SHA256).Hash)"
