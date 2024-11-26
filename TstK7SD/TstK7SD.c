/*
    Output test using the PIO to generate the pulses
    Sends a program stored in a SD Card

    (C) 2024, Daniel Quadros
    MIT License
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "k7.pio.h"

#include "ff.h"
#include "f_util.h"
#include "sd_card.h"
#include "hw_config.h"

uint8_t pgmname[] = "\x29\xB6"; // DQ

uint8_t code[16*1024];

#define PIN_EAR 15
#define K7_PIO  pio0

static PIO k7_pio;
static uint k7_sm;
static uint k7_pin;


void send_pgm (uint8_t *name, const uint8_t *code, int size);
void k7PioInit (PIO pio, uint pin);
UINT check_code (UINT code);

int main()
{
    stdio_init_all();

    k7PioInit(K7_PIO, PIN_EAR);
	
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, true);
    gpio_put(PICO_DEFAULT_LED_PIN, false);

  #ifdef LIB_PICO_STDIO_USB
  while (!stdio_usb_connected()) {
      sleep_ms(100);
  }
  #endif

    bool sd_ok = false;
    sd_card_t *sd_card_p = sd_get_by_num(0);
    FATFS *fs_p = &sd_card_p->state.fatfs;
    FRESULT fr = f_mount(fs_p, "0:", 1);
    if (fr == FR_OK) {
        printf("SD card mounted\n");
        sd_card_p->state.mounted = true;
        FRESULT fr = f_chdir("/ZX81");
        if (fr == FR_OK) {
            sd_ok = true;
            printf("ZX81 directory found\n");
        } else {
            printf("f_chdir error: %s (%d)\n", FRESULT_str(fr), fr);
        }
    } else {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

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
                if (sd_ok) {
                    FIL fp;
                    fr = f_open(&fp, "CAR-RACE.P", FA_OPEN_EXISTING | FA_READ);
                    if (fr == FR_OK) {
                        UINT size = 0;
                        fr = f_read(&fp, code, 16*1024, &size);
                        f_close(&fp);
                        if (fr == FR_OK) {
                            size = check_code(size);
                            if (size) {
                                printf ("Sendind %u bytes\n", size);
                                send_pgm(pgmname, code, size);
                            } else {
                                printf ("Invalid file\n");
                            }
                        }
                    } else {
                        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
                    }
                }
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
    printf("Sent 0%%\r");
    busy_wait_ms(5000);
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    do {
        pio_sm_put_blocking (K7_PIO, k7_sm, *name << 24);  // waits if FIFO is full
    } while ((*name++ & 0x80) == 0);
    int tosend = size;
    while (tosend--) {
        pio_sm_put_blocking (K7_PIO, k7_sm, *code++ << 24);  // waits if FIFO is full
        if ((tosend & 0x7F) == 0) {
            printf("Sent %d%%\r", 100*(size-tosend)/size);
        }
    }
    while (!pio_sm_is_tx_fifo_empty (k7_pio, k7_sm)) {
        busy_wait_us(500);
    }
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    printf("Sent 100%%\n");
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

// Check code and fix size
// files contains ZX81 memory starting from 4009
// E_LINE (end of saved memory) is at 4014 -> offset 11 in code
// Some P files have garbage added at the end
UINT check_code (UINT size) {
    if ((size <= 120) || (code[0] != 0)) {
        printf ("Error size %u code[0]=%02X\n", size, code[0]);
        return 0;   // too short or invalid version
    }
    UINT real_size = (code[11] + 256*code[12]) - 0x4009;
    if (real_size > size) {
        printf ("Error real size %u\n", real_size);
        return 0;   // something is missing in the file
    }
    if (code[real_size-1] != 0x80) {
        printf ("Error code[real_size-1]=%02X\n", code[real_size-1]);
        return 0;   // last byte should be 0x80
    }
    return real_size;
}
