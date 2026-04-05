@echo off
REM === MapMap Qt6 Launcher ===
REM Sets up GStreamer and Qt paths then launches MapMap

set PATH=%~dp0build_qt6;C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;%PATH%
set GST_PLUGIN_PATH=%~dp0build_qt6\lib\gstreamer-1.0

cd /d "%~dp0build_qt6"
echo Starting MapMap...
MapMap.exe
if errorlevel 1 (
    echo.
    echo MapMap exited with error code %errorlevel%
    pause
)
