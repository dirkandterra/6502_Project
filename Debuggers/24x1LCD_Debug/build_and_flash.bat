@echo off
setlocal

REM Build and flash the ATmega328P Nano using the installed Arduino AVR tools.
REM Usage:
REM   build_and_flash.bat
REM   build_and_flash.bat COM6

set "PORT=%~1"
if not defined PORT set "PORT=COM4"

call "%~dp0flash.bat" "%PORT%"
if errorlevel 1 (
    echo Build or upload failed.
    pause
    exit /b 1
)

echo.
echo Build and upload complete.
pause
exit /b 0
