@echo off
REM Build script for MapMap on Windows with MSVC

REM Setup MSVC environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

REM Add Qt to PATH
set PATH=C:\Qt\5.15.2\msvc2019_64\bin;%PATH%

REM Add GStreamer to PATH (for runtime)
set PATH=C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;%PATH%
set GST_PLUGIN_PATH=C:\Program Files\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0

REM Create build directory
if not exist build mkdir build
cd build

REM Run qmake
echo Running qmake...
qmake ..\mapmap.pro -spec win32-msvc CONFIG+=release

REM Build
echo Building...
nmake

echo.
echo Build complete!
pause
