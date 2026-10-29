@echo off
REM FileTgie Windows build (MinGW gcc)
setlocal
cd /d %~dp0
gcc -O2 -Wall -Wextra -o FileTgie.exe src\main.c src\commands.c src\port.c src\util.c src\i18n.c src\update.c -lwinhttp -lshell32
if errorlevel 1 (
  echo BUILD FAILED
  exit /b 1
)
gcc -O2 -Wall -Wextra -o FileTgieUpdater.exe src\updater_main.c -lwinhttp -lshell32 -luser32
if errorlevel 1 (
  echo BUILD FAILED: FileTgieUpdater.exe
  exit /b 1
)
echo BUILD OK: FileTgie.exe and FileTgieUpdater.exe
