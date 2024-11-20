/*
    Simple output test
    Sends a program stored in flash

    (C) 2024, Daniel Quadros
    MIT License
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "CAR-RACE.h"

uint8_t pgmname[] = "\x29\xB6"; // DQ

#define PIN_EAR 15

void send_pgm (uint8_t *name, const uint8_t *code, int size);

int main()
{
    stdio_init_all();

    gpio_init(PIN_EAR);
    gpio_set_dir(PIN_EAR, true);
    gpio_put(PIN_EAR, true);    // transistor inverts

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, true);
    gpio_put(PICO_DEFAULT_LED_PIN, false);

    while (true) {
        if (getchar() == 0x0D) {
            send_pgm(pgmname, code, sizeof(code));
        }
    }
}

// Send a byte (using busy wait for timing)
// 0 = 4 pulses + silence
// 1 = 9 pulses + silence
// pulse = 150us High, 150us Low
// silence = 1300 us LOW
void send_byte(uint8_t b) {
    for (int i = 0; i < 8; i++) {
        int n = (b & 0x80) ? 9 : 4;
        for (int j = 0; j < n; j++) {
           gpio_put(PIN_EAR, false);
           busy_wait_us_32(150);
           gpio_put(PIN_EAR, true);
           busy_wait_us_32(150);
        }
        busy_wait_us_32(1350);
        b = b << 1;
    }
}

// Send a program
// 5 seconds silence
// program name (bit7 set in last char, uses ZX81 char codes)
// code
// silence
void send_pgm (uint8_t *name, const uint8_t *code, int size) {
    gpio_put(PIN_EAR, true);   // just to make sure
    busy_wait_ms(5000);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    do {
        send_byte(*name);
    } while ((*name++ & 0x80) == 0);
    while (size--) {
        send_byte(*code++);
    }
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    gpio_put(PIN_EAR, true);   // just to make sure
}
