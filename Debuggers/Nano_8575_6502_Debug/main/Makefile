MCU = atmega328p
TARGET = pcf8575_nano
BUILD_DIR = build
SRC = main.c
CFLAGS = -std=c11 -Wall -Wextra -Os -mmcu=$(MCU) -DF_CPU=16000000UL
LDFLAGS = -mmcu=$(MCU)
CPPFLAGS =

all: $(BUILD_DIR)/$(TARGET).hex

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET).elf: $(SRC) | $(BUILD_DIR)
	avr-gcc $(CFLAGS) $(CPPFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	avr-objcopy -O ihex -R .eeprom $< $@
	avr-size --format=avr --mcu=$(MCU) $<

flash: $(BUILD_DIR)/$(TARGET).hex
	@echo "Usage: make flash PORT=COM3"
	@echo "       or: make flash PORT=/dev/ttyUSB0"
	@avrdude -C avrdude.conf -p $(MCU) -c arduino -P $(PORT) -b 57600 -U flash:w:$(BUILD_DIR)/$(TARGET).hex:i

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all flash clean
