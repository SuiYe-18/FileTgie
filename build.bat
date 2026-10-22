@echo off
REM FileTgie Windows build (MinGW gcc)
setlocal
cd /d %~dp0
gcc -O2 -Wall -o FileTgie.exe src\main.c src\commands.c src\port.c src\util.c src\i18n.c
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
echo BUILD OK: FileTgie.exe
