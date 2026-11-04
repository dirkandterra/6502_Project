@echo off
setlocal

vasm6502_oldstyle.exe -dotdir -Fbin -o ..\EEProm_Util\lcd.out LCD_Working.s
..\EEProm_Util\eeprog.exe -f ..\EEProm_Util\lcd.out -p COM6 -v

PAUSE
exit /b 0
