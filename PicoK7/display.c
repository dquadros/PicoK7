#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/spi.h"
#include "hardware/pio.h"

#include "picok7.h"

#include "font.h"

// SPI Configuration
#define BAUD_RATE 10000000   // 10 MHz
#define DATA_BITS 8

// Logical Levels
#define HIGH 1
#define LOW  0

// RS pin convention
#define DATA HIGH
#define CMD  LOW

// ST7565R Commands
#define ST7565R_DISPLAY_OFF 0xAE
#define ST7565R_DISPLAY_ON  0xAF
#define ST7565R_SET_DISP_START_LINE 0x40
#define ST7565R_SET_PAGE    0xB0
#define ST7565R_SET_COLUMN_UPPER  0x10
#define ST7565R_SET_COLUMN_LOWER  0x00
#define ST7565R_SET_ADC_NORMAL  0xA0
#define ST7565R_SET_ADC_REVERSE 0xA1
#define ST7565R_SET_ALLPTS_NORMAL 0xA4
#define ST7565R_SET_ALLPTS_ON  0xA5
#define ST7565R_SET_BIAS_9 0xA2 
#define ST7565R_SET_BIAS_7 0xA3
#define ST7565R_RMW  0xE0
#define ST7565R_RMW_CLEAR 0xEE
#define ST7565R_INTERNAL_RESET  0xE2
#define ST7565R_SET_COM_NORMAL  0xC0
#define ST7565R_SET_COM_REVERSE  0xC8
#define ST7565R_SET_POWER_CONTROL  0x28
#define ST7565R_SET_RESISTOR_RATIO  0x20
#define ST7565R_SET_VOLUME_FIRST  0x81
#define ST7565R_SET_VOLUME_SECOND  0
#define ST7565R_SET_STATIC_OFF  0xAC
#define ST7565R_SET_STATIC_ON  0xAD
#define ST7565R_SET_STATIC_REG  0x0
#define ST7565R_SET_BOOSTER_FIRST  0xF8
#define ST7565R_SET_BOOSTER_234  0
#define ST7565R_SET_BOOSTER_5  1
#define ST7565R_SET_BOOSTER_6  3
#define ST7565R_NOP  0xE3
#define ST7565R_TEST  0xF0

// Screen size
#define LCD_WIDTH    128
#define LCD_HEIGHT   64

// Display initialization commands
static uint8_t cmdInit[] = {
  // cmd                              // sleep_ms
  ST7565R_SET_BIAS_7,                 0, 
  ST7565R_SET_ADC_NORMAL,             0,
  ST7565R_SET_COM_NORMAL,             0,
  ST7565R_SET_DISP_START_LINE,        0,
  ST7565R_SET_POWER_CONTROL | 0x4,    50,
  ST7565R_SET_POWER_CONTROL | 0x6,    50,
  ST7565R_SET_POWER_CONTROL | 0x7,    10,
  ST7565R_SET_RESISTOR_RATIO | 0x6,   0,
  ST7565R_DISPLAY_ON,                 0,
  ST7565R_SET_ALLPTS_NORMAL,          0,
  ST7565R_SET_VOLUME_FIRST,           0,
  ST7565R_SET_VOLUME_SECOND | 13,     0
};

// Buffer for a screen page (128x8 pixels)
static uint8_t linBuf[LCD_WIDTH];

// Local routines
static inline void pinInit(int pin, int value) {
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  gpio_put(pin, value);
}
void Display_chr(char chr, int l, int c);
void Display_invchr(char chr, int l, int c);
static void Display_sendcmds (uint8_t *cmd, int nCmds);
static void Display_sendcmd (uint8_t cmd);

