#Requires -Version 5.1
<#
.SYNOPSIS
    Configure, build, and optionally package Deck for Windows (MSVC).

.DESCRIPTION
    Uses CMake + Visual Studio 2022 (x64). JUCE 8 does not support MinGW.
    See scripts/BUILD-WINDOWS.md for prerequisites and step-by-step usage.

.PARAMETER BuildDir
    CMake build directory (default: build).

.PARAMETER Config
    Debug or Release (default: Release).

.PARAMETER Tests
    Enable CASSETTE_BUILD_TESTS and run ctest after the build.

.PARAMETER Package
    Copy the built executable into dist\Windows and create a ZIP archive.

.PARAMETER Clean
    Delete the build directory before configuring.

.PARAMETER Jobs
    Parallel build jobs (0 = let CMake pick).

.PARAMETER Generator
    CMake generator override (default: Visual Studio 17 2022).

.EXAMPLE
    .\scripts\build_windows_msvc.ps1

.EXAMPLE
    .\scripts\build_windows_msvc.ps1 -Package

.EXAMPLE
    .\scripts\build_windows_msvc.ps1 -Clean -Tests -Package
#>
[CmdletBinding()]
param(
    [string] $BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string] $Config = "Release",
    [switch] $Tests,
    [switch] $Package,
    [switch] $Clean,
    [int] $Jobs = 0,
    [string] $Generator = "Visual Studio 17 2022"
)

$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    $root = Resolve-Path (Join-Path $PSScriptRoot "..")
    return $root.Path
}

function Assert-Command {
    param([string] $Name, [string] $InstallHint)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required tool '$Name' was not found in PATH.`n$InstallHint"
    }
}

function Get-ProjectVersion {
    param([string] $CMakeListsPath)
    $content = Get-Content -LiteralPath $CMakeListsPath -Raw
    if ($content -match 'project\s*\(\s*Deck\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        return $Matches[1]
    }
    return "0.0.0"
}

function Find-DeckExecutable {
    param([string] $ArtifactsDir)
    if (-not (Test-Path $ArtifactsDir)) {
        throw "Build output folder not found: $ArtifactsDir"
    }

    $candidates = @(
        Join-Path $ArtifactsDir "Deck.exe"
        Join-Path $ArtifactsDir "Deck_artefacts\$Config\Deck.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return (Resolve-Path $path).Path
        }
    }

    $exe = Get-ChildItem -Path $ArtifactsDir -Filter "Deck.exe" -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($exe) {
        return $exe.FullName
    }

    throw "Deck.exe not found under $ArtifactsDir. Build may have failed."
}

function Invoke-CMake {
    param([string[]] $Arguments)
    Write-Host ">> cmake $($Arguments -join ' ')" -ForegroundColor Cyan
    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "cmake failed with exit code $LASTEXITCODE"
    }
}

$root = Get-RepoRoot
Set-Location $root

$iconPath = Join-Path $root "Assets\AppIcon.png"
if (-not (Test-Path $iconPath)) {
    throw "Missing $iconPath — add a 1024x1024 PNG app icon before building."
}

Assert-Command -Name "cmake" -InstallHint "Install CMake 3.22+ from https://cmake.org/download/ and add it to PATH."
Assert-Command -Name "git" -InstallHint "Install Git for Windows — required for FetchContent (JUCE)."

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    throw @"
Visual Studio 2022 was not detected.
Install "Visual Studio 2022" with workload "Desktop development with C++"
(https://visualstudio.microsoft.com/downloads/).
"@
}

$vsInstall = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) {
    throw "Visual Studio 2022 C++ toolchain not found. Install the Desktop development with C++ workload."
}

Write-Host "Using Visual Studio at: $vsInstall" -ForegroundColor DarkGray

$buildPath = Join-Path $root $BuildDir
if ($Clean -and (Test-Path $buildPath)) {
    Write-Host "Removing $buildPath" -ForegroundColor Yellow
    Remove-Item -LiteralPath $buildPath -Recurse -Force
}

$testsFlag = if ($Tests) { "ON" } else { "OFF" }

Invoke-CMake @(
    "-B", $buildPath,
    "-G", $Generator,
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DCASSETTE_BUILD_TESTS=$testsFlag",
    "-DCASSETTE_ENABLE_PI_TAPE=OFF"
)

$buildArgs = @("--build", $buildPath, "--config", $Config, "--target", "Deck")
if ($Jobs -gt 0) {
    $buildArgs += @("--parallel", "$Jobs")
}
Invoke-CMake $buildArgs

if ($Tests) {
    Invoke-CMake @("--build", $buildPath, "--config", $Config, "--target", "DeckTests")
    Invoke-CMake @("--test-dir", $buildPath, "--build-config", $Config, "--output-on-failure")
}

$artifactsDir = Join-Path $buildPath "Deck_artefacts\$Config"
$exePath = Find-DeckExecutable -ArtifactsDir $artifactsDir

Write-Host ""
Write-Host "Build complete:" -ForegroundColor Green
Write-Host "  $exePath"

if ($Package) {
    $version = Get-ProjectVersion -CMakeListsPath (Join-Path $root "CMakeLists.txt")
    $distDir = Join-Path $root "dist\Windows"
    $zipPath = Join-Path $root "dist\Deck-$version-Windows.zip"

    if (Test-Path $distDir) {
        Remove-Item -LiteralPath $distDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $distDir | Out-Null

    Copy-Item -LiteralPath $exePath -Destination $distDir

    $exeFolder = Split-Path -Parent $exePath
    Get-ChildItem -Path $exeFolder -Filter "*.dll" -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $distDir }

    if (Test-Path $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $distDir "*") -DestinationPath $zipPath -Force

    Write-Host "Package:" -ForegroundColor Green
    Write-Host "  Folder: $distDir"
    Write-Host "  Zip:    $zipPath"
}

Write-Host ""
Write-Host "Run: Start-Process '$exePath'"
