/*
    Output test using the PIO to generate the pulses
    Sends a program stored in flash

    (C) 2024, Daniel Quadros
    MIT License
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "k7.pio.h"

#include "CAR-RACE.h"

uint8_t pgmname[] = "\x29\xB6"; // DQ

#define PIN_EAR 15
#define K7_PIO  pio0

static PIO k7_pio;
static uint k7_sm;
static uint k7_pin;


void send_pgm (uint8_t *name, const uint8_t *code, int size);
void k7PioInit (PIO pio, uint pin);

int main()
{
    stdio_init_all();

    k7PioInit(K7_PIO, PIN_EAR);
	
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, true);
    gpio_put(PICO_DEFAULT_LED_PIN, false);

    while (true) {
        switch (getchar()) {
            case '0':
                // sends zero until a char is received
                while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
                    pio_sm_put_blocking (K7_PIO, k7_sm, 0);  // waits if FIFO is full
                }
                break;
            case '1':
                // sends one until a char is received
                while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
                    pio_sm_put_blocking (K7_PIO, k7_sm, 0xFF000000);  // waits if FIFO is full
                }
                break;
            case 'p':
                // sends program
                send_pgm(pgmname, code, sizeof(code));
                break;
        }
    }
}

// Send a program
// 5 seconds silence
// program name (bit7 set in last char, uses ZX81 char codes)
// code
// silence
void send_pgm (uint8_t *name, const uint8_t *code, int size) {
    busy_wait_ms(5000);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    do {
        pio_sm_put_blocking (K7_PIO, k7_sm, *name << 24);  // waits if FIFO is full
    } while ((*name++ & 0x80) == 0);
    while (size--) {
        pio_sm_put_blocking (K7_PIO, k7_sm, *code++ << 24);  // waits if FIFO is full
    }
    while (!pio_sm_is_tx_fifo_empty (k7_pio, k7_sm)) {
        busy_wait_us(500);
    }
    gpio_put(PICO_DEFAULT_LED_PIN, false);
}

// Inits the PIO
void k7PioInit (PIO pio, uint pin) {
    // Save parammeters
    k7_pio = pio;
    k7_pin = pin;

    // Get a state machine
    k7_sm = pio_claim_unused_sm(pio, true);

    // Loads the program
    uint offset = pio_add_program(pio, &k7_program);

    // Init output pin
    pio_gpio_init(pio, k7_pin);
    pio_sm_set_consecutive_pindirs(pio, k7_sm, k7_pin, 1, true);

    // Configura a maquina de estado
    pio_sm_config c = k7_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, k7_pin);
    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    float div = clock_get_hz(clk_sys)*0.00005;
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, k7_sm, offset, &c);

    // Dispara a execucao da maquina de estado
    pio_sm_set_enabled(pio, k7_sm, true);    
}
