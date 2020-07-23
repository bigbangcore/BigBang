echo "Not support source installation in WIN32"
@echo off

set old_dir=%cd%

cd %~dp0

REM delete build directory
rd /s /q build

REM create build directory
mkdir build
if "%errorlevel%"=="1" goto :end

REM go to build
cd build
if "%errorlevel%"=="1" goto :end

REM cmake
cmake .. -G "Ninja"
if "%errorlevel%"=="1" goto :end

REM make
ninja
if "%errorlevel%"=="1" goto :end

REM install
mkdir bin
copy src\bigbang\bigbang.exe bin\
copy src\bigbang\*.dll bin\

echo bigbang.exe console > bin\bigbang-console.bat
echo bigbang.exe -daemon > bin\bigbang-server.bat

:end

cd %old_dir%
