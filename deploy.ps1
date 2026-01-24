# Deploy script for MapMap on Windows
$ErrorActionPreference = "Stop"

$VS_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$QT_PATH = "C:\Qt\5.15.2\msvc2019_64\bin"
$RELEASE_DIR = "z:\IA\Mapmapmap\build\release"

# Create batch to run windeployqt
$batchContent = @"
@echo off
call "$VS_PATH"
set PATH=$QT_PATH;%PATH%
"$QT_PATH\windeployqt.exe" --release "$RELEASE_DIR\MapMap.exe"
"@

$tempBatch = Join-Path $env:TEMP "mapmap_deploy.bat"
$batchContent | Out-File -FilePath $tempBatch -Encoding ASCII

Write-Host "Deploying Qt DLLs..."
cmd /c $tempBatch

Write-Host "Done!"
