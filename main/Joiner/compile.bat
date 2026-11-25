@echo off
setlocal enabledelayedexpansion

set "GENERATOR=Visual Studio 17 2022"
set "CONFIG=Release"

echo Configuring and building 32-bit (Win32)...
cmake -B build32 -G "%GENERATOR%" -A Win32
if errorlevel 1 exit /b 1

cmake --build build32 --config %CONFIG% --parallel
if errorlevel 1 exit /b 1

echo Configuring and building 64-bit (x64)...
cmake -B build64 -G "%GENERATOR%" -A x64
if errorlevel 1 exit /b 1

cmake --build build64 --config %CONFIG% --parallel
if errorlevel 1 exit /b 1

echo Packaging distribution...
python ..\..\util\copy_dist.py
if errorlevel 1 exit /b 1

echo Optimizing JSON manifest...
python ..\..\util\optimize_json.py dist
if errorlevel 1 exit /b 1

echo Done