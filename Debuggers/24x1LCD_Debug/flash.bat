@echo off
setlocal

set "PORT=%~1"
if not defined PORT set "PORT=COM4"

set "AVRDUDE_DIR=%LOCALAPPDATA%\Arduino15\packages\arduino\tools\avrdude\6.3.0-arduino17"
set "AVRDUDE=%AVRDUDE_DIR%\bin\avrdude.exe"
set "CONF=%AVRDUDE_DIR%\etc\avrdude.conf"

if not exist "%AVRDUDE%" (
    echo ERROR: avrdude not found: "%AVRDUDE%"
    exit /b 1
)
if not exist "%CONF%" (
    echo ERROR: avrdude.conf not found: "%CONF%"
    exit /b 1
)

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

echo Uploading firmware to %PORT%...
"%AVRDUDE%" -C "%CONF%" -p atmega328p -c arduino -P "%PORT%" -b 57600 -D -U flash:w:"%~dp0build\24x1LCD.hex":i
if errorlevel 1 exit /b 1

echo Build and upload complete.
exit /b 0
