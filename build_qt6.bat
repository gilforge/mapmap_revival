@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set PATH=C:\Qt\6.8.3\msvc2022_64\bin;%PATH%
cd /d Z:\IA\mapmap_revival_new
if not exist build_qt6 mkdir build_qt6
cd build_qt6
echo === CMake Configure ===
qt-cmake.bat .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release 2>&1
echo === Build ===
nmake 2>&1
echo === Done ===
