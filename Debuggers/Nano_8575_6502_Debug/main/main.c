#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define F_CPU 16000000UL
#define UART_BAUD_RATE 19200u
#define PCF8575_ADDRESS 0x20u
#define PCF8575_TWI_CLOCK_HZ 100000u

static uint16_t g_port_value[4] = {0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu};
static uint8_t g_display_hex[6] = {0, 0, 0, 0, 0, 0};
static volatile uint16_t g_pulse_ms = 0;
static volatile uint16_t g_pulse_interval_ms = 500u;
static volatile bool g_pulses_paused = false;
static volatile uint8_t g_status_pending = 0;
static void print_status(void);

static const uint8_t g_hex_to_7Seg[16] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90, 
                           0x88,0x83,0xC6,0xA1,0x86,0x8E}; // Hex to 7-segment encoding


static void setupPulseTimer(void)
{
    DDRD |= _BV(DDD3);
    PORTD |= _BV(PORTD3);

    // Timer1 CTC interrupt every 1 ms at 16 MHz, prescaler 64
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = 249;
    TIFR1 = _BV(OCF1A);
    TIMSK1 |= _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
    sei();
}

ISR(TIMER1_COMPA_vect)
{
    if (!g_pulses_paused && ++g_pulse_ms >= g_pulse_interval_ms) {
        g_pulse_ms = 0u;
        PORTD ^= _BV(PORTD3);
        if (g_status_pending < 0xFFu) {
            g_status_pending++;
        }
    }
}

static void toggle_pulses_paused(void)
{
    uint8_t saved_sreg = SREG;
    cli();

    bool d3_was_high = (PORTD & _BV(PORTD3)) != 0u;
    g_pulses_paused = !g_pulses_paused;
    g_pulse_ms = 0u;
    if (g_pulses_paused) {
        PORTD &= ~_BV(PORTD3);
    } else {
        PORTD |= _BV(PORTD3);
    }

    if (d3_was_high != ((PORTD & _BV(PORTD3)) != 0u) && g_status_pending < 0xFFu) {
        g_status_pending++;
    }
    SREG = saved_sreg;
}

static void set_pulse_interval(uint16_t interval_ms)
{
    uint8_t saved_sreg = SREG;
    cli();
    g_pulse_interval_ms = interval_ms;
    g_pulse_ms = 0u;
    SREG = saved_sreg;
}

static int uart_putchar(char c, FILE *stream)
{
    (void)stream;
    while (!(UCSR0A & _BV(UDRE0))) {
    }
    UDR0 = (uint8_t)c;
    return 0;
}

static int uart_getchar(FILE *stream)
{
    (void)stream;
    while (!(UCSR0A & _BV(RXC0))) {
    }
    return UDR0;
}

static FILE uart_stream = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);

static void uart_init(void)
{
    UBRR0 = (F_CPU / 16UL / UART_BAUD_RATE) - 1UL;
    UCSR0A = 0;
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
    stdout = &uart_stream;
    stdin = &uart_stream;
}

static bool twi_wait_for_status(uint8_t expected_status)
{
    while (!(TWCR & _BV(TWINT))) {
    }
    return (TWSR & 0xF8u) == expected_status;
}

static bool twi_start(void)
{
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    return twi_wait_for_status(TW_START);
}

static bool twi_write_byte(uint8_t data)
{
    TWDR = data;
    TWCR = _BV(TWINT) | _BV(TWEN);
    return twi_wait_for_status(TW_MT_DATA_ACK);
}

static bool twi_read_byte(uint8_t *value, bool ack)
{
    TWCR = _BV(TWINT) | _BV(TWEN) | (ack ? _BV(TWEA) : 0u);
    if (!twi_wait_for_status(ack ? TW_MR_DATA_ACK : TW_MR_DATA_NACK)) {
        return false;
    }
    *value = TWDR;
    return true;
}

static void twi_stop(void)
{
    TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
}

static void twi_init(void)
{
    DDRC &= ~(_BV(PC4) | _BV(PC5));
    PORTC |= _BV(PC4) | _BV(PC5);

    TWBR = ((F_CPU / PCF8575_TWI_CLOCK_HZ) - 16u) / 2u;
    TWSR = 0;
    TWCR = _BV(TWEN);
}

