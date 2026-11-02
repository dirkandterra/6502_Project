@echo off
setlocal

REM Build + flash script for the ATmega328P Nano using the Arduino AVR toolchain
REM Usage:
REM   build_and_flash.bat
REM   build_and_flash.bat COM6

set "PORT=%~1"
if "%PORT%"=="" set "PORT=COM4"

set "GCC=C:\Users\dirkr\AppData\Local\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino7\bin\avr-gcc.exe"
set "OBJCOPY=C:\Users\dirkr\AppData\Local\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino7\bin\avr-objcopy.exe"
set "AVRDUDE=C:\Users\dirkr\AppData\Local\Arduino15\packages\arduino\tools\avrdude\6.3.0-arduino17\bin\avrdude.exe"
set "CONF=C:\Users\dirkr\AppData\Local\Arduino15\packages\arduino\tools\avrdude\6.3.0-arduino17\etc\avrdude.conf"

if not exist "%GCC%" (
    echo avr-gcc not found at the expected Arduino path.
    PAUSE
    exit /b 1
)
if not exist "%OBJCOPY%" (
    echo avr-objcopy not found at the expected Arduino path.
    PAUSE
    exit /b 1
)
if not exist "%AVRDUDE%" (
    echo avrdude not found at the expected Arduino path.
    PAUSE
    exit /b 1
)
if not exist "%CONF%" (
    echo avrdude.conf not found at the expected Arduino path.
    PAUSE
    exit /b 1
)

if not exist "%~dp0build" mkdir "%~dp0build"

echo Building firmware...
"%GCC%" -std=c11 -Wall -Wextra -Os -mmcu=atmega328p -DF_CPU=16000000UL "%~dp0main.c" -o "%~dp0build\pcf8575_nano.elf"
if errorlevel 1 (
    echo Build failed.
    PAUSE
    exit /b 1
)

"%OBJCOPY%" -O ihex -R .eeprom "%~dp0build\pcf8575_nano.elf" "%~dp0build\pcf8575_nano.hex"
if errorlevel 1 (
    echo Object copy failed.
    PAUSE
    exit /b 1
)

echo Uploading to %PORT%...
"%AVRDUDE%" -C "%CONF%" -p atmega328p -c arduino -P %PORT% -b 57600 -U flash:w:"%~dp0build\pcf8575_nano.hex":i
if errorlevel 1 (
    echo Upload failed.
    echo Confirm the Nano is connected and the COM port is correct.
    PAUSE
    exit /b 1
)

echo.
echo Build and upload complete.
PAUSE
exit /b 0
