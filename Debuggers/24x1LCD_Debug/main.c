#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define LCD_RESET_BIT PD2
#define LCD_RS_BIT    PD3
#define LCD_E_BIT     PD4
#define LCD_PORTD_DATA_MASK 0xE0
#define LCD_PORTB_DATA_MASK 0x1F

#define INPUT_DATA_BIT  PC2
#define INPUT_CLOCK_BIT PC3
#define INPUT_LATCH_BIT PC4
#define OUTPUT_TOGGLE_BIT PC5
#define OUTPUT_TOGGLE2_BIT INPUT_LATCH_BIT
#define R_W_INPUT PC0
#define READY_INPUT PC1



uint16_t address;
uint8_t data;
uint8_t read_write_input;

static void console_init(void)
{
    const uint16_t baud_divisor = (F_CPU / (16UL * 9600UL)) - 1;
    UBRR0H = (uint8_t)(baud_divisor >> 8);
    UBRR0L = (uint8_t)baud_divisor;
    UCSR0A = 0;
    UCSR0B = _BV(RXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

static void poll_console(void)
{
    if (UCSR0A & _BV(RXC0)) {
        uint8_t received = UDR0;
        if (received == 'c') {
            PORTC ^= _BV(OUTPUT_TOGGLE_BIT);
        } else if (received == 'l') {
            PORTC ^= _BV(OUTPUT_TOGGLE2_BIT);
        }
    }
}

static void write8_bits(uint8_t payload)
{
    PORTD = (PORTD & (uint8_t)~LCD_PORTD_DATA_MASK)
          | (uint8_t)((payload & 0x07) << 5);
    PORTB = (PORTB & (uint8_t)~LCD_PORTB_DATA_MASK)
          | (uint8_t)(payload >> 3);

    PORTD |= _BV(LCD_E_BIT);
    _delay_us(10);
    PORTD &= (uint8_t)~_BV(LCD_E_BIT);
    _delay_us(500);
}

static void write_command(uint8_t command)
{
    PORTD &= (uint8_t)~_BV(LCD_RS_BIT);
    write8_bits(command);
}

static void write_data(uint8_t data)
{
    PORTD |= _BV(LCD_RS_BIT);
    write8_bits(data);
}

static void print_lcd(const char *message)
{
    while (*message != '\0') {
        write_data((uint8_t)*message++);
    }
}

static void go_to_addr(uint8_t address)
{
    if (address > 15) {
        address += 4;
    }
    write_command((uint8_t)(0xE0 | address));
}

static void input_init(void)
{
    DDRC |= _BV(INPUT_LATCH_BIT) | _BV(INPUT_CLOCK_BIT)
          | _BV(OUTPUT_TOGGLE_BIT) | _BV(OUTPUT_TOGGLE2_BIT);
    DDRC &= (uint8_t)~(_BV(INPUT_DATA_BIT) | _BV(R_W_INPUT));
    PORTC |= _BV(INPUT_LATCH_BIT);
    PORTC &= (uint8_t)~(_BV(INPUT_CLOCK_BIT) | _BV(OUTPUT_TOGGLE_BIT)
                        | _BV(OUTPUT_TOGGLE2_BIT));
    PORTC &= (uint8_t)~(_BV(INPUT_DATA_BIT) | _BV(R_W_INPUT));
}

static void read_inputs(void)
{
    uint16_t address_value = 0;
    uint8_t data_value = 0;

    //Toggle the latch to read the inputs
    PORTC &= (uint8_t)~_BV(INPUT_LATCH_BIT);
    _delay_us(5);
    PORTC |= _BV(INPUT_LATCH_BIT);

    // Read 24 bits of data from the input shift register
    // The first 16 bits are the address, and the last 8 bits are the data
    // Address bytes are read in lsbit first, but the msbyte is read in first
    for (uint8_t bit = 0; bit < 24; bit++) {
        uint8_t input_bit = (PINC & _BV(INPUT_DATA_BIT)) ? 1 : 0;

        if (bit < 16) {
            address_value = address_value>>1 | (0x8000u * input_bit);
        } else {
            data_value = (uint8_t)((data_value << 1) | input_bit);
        }

        //Clock the shift register to read the next bit
        PORTC |= _BV(INPUT_CLOCK_BIT);
        _delay_us(1);
        PORTC &= (uint8_t)~_BV(INPUT_CLOCK_BIT);
        _delay_us(1);
    }
    data = data_value;
    address_value = ((address_value << 8)| (address_value >> 8));   //bytes need swapped
    address = address_value;
    
    read_write_input = (PINC & _BV(R_W_INPUT)) ? 1 : 0;
}

static void print_hex16(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    write_data(digits[(value >> 12) & 0x0F]);
    write_data(digits[(value >> 8) & 0x0F]);
    write_data(digits[(value >> 4) & 0x0F]);
    write_data(digits[value & 0x0F]);
}

static void print_hex8(uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    write_data(digits[(value >> 4) & 0x0F]);
    write_data(digits[value & 0x0F]);
}

static void lcd_init(void)
{
    DDRD |= _BV(LCD_RESET_BIT) | _BV(LCD_RS_BIT) | _BV(LCD_E_BIT)
          | _BV(PD5) | _BV(PD6) | _BV(PD7);
    DDRB |= LCD_PORTB_DATA_MASK;

    PORTD &= (uint8_t)~_BV(LCD_E_BIT);

    /* Preserve the reset signal sequence from the original sketch. */
    PORTD |= _BV(LCD_RESET_BIT);
    _delay_ms(10);
    _delay_ms(20);
    PORTD |= _BV(LCD_RESET_BIT);
    _delay_ms(20);

    _delay_ms(100);
    _delay_ms(5);
    write_command(0x1C);
    _delay_ms(200);
    write_command(0x14);
    _delay_ms(5);
    write_command(0x28);
    _delay_ms(5);
    write_command(0x4F);
    _delay_ms(5);
    write_command(0x08);
    _delay_ms(5);
    write_command(0x06);
    _delay_ms(5);
    write_command(0x01);
    _delay_ms(20);

    print_lcd("Add:0x     Data:0x      ");
}

int main(void)
{
    int printDelay = 2;
    input_init();
    console_init();
    lcd_init();

    for (;;) {
        poll_console();
        read_inputs();
        // Update the LCD display every 200ms
        if(printDelay-- <= 0){
            printDelay = 2;
            go_to_addr(6);
            print_hex16(address);

            go_to_addr(18);
            print_hex8(data);

            go_to_addr(21);
            write_data(PINC & (1 << OUTPUT_TOGGLE_BIT) ? '-' : '_');

            go_to_addr(23);
            write_data(read_write_input ? 'R' : 'W');
        }

        _delay_ms(100);
    }
}
