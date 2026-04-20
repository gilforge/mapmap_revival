@echo off
REM ============================================================
REM  MapMap Revival - Portable Package Builder
REM  Creates a fully self-contained folder that runs on any
REM  Windows 10/11 PC without admin rights or installations.
REM ============================================================

setlocal enabledelayedexpansion

set "SRC=%~dp0build_qt6"
set "DST=%~dp0MapMap_Portable"

echo ============================================================
echo  MapMap Revival - Portable Packager
echo ============================================================
echo.

REM Check that build exists
if not exist "%SRC%\MapMap.exe" (
    echo ERROR: MapMap.exe not found in build_qt6\
    echo        Run the build first with build_qt6.bat
    pause
    exit /b 1
)

REM Clean previous package
if exist "%DST%" (
    echo Cleaning previous package...
    rmdir /s /q "%DST%"
)

echo Creating portable package in MapMap_Portable\...
mkdir "%DST%"

REM === 1. Copy executable ===
echo [1/6] Copying MapMap.exe...
copy "%SRC%\MapMap.exe" "%DST%\" >nul

REM === 2. Copy all DLLs (Qt6 + GStreamer + codec libs) ===
echo [2/6] Copying runtime DLLs...
copy "%SRC%\*.dll" "%DST%\" >nul

REM === 3. Copy Qt plugins ===
echo [3/6] Copying Qt plugins...
for %%D in (platforms imageformats iconengines styles multimedia tls generic networkinformation) do (
    if exist "%SRC%\%%D" (
        xcopy "%SRC%\%%D" "%DST%\%%D\" /E /Q /Y >nul
    )
)

REM === 4. Copy GStreamer plugins ===
echo [4/6] Copying GStreamer plugins...
if exist "%SRC%\lib\gstreamer-1.0" (
    xcopy "%SRC%\lib\gstreamer-1.0" "%DST%\lib\gstreamer-1.0\" /E /Q /Y >nul
)

REM === 5. Copy translations ===
if exist "%SRC%\translations" (
    xcopy "%SRC%\translations" "%DST%\translations\" /E /Q /Y >nul
)

REM === 6. Copy MSVC Runtime (so no install needed) ===
echo [5/6] Copying MSVC runtime...
set "SYSDIR=C:\Windows\System32"
for %%F in (msvcp140.dll msvcp140_1.dll msvcp140_2.dll vcruntime140.dll vcruntime140_1.dll ucrtbase.dll concrt140.dll) do (
    if exist "%SYSDIR%\%%F" (
        copy "%SYSDIR%\%%F" "%DST%\" >nul
    )
)

REM === 7. Create launcher ===
echo [6/6] Creating launcher...
(
echo @echo off
echo REM === MapMap Revival - Portable Launcher ===
echo REM No installation required. Just double-click this file.
echo.
echo set "MAPMAP_DIR=%%~dp0"
echo set "PATH=%%MAPMAP_DIR%%;%%PATH%%"
echo set "GST_PLUGIN_PATH=%%MAPMAP_DIR%%lib\gstreamer-1.0"
echo set "GST_PLUGIN_SYSTEM_PATH=%%MAPMAP_DIR%%lib\gstreamer-1.0"
echo set "GST_REGISTRY=%%MAPMAP_DIR%%gst_registry.bin"
echo.
echo cd /d "%%MAPMAP_DIR%%"
echo start "" "%%MAPMAP_DIR%%MapMap.exe" %%*
) > "%DST%\MapMap.bat"

REM === Count what we packaged ===
set /a count=0
for %%F in ("%DST%\*.*") do set /a count+=1
for /r "%DST%" %%F in (*.*) do set /a count+=1

echo.
echo ============================================================
echo  Package created: MapMap_Portable\
echo  Contents: ~!count! files
echo.
echo  To distribute: copy the MapMap_Portable folder to a USB
echo  stick. Students double-click MapMap.bat to run.
echo  No admin rights needed. No installation needed.
echo ============================================================
echo.
pause
