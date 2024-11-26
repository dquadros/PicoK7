/**
 * @file ws2812,c
 * @author Daniel Quadros
 * @brief WS2312 RGB LED Control
 * @version 1.0
 * @date 2024-11-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdlib.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

#include "ws2812.pio.h"

#include "picok7.h"

#define FREQ 800000.0

static PIO WS2812_pio;
static int WS2812_sm;

// Init PIO state machine for send commands to the WS2812 LED
void ws282Init(PIO pio, uint pin) {

    WS2812_pio = pio;
    WS2812_sm = pio_claim_unused_sm(pio, true);
    prtdbg("WS2812: sm %d\n", WS2812_sm);
    int offset = pio_add_program(pio, &ws2812_program);
    if (offset < 0) {
      prtdbg("WS2812: PIO memory full\n");
    }

    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, WS2812_sm, pin, 1, true);

    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    int cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3;
    float div = clock_get_hz(clk_sys) / (FREQ * cycles_per_bit);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, WS2812_sm, offset, &c);
    pio_sm_set_enabled(pio, WS2812_sm, true);
}

// Update LED color
void ws2812Update(uint32_t pixel) {
    sleep_us(50);   // marks start
    pio_sm_put_blocking(WS2812_pio, WS2812_sm, pixel);
}
