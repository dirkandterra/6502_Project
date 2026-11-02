@echo off
setlocal

REM Windows batch uploader for ATmega328P Nano bare-metal firmware
REM Usage:
REM   flash_nano.bat COM3
REM   flash_nano.bat COM10
REM   flash_nano.bat
REM If no port is supplied, it tries the common Nano port names.

set "HEX_FILE=%~dp0build\pcf8575_nano.hex"
set "PORT=%~1"

if "%PORT%"=="" (
    for %%P in (COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10 COM11 COM12) do (
        if exist "\\.\%%P" (
            set "PORT=%%P"
            goto :found_port
        )
    )
    echo No serial port detected. Pass a port explicitly, e.g.:
    echo   flash_nano.bat COM3
    exit /b 1
)

:found_port
if not exist "%HEX_FILE%" (
    echo Missing firmware image: %HEX_FILE%
    echo Build it first with:
    echo   make
    exit /b 1
)

echo Flashing %HEX_FILE% to %PORT%

avrdude -C avrdude.conf -p atmega328p -c arduino -P %PORT% -b 57600 -U flash:w:"%HEX_FILE%":i

if errorlevel 1 (
    echo.
    echo Upload failed.
    echo Make sure the Nano is connected and the port is correct.
    exit /b 1
)

echo.
echo Upload successful.
exit /b 0
