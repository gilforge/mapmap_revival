# Build script for MapMap on Windows with MSVC
$ErrorActionPreference = "Stop"

# Setup paths
$VS_PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$QT_PATH = "C:\Qt\5.15.2\msvc2019_64\bin"
$GST_PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\bin"
# GStreamer development files location (for headers and libs)
$GST_DEV_PATH = "C:\gstreamer\1.0\msvc_x86_64\1.0\msvc_x86_64"

# Create build directory
$BUILD_DIR = Join-Path $PSScriptRoot "build"
if (-not (Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory -Path $BUILD_DIR | Out-Null
}

# Create a batch file to run in the VS environment
$batchContent = @"
@echo off
call "$VS_PATH"
set PATH=$QT_PATH;$GST_PATH;%PATH%
set GSTREAMER_1_0_ROOT_MSVC_X86_64=$GST_DEV_PATH
cd /d "$BUILD_DIR"
echo Running qmake...
"$QT_PATH\qmake.exe" "$PSScriptRoot\mapmap.pro" -spec win32-msvc CONFIG+=release
if errorlevel 1 (
    echo qmake failed!
    exit /b 1
)
echo Running nmake...
nmake
if errorlevel 1 (
    echo nmake failed!
    exit /b 1
)
echo Build successful!
"@

$tempBatch = Join-Path $env:TEMP "mapmap_build.bat"
$batchContent | Out-File -FilePath $tempBatch -Encoding ASCII

# Run the batch file
Write-Host "Starting build process..."
cmd /c $tempBatch
