@echo off

set old_dir=%cd%

cd %~dp0

REM delete build directory
rd /s /q build

REM create build directory
mkdir build
if "%errorlevel%"=="1" goto end

REM go to build
cd build
if "%errorlevel%"=="1" goto end

REM cmake
set flagdebug="off"
if "%1%"=="debug" set flagdebug="on"
if "%2%"=="debug" set flagdebug="on"
if %flagdebug%=="on" (
	set flagdebug="-DCMAKE_BUILD_TYPE=Debug"
) else (
	set flagdebug="-DCMAKE_BUILD_TYPE=Release"
)

set flagtestnet="off"
if "%1%"=="testnet" set flagtestnet="on"
if "%2%"=="testnet" set flagtestnet="on"
if %flagtestnet%=="on" (
	set flagtestnet="-DTESTNET=on"
) else (
	set flagtestnet="-DTESTNET=off"
)

echo 'cmake .. -G "Ninja" %flagdebug% %flagtestnet%'
cmake .. -G "Ninja" %flagdebug% %flagtestnet%
if "%errorlevel%"=="1" goto end

REM make
ninja
if "%errorlevel%"=="1" goto end

REM install
mkdir bin
copy src\bigbang\bigbang.exe bin\
copy src\bigbang\*.dll bin\

echo bigbang.exe console > bin\bigbang-console.bat
echo bigbang.exe -daemon > bin\bigbang-server.bat

echo 'Installed to build\bin\'
echo
echo 'Usage:'
echo 'Run build\bin\bigbang.exe to launch bigbang'
echo 'Run build\bin\bigbang-console.bat to launch bigbang-console'
echo 'Run build\bin\bigbang-server.bat to launch bigbang server on background'
echo 'Run build\test\test_big.exe to launch test program.'

:end

cd %old_dir%
