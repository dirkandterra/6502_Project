@echo off
setlocal

set "ARDUINO_TOOLS=%LOCALAPPDATA%\Arduino15\packages\arduino\tools"
set "AVR_GCC_DIR=%ARDUINO_TOOLS%\avr-gcc\7.3.0-atmel3.6.1-arduino7\bin"
set "GCC=%AVR_GCC_DIR%\avr-gcc.exe"
set "OBJCOPY=%AVR_GCC_DIR%\avr-objcopy.exe"
set "MCU=atmega328p"
set "F_CPU=16000000UL"

if not exist "%GCC%" (
    echo ERROR: avr-gcc not found: "%GCC%"
    exit /b 1
)
if not exist "%OBJCOPY%" (
    echo ERROR: avr-objcopy not found: "%OBJCOPY%"
    exit /b 1
)

if not exist "%~dp0build" mkdir "%~dp0build"

echo Building for %MCU% at %F_CPU%...
"%GCC%" -std=c11 -Wall -Wextra -Os -mmcu=%MCU% -DF_CPU=%F_CPU% "%~dp0main.c" -o "%~dp0build\24x1LCD.elf"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O ihex -R .eeprom "%~dp0build\24x1LCD.elf" "%~dp0build\24x1LCD.hex"
if errorlevel 1 exit /b 1

echo Build complete: "%~dp0build\24x1LCD.hex"
exit /b 0
