echo "Not support source installation in WIN32"
REM @echo off

REM set old_dir=%cd%

REM cd %~dp0

REM REM delete build directory
REM rd /s /q build
REM del /f /q bigbang.exe

REM REM create build directory
REM mkdir build
REM if "%errorlevel%"=="1" goto :end

REM REM go to build
REM cd build
REM if "%errorlevel%"=="1" goto :end

REM REM cmake
REM cmake .. -G "MinGW Makefiles"
REM if "%errorlevel%"=="1" goto :end

REM REM make
REM mingw32-make
REM if "%errorlevel%"=="1" goto :end

REM REM install
REM copy src\bigbang.exe ..\

REM :end
REM cd %old_dir%
