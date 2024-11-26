/**
 * @file tape.c
 * @author Daniel Quadros
 * @brief Tape simulation - Generates ZX81 K& pulses from a .P file
 * @version 1.0
 * @date 2024-11-23
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

#include "k7.pio.h"

#include "ff.h"
#include "f_util.h"
#include "sd_card.h"
#include "hw_config.h"

#include "picok7.h"

static uint8_t pgmname[] = "\x29\xB6"; // DQ

static uint8_t code[16*1024];

static int8_t led_int;
static int8_t led_delta;

static PIO k7_pio;
static uint k7_sm;
static uint k7_pin;

static UINT check_code (UINT code);
static bool send_pgm (uint8_t *name, const uint8_t *code, int size);
static void updateLED(void);
static void displayPercent(int perc);

// Inits the K7 emulation
void k7Init (PIO pio, uint pin) {
    // Save parammeters
    k7_pio = pio;
    k7_pin = pin;

    // Get a state machine
    k7_sm = pio_claim_unused_sm(pio, true);
    prtdbg("TAPE: sm %d\n", k7_sm);

    // Loads the program
    uint offset = pio_add_program(pio, &k7_program);
    if (offset < 0) {
        prtdbg("TAPE: PIO memory full\n");
    }

    // Init output pin
    pio_gpio_init(pio, k7_pin);
    pio_sm_set_consecutive_pindirs(pio, k7_sm, k7_pin, 1, true);

    // Configure the state machine
    pio_sm_config c = k7_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, k7_pin);
    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    float div = clock_get_hz(clk_sys)*0.00005;
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, k7_sm, offset, &c);

    // Starts the state machine
    pio_sm_set_enabled(pio, k7_sm, true);    
}

// Send program in file
// Returns false if invalid file
bool k7Send (char *pfile) {
  FIL fp;
  FRESULT fr = f_open(&fp, pfile, FA_OPEN_EXISTING | FA_READ);

  displayClear();
  displayStr("Sending", 0, 0, false);
  displayStr(pfile, 1, 0, false);

  if (fr == FR_OK) {
      UINT size = 0;
      fr = f_read(&fp, code, 16*1024, &size);
      f_close(&fp);
      if (fr == FR_OK) {
          size = check_code(size);
          if (size) {
              prtdbg ("Sending %u bytes\n", size);
              return send_pgm(pgmname, code, size);
          } else {
              prtdbg ("Invalid file\n");
          }
      }
  } else {
      prtdbg("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
  }
  return false;
}

// Check code and fix size
// files contains ZX81 memory starting from 4009
// E_LINE (end of saved memory) is at 4014 -> offset 11 in code
// Some P files have garbage added at the end
static UINT check_code (UINT size) {
    if ((size <= 120) || (code[0] != 0)) {
        prtdbg ("Error size %u code[0]=%02X\n", size, code[0]);
        return 0;   // too short or invalid version
    }
    UINT real_size = (code[11] + 256*code[12]) - 0x4009;
    if (real_size > size) {
        prtdbg ("Error real size %u\n", real_size);
        return 0;   // something is missing in the file
    }
    if (code[real_size-1] != 0x80) {
        prtdbg ("Error code[real_size-1]=%02X\n", code[real_size-1]);
        return 0;   // last byte should be 0x80
    }
    return real_size;
}

// Send a program
// 3 seconds silence
// program name (bit7 set in last char, uses ZX81 char codes)
// code
// silence
bool send_pgm (uint8_t *name, const uint8_t *code, int size) {
    displayPercent(0);
    busy_wait_ms(3000);
    led_int = 0;
    led_delta = 2;
    do {
        pio_sm_put_blocking (K7_PIO, k7_sm, *name << 24);  // waits if FIFO is full
        updateLED();
    } while ((*name++ & 0x80) == 0);
    int tosend = size;
    while (tosend--) {
        pio_sm_put_blocking (K7_PIO, k7_sm, *code++ << 24);  // waits if FIFO is full
        if ((tosend & 0x7F) == 0) {
            displayPercent(100*(size-tosend)/size);
        }
        updateLED();
    }
    while (!pio_sm_is_tx_fifo_empty (k7_pio, k7_sm)) {
        updateLED();
        busy_wait_us(500);
    }
    displayPercent(100);
    return true;
}

static void updateLED() {
    ws2812Update(urgb_u32(0,0,led_int));
    if ((led_delta < 0) && (led_int < -led_delta)) {
        led_delta = -led_delta;
    }
    if ((led_int > 120) && (led_delta > 0)) {
        led_delta = -led_delta;
    }
    led_int += led_delta;
}

static void displayPercent(int perc) {
    char aux[] = "Sent     ";
    int i = 5;
    if (perc > 99) {
        aux[i++] = (perc / 100)+'0';
    }
    if (perc > 9) {
        aux[i++] = ((perc / 10) % 10)+'0';
    }
    aux[i++] = (perc % 10) + '0';
    aux [i++] = '%';
    displayStr(aux, 3, 0, false);
}