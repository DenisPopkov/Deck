# Build Cassette Auto Master on Windows (Visual Studio 2022).
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot\..

if (-not (Test-Path "Assets\AppIcon.png")) {
    Write-Error "Missing Assets\AppIcon.png"
}

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_TESTS=OFF
cmake --build build --config Release -j 4

$exe = Get-ChildItem -Path "build\CassetteAutoMaster_artefacts\Release" -Filter "*.exe" -Recurse | Select-Object -First 1
Write-Host "Built:" $exe.FullName

$dist = "dist\Windows"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item $exe.FullName $dist\
Compress-Archive -Path "$dist\*" -DestinationPath "dist\CassetteAutoMaster-0.2.0-Windows.zip" -Force
Write-Host "Zip: dist\CassetteAutoMaster-0.2.0-Windows.zip"