// Initialize the display
void displayInit()
{
  // Setup SPI
  uint baud = spi_init (LCD_SPI, BAUD_RATE);
  spi_set_format (LCD_SPI, DATA_BITS, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
  gpio_set_function(LCD_pinSCL, GPIO_FUNC_SPI);
  gpio_set_function(LCD_pinSI, GPIO_FUNC_SPI);

  // Setup other pins
  pinInit(LCD_pinCS, HIGH);
  pinInit(LCD_pinRES, LOW);
  pinInit(LCD_pinRS, DATA);

  // Reset controller
  gpio_put(LCD_pinRES, LOW);
  sleep_ms (500);
  gpio_put(LCD_pinRES, HIGH);
  sleep_ms (100);
  
  // Configure controller and clean the screen
  Display_sendcmds (cmdInit, sizeof(cmdInit)/2);
  displayClear();
}

// Clear display
void displayClear()
{
  memset(linBuf, 0x00, LCD_WIDTH);

  // write zeroes in all columns in all pages
  for (uint8_t p = 0; p < LCD_HEIGHT/8; p++) {
    Display_sendcmd(ST7565R_SET_PAGE | p);
    Display_sendcmd(ST7565R_SET_COLUMN_UPPER | 0);
    Display_sendcmd(ST7565R_SET_COLUMN_LOWER | 0);
    gpio_put(LCD_pinCS, LOW);
    gpio_put(LCD_pinRS, DATA);
    spi_write_blocking(LCD_SPI, linBuf, LCD_WIDTH);
    gpio_put(LCD_pinCS, HIGH);
  }
}

// Write string s starting at line l (0-7) collumn c (0-16)
void displayStr(char *str, int l, int c, bool inverse) {
  while (*str) {
    if (inverse) {
      Display_invchr(*str, l, c);  
    } else {
      Display_chr(*str, l, c);
    }
    if (++c == 16) {
      c = 0;
      if (++l == 8) {
        l = 0;
      }
    }
    str++;
  }
}

// Implements a simple menu
int displayMenu (int lt, int nl, int nopc, char *opc[]) {
  char aux[LCD_WIDTH/8+1];
  int sel = 0;
  int top = 0;

  while (true) {
    for (int i = 0, j = top; (i < nl) && (j < nopc); i++, j++) {
      memset(aux, ' ', LCD_WIDTH/8);
      aux[LCD_WIDTH/8] = 0;
      int s = strlen(opc[j]);
      memcpy (aux, opc[j], s < (LCD_WIDTH/8) ? s : (LCD_WIDTH/8));
      displayStr(aux, lt+i, 0, j == sel);
    }
    switch (getKey()) {
      case KEY_ENTER:
        return sel;
      case KEY_DN:
        if (sel > 0) {
          sel--;
          if (sel < top) {
            top--;
          }
        }
        break;
      case KEY_UP:
        if (sel < (nopc-1)) {
          sel++;
          if (sel >= (top+nl)) {
            top++;
          }
        }
        break;
    }
  }
}

// Write char chr at line l (0-7) collumn c (0-16)
void Display_chr(char chr, int l, int c) {
  uint8_t *pgc = (uint8_t *) (font + ((chr - 0x20) << 3));
  c = c << 3;

  Display_sendcmd(ST7565R_SET_PAGE | (7-l));  // page numbered from bottom to top
  Display_sendcmd(ST7565R_SET_COLUMN_UPPER | (c >> 4));
  Display_sendcmd(ST7565R_SET_COLUMN_LOWER | (c & 0x0F));
  gpio_put(LCD_pinCS, LOW);
  gpio_put(LCD_pinRS, DATA);
  spi_write_blocking(LCD_SPI, pgc, 8);
  gpio_put(LCD_pinCS, HIGH);
}

// Write inverse char chr at line l (0-7) collumn c (0-16)
void Display_invchr(char chr, int l, int c) {
  uint8_t aux[8];
  uint8_t *pgc = (uint8_t *) (font + ((chr - 0x20) << 3));
  for (int i = 0; i < 8; i++) {
    aux[i] = pgc[i] ^0xFF;
  }
  c = c << 3;

  Display_sendcmd(ST7565R_SET_PAGE | (7-l));  // page numbered from bottom to top
  Display_sendcmd(ST7565R_SET_COLUMN_UPPER | (c >> 4));
  Display_sendcmd(ST7565R_SET_COLUMN_LOWER | (c & 0x0F));
  gpio_put(LCD_pinCS, LOW);
  gpio_put(LCD_pinRS, DATA);
  spi_write_blocking(LCD_SPI, aux, 8);
  gpio_put(LCD_pinCS, HIGH);
}

// Send a sequence of commands to the display, with sleep_mss
static void Display_sendcmds (uint8_t *cmd, int nCmds)
{
  gpio_put(LCD_pinCS, LOW);
  gpio_put(LCD_pinRS, CMD);
  for (int i = 0; i < nCmds*2; i+=2)
  {
    spi_write_blocking(LCD_SPI, &cmd[i], 1);
    if (cmd[i+1] != 0) {
      sleep_ms(cmd[i+1]);
    }
  }
  gpio_put(LCD_pinCS, HIGH);
}

// Send a command uint8_t to the display
static void Display_sendcmd (uint8_t cmd)
{
  gpio_put(LCD_pinCS, LOW);
  gpio_put(LCD_pinRS, CMD);
  spi_write_blocking(LCD_SPI, &cmd, 1);
  gpio_put(LCD_pinCS, HIGH);
}

