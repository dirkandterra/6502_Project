# 24x1LCD AVR firmware

This is a bare-metal AVR C project for an ATmega328P-based Uno/Nano at 16 MHz.
It uses AVR registers and avr-libc delays; it does not use the Arduino core or
Arduino libraries.

## Pin mapping

The LCD wiring keeps the original Arduino digital pin assignments:

| LCD signal | Arduino pin | ATmega328P pin |
| --- | --- | --- |
| Reset | D2 | PD2 |
| RS | D3 | PD3 |
| E | D4 | PD4 |
| DB0-DB2 | D5-D7 | PD5-PD7 |
| DB3-DB7 | D8-D12 | PB0-PB4 |

Connect the R/W status input to Nano A0 (PC0). Do not use PC6 for this input:
on the ATmega328P it is the RESET pin unless the reset function is disabled by
fuse configuration.

## 74HC165 input chain

Three cascaded 74HC165 shift registers use Nano A2 as the active-low parallel
load/latch output, A3 as the shift clock output, and A4 as the serial-data input
(Q7). The firmware reads 24 bits after latching: the first 16 are shifted into
the global `address` variable and the following 8 into `data`. Bits are
assembled MSB-first in the order they appear on Q7.

## Serial console output

Open the Nano's USB serial console at 9600 baud. A lowercase `c` toggles A5
(PC5), and a lowercase `l` toggles A4 (PC4). UART receive uses the Nano's
hardware serial RX pin (D0/RX).

## Build

The build script uses the Arduino AVR GCC package installed under
`%LOCALAPPDATA%\Arduino15\packages\arduino\tools`, including avr-libc. Run
`build.bat` from Explorer or a command prompt. The firmware is written to
`build\24x1LCD.hex`.

The target is `atmega328p` at `16000000UL`.

## Flash

The flash scripts use the Arduino AVR avrdude package and configuration file
under `%LOCALAPPDATA%\Arduino15\packages\arduino\tools`. Connect the classic
Nano and run:

```bat
build_and_flash.bat COM4
```

The serial port defaults to `COM4`; pass a different port if needed. The script
builds and then programs the ATmega328P using the Nano bootloader at 57600 baud.
Alternatively, run `build.bat` to build only or `flash.bat [COM port]` to build
and flash.
