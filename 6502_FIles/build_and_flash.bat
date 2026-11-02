@echo off
setlocal

vasm6502_oldstyle.exe -dotdir -Fbin -o Blink_Rotate_LCD_Port.out Blink_Rotate_LCD_Port.s
..\EEProm_Util\eeprog.exe -f Blink_Rotate_LCD_Port.out -p COM4 -v

PAUSE
exit /b 0
