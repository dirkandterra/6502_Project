//To assemble .s file
vasm6502_oldstyle.exe -Fbin -dotdir -o a.out <PROGRAM_NAME>.s

//Program with EEPROM_Util
eeprog.exe -f a.out -p COM4 -v