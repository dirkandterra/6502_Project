@echo off
setlocal

vasm6502_oldstyle.exe -dotdir -Fbin -o ..\EEProm_Util\a.out Blink_Rotate_LCD_Port.s
..\EEProm_Util\eeprog.exe -f ..\EEProm_Util\a.out -p COM4 -v

PAUSE
exit /b 0
