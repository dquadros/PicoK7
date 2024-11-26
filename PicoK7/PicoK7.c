/**
 * @file PicoK7.c
 * @author Daniel Quadros
 * @brief K7 emulator - Generates ZX81 K& pulses from a .P file
 * @version 1.0
 * @date 2024-11-23
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

#include "ff.h"
#include "f_util.h"
#include "sd_card.h"
#include "hw_config.h"

#include "picok7.h"

#define MAX_PFILES   50
static char *pfiles[MAX_PFILES];
static int npfiles;

static bool findPFiles(void);

int main()
{
    stdio_init_all();

    #ifdef LIB_PICO_STDIO_USB
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    #endif

    ws282Init(WS2812_PIO, PIN_WS2812);
    ws2812Update(urgb_u32(0,0,128));
    k7Init(K7_PIO, PIN_EAR);
    encoderInit(ENC_PIO, PIN_ENC_CLK, PIN_ENC_DT, PIN_ENC_SW);
    displayInit();
    displayStr((char *)"PicoK7 v1.00    ", 0, 0, true);

    bool sd_ok = false;
    sd_card_t *sd_card_p = sd_get_by_num(0);
    FATFS *fs_p = &sd_card_p->state.fatfs;
    FRESULT fr = f_mount(fs_p, "0:", 1);
    if (fr == FR_OK) {
        prtdbg("SD card mounted\n");
        sd_card_p->state.mounted = true;
        FRESULT fr = f_chdir("/ZX81");
        if (fr == FR_OK) {
            sd_ok = true;
            prtdbg("ZX81 directory found\n");
            displayStr((char *) "SD card OK", 1, 0, false);
        } else {
            prtdbg("f_chdir error: %s (%d)\n", FRESULT_str(fr), fr);
            displayStr((char *) "No ZX81 dir", 2, 0, false);
        }
    } else {
        prtdbg("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        displayStr((char *) "SD not found!", 2, 0, false);
    }

    if (sd_ok) {
        if (!findPFiles()) {
            displayStr((char *) "No P file!", 2, 0, false);
            sd_ok = false;
        }
    }

    if (sd_ok) {
        while (true) {
            displayClear();
            ws2812Update(urgb_u32(0,127,0));
            displayStr((char *)"==File to Send==", 0, 0, false);
            int sel = displayMenu(2, 5, npfiles, pfiles);
            if (k7Send(pfiles[sel])) {
                ws2812Update(urgb_u32(0,127,0));
            }
            else {
                displayStr((char *)"Invalid file", 6, 0, false);
                ws2812Update(urgb_u32(128,0,0));
            }
            displayStr((char *)"Press Enter", 7, 0, false);
            while (getKey() != KEY_ENTER) {
                sleep_ms(100);        
            }
        }
    } else {
        ws2812Update(urgb_u32(128,0,0));
    }

    while (true) {
        sleep_ms(100);
    }
}


// Find all .P files in current directory
// Returns false if error
bool findPFiles() {
    char cwdbuf[FF_LFN_BUF] = {0};
    FRESULT fr;

    npfiles = 0;

    fr = f_getcwd(cwdbuf, sizeof cwdbuf);
    if (FR_OK != fr) {
        prtdbg("f_getcwd error: %s (%d)\n", FRESULT_str(fr), fr);
        return false;
    }
    char const *p_dir = cwdbuf;

    DIR dj = {};      /* Directory object */
    FILINFO fno = {}; /* File information */
    fr = f_findfirst(&dj, &fno, p_dir, "*.P");
    if (FR_OK != fr) {
        prtdbg("f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        return false;
    }
    while (fr == FR_OK && fno.fname[0]) {
        if ((fno.fattrib & AM_DIR) == 0) {
            char *name = (char *) malloc(strlen(fno.fname)+1);
            if (name == NULL) {
                prtdbg("Out of memory\n");
                return false;
            }
            strcpy (name, fno.fname);
            if (npfiles < MAX_PFILES) {
                pfiles[npfiles++] = name;
            } else {
                prtdbg("Too many files\n");
                break;
            }
        }
        fr = f_findnext(&dj, &fno); /* Search for next item */
    }
    f_closedir(&dj);
    prtdbg("Found %d files\n", npfiles);
    return true;
}
