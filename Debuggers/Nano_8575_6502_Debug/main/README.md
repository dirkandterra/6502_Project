# ATmega328P Nano + PCF8575 port

This directory contains a bare-metal C port of the ESP32 PCF8575 toggle example for an ATmega328P Nano board without using the Arduino IDE.

## Hardware

- ATmega328P Nano
- PCF8575 I2C GPIO expander at address 0x20
- SDA -> A4 on the Nano
- SCL -> A5 on the Nano
- GND -> common ground
- 5V or 3.3V depending on the PCF8575 board and pull-ups
- Add pull-up resistors on SDA/SCL if your board does not include them

## Build without the Arduino IDE

Install the AVR cross-toolchain and avrdude, then build from the command line:

```bash
cd avr_nano
make
```

Flash to the board:

```bash
make flash PORT=COM3
```

On Linux/macOS:

```bash
make flash PORT=/dev/ttyUSB0
```

## Behavior

The firmware exposes the same core function as the ESP32 project:

- `1` sets P0 high
- `0` sets P0 low
- `r` reads the current 16-bit port state
- D3 pulses high for 500 ms and low for 500 ms by default
- `C` toggles the D3 pulse output between running and paused (lowercase `c` also works)
- `F` selects 100 ms per high/low phase; `S` selects 500 ms per phase (lowercase also works)

## Notes

- This is intentionally a minimal port based on the original ESP32 logic.
- It uses the ATmega328P's hardware TWI peripheral rather than the Arduino core libraries.
- The output toggled is the lowest bit on the PCF8575 at address 0x20.