static bool pcf8575_read_device(uint8_t address, uint16_t *value)
{
    uint8_t low = 0;
    uint8_t high = 0;

    if (!twi_start()) {
        twi_stop();
        return false;
    }

    TWDR = (address << 1) | 0x01u;
    TWCR = _BV(TWINT) | _BV(TWEN);
    if (!twi_wait_for_status(TW_MR_SLA_ACK)) {
        twi_stop();
        return false;
    }

    if (!twi_read_byte(&low, true)) {
        twi_stop();
        return false;
    }

    if (!twi_read_byte(&high, false)) {
        twi_stop();
        return false;
    }

    twi_stop();
    *value = ((uint16_t)high << 8) | (uint16_t)low;
    return true;
}

static bool pcf8575_write_device(uint8_t address, uint16_t value)
{
    uint8_t low = (uint8_t)(value & 0xFFu);
    uint8_t high = (uint8_t)((value >> 8) & 0xFFu);

    if (!twi_start()) {
        twi_stop();
        return false;
    }

    TWDR = (address << 1) | 0x00u;
    TWCR = _BV(TWINT) | _BV(TWEN);
    if (!twi_wait_for_status(TW_MT_SLA_ACK)) {
        twi_stop();
        return false;
    }

    if (!twi_write_byte(low)) {
        twi_stop();
        return false;
    }

    if (!twi_write_byte(high)) {
        twi_stop();
        return false;
    }

    twi_stop();
    return true;
}

static void clear_digit_selects(void)
{
    PORTD &= ~(_BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7));
    PORTB &= ~(_BV(PB0) | _BV(PB1));
}

static void select_digit(uint8_t digit)
{
    clear_digit_selects();

    switch (digit) {
        case 5u:
            PORTD |= _BV(PD4);
            break;
        case 4u:
            PORTD |= _BV(PD5);
            break;
        case 3u:
            PORTD |= _BV(PD6);
            break;
        case 2u:
            PORTD |= _BV(PD7);
            break;
        case 1u:
            PORTB |= _BV(PB0);
            break;
        case 0u:
            PORTB |= _BV(PB1);
            break;
        default:
            break;
    }
}

static void refresh_display(uint8_t digit)
{
    clear_digit_selects();
    g_port_value[0] = 0xFF00+(uint16_t)g_hex_to_7Seg[g_display_hex[digit] & 0x0Fu];
    (void)pcf8575_write_device(PCF8575_ADDRESS, g_port_value[0]);
    select_digit(digit);
    _delay_ms(2);
    //clear_digit_selects();
}

static void set_ascii(uint8_t ascii_value)
{
    uint8_t low_nibble = ascii_value & 0x0Fu;
    uint8_t high_nibble = (ascii_value >> 4) & 0x0Fu;

    g_display_hex[2] = (uint8_t)((low_nibble) & 0x0Fu);
    g_display_hex[3] = (uint8_t)((high_nibble) & 0x0Fu);
}

static void set_bit0(bool state)
{
    if (state) {
        g_port_value[0] |= 0x0001u;
    } else {
        g_port_value[0] &= ~0x0001u;
    }

    if (!pcf8575_write_device(PCF8575_ADDRESS, g_port_value[0])) {
        printf("Write failed.\r\n");
    }
}

static void print_status(void)
{
    printf("D3 = %s\r\n", (PORTD & _BV(PORTD3)) ? "HIGH" : "LOW");

    uint16_t port_state = g_port_value[0];
    if (pcf8575_read_device(PCF8575_ADDRESS, &port_state)) {
        g_port_value[0] = port_state;
        g_display_hex[0] = (uint8_t)((g_port_value[0] >>8) & 0x0Fu);
        g_display_hex[1] = (uint8_t)((g_port_value[0] >> 12) & 0x0Fu);
        printf("P15..P0 = 0x%04X\r\n", g_port_value[0]);
    }
    if (pcf8575_read_device(PCF8575_ADDRESS+3, &g_port_value[1])) {
        g_display_hex[2] = (uint8_t)((g_port_value[1]) & 0x0Fu);
        g_display_hex[3] = (uint8_t)((g_port_value[1] >> 4) & 0x0Fu);
        g_display_hex[4] = (uint8_t)((g_port_value[1] >>8) & 0x0Fu);
        g_display_hex[5] = (uint8_t)((g_port_value[1] >> 12) & 0x0Fu);
        printf("P31..P16 = 0x%04X\r\n", g_port_value[1]);
    }
}

int main(void)
{
    char cmd[8] = {0};
    uint8_t cmd_len = 0;
    uint8_t display_digit = 0;

    uart_init();
    twi_init();
    sei();

    DDRD |= _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7);
    DDRB |= _BV(PB0) | _BV(PB1);
    clear_digit_selects();
    setupPulseTimer();
    printf("PCF8575 ATmega328P Nano port ready.\r\n");
    printf("Type '1', '0', 'r', 'C', 'F', or 'S' and press Enter.\r\n");
    printf("'1' and '0' set P0, while 'r' reads the port state.\r\n");
    printf("'C' toggles D3 pulses; 'F' selects 100 ms and 'S' selects 500 ms per phase.\r\n");

    if (!pcf8575_read_device(PCF8575_ADDRESS, &g_port_value[0])) {
        printf("PCF8575 not detected at 0x%02X. Check wiring and pull-ups.\r\n", PCF8575_ADDRESS);
    } else {
        printf("PCF8575-0 detected. Current state: 0x%04X\r\n", g_port_value[0]);
    }

    if (!pcf8575_read_device(PCF8575_ADDRESS+3, &g_port_value[1])) {
        printf("PCF8575 not detected at 0x%02X. Check wiring and pull-ups.\r\n", PCF8575_ADDRESS+3);
    } else {
        printf("PCF8575-3 detected. Current state: 0x%04X\r\n", g_port_value[1]);
    }

    while (1) {
        refresh_display(display_digit);
        display_digit = (uint8_t)((display_digit + 1u) % 6u);

        uint8_t saved_sreg = SREG;
        cli();
        bool status_pending = g_status_pending > 0u;
        if (status_pending) {
            g_status_pending--;
        }
        SREG = saved_sreg;
        if (status_pending) {
            print_status();
        }

        if (!(UCSR0A & _BV(RXC0))) {
            continue;
        }

        int c = UDR0;

        if (c == '\r' || c == '\n') {
            if (cmd_len == 0) {
                continue;
            }

            cmd[cmd_len] = '\0';

            if (strcmp(cmd, "1") == 0) {
                set_bit0(true);
                printf("P0 is now 1\r\n");
            } else if (strcmp(cmd, "0") == 0) {
                set_bit0(false);
                printf("P0 is now 0\r\n");
            } else if (strcmp(cmd, "r") == 0) {
                print_status();
            } else if (strcmp(cmd, "C") == 0 || strcmp(cmd, "c") == 0) {
                toggle_pulses_paused();
                printf("D3 pulses %s.\r\n", g_pulses_paused ? "paused" : "resumed");
            } else if (strcmp(cmd, "F") == 0 || strcmp(cmd, "f") == 0) {
                set_pulse_interval(100u);
                printf("D3 pulse interval set to 100 ms per phase.\r\n");
            } else if (strcmp(cmd, "S") == 0 || strcmp(cmd, "s") == 0) {
                set_pulse_interval(500u);
                printf("D3 pulse interval set to 500 ms per phase.\r\n");
            } else {
                printf("Invalid input: '%s'. Only 0, 1, r, C, F, or S is accepted.\r\n", cmd);
            }

            cmd_len = 0;
            memset(cmd, 0, sizeof(cmd));
            continue;
        }

        if (cmd_len < (sizeof(cmd) - 1u)) {
            cmd[cmd_len++] = (char)c;
        }

        if (c != '1' && c != '0' && c != 'r' && c != 'R' &&
            c != 'C' && c != 'c' && c != 'F' && c != 'f' && c != 'S' && c != 's') {
            set_ascii((uint8_t)c);
        }
    }

    return 0;
}
