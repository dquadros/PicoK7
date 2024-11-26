/**
 * @file encoder.cpp
 * @author Daniel Quadros
 * @brief Rotary encoder (with switch) input
 * @version 1.0
 * @date 2022-11-16
 * 
 * This is a simplified version of the code at
 * https://github.com/pimoroni/pimoroni-pico/tree/main/drivers/encoder
 *  
 * We will only generate UP and DOWN "keys" when the encoder is
 * rotate clockwise or anit-clockwise
 * see http://dqsoft.blogspot.com/2020/07/usando-um-rotary-encoder.html
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdlib.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

#include "encoder.pio.h"

#include "picok7.h"

#define DEBOUNCE_MS 100
#define T_QUEUE 32

// State codes

static const uint32_t STATE_A_MASK      = 0x80000000;
static const uint32_t STATE_B_MASK      = 0x40000000;
static const uint32_t STATE_A_LAST_MASK = 0x20000000;
static const uint32_t STATE_B_LAST_MASK = 0x10000000;

static const uint32_t STATES_MASK = STATE_A_MASK | STATE_B_MASK |
                                    STATE_A_LAST_MASK | STATE_B_LAST_MASK;

static const uint32_t TIME_MASK   = 0x0fffffff;

#define LAST_STATE(state)  ((state) & 0b0011)
#define CURR_STATE(state)  (((state) & 0b1100) >> 2)

// Direction control

enum StepDir {
    NO_DIR    = 0,
    INCREASING = 1,
    DECREASING = -1,
};

enum MicroStep {
    MICROSTEP_0 = 0b00,
    MICROSTEP_1 = 0b10,
    MICROSTEP_2 = 0b11,
    MICROSTEP_3 = 0b01,
};

// Local variables
static struct repeating_timer timer;

static PIO enc_pio;
static uint enc_sm;
static uint enc_pin_a, enc_pin_b, enc_pin_sw;

static volatile bool enc_state_a = false;
static volatile bool enc_state_b = false;
static volatile bool enc_sw_pressed = false;
static volatile int enc_cnt_debounce = 0;

static int key_queue[T_QUEUE];
static volatile int q_input, q_output;

// store key in queue
static inline void storeKey(int key) {
    int prox = (q_input + 1) % T_QUEUE;
    if (prox != q_output) {
        key_queue[q_input] = key;
        q_input = prox;
    } else {
        // queue is full, ignore key
    }
}

// Update switch state
static bool checkSwitch(struct repeating_timer *t) {
        bool pressed = ! gpio_get (enc_pin_sw);
        if (pressed == enc_sw_pressed) {
            // State has not changed
            enc_cnt_debounce = 0;
        } else {
            if (enc_cnt_debounce == 0) {
                // Changed, start debounce
                enc_cnt_debounce = DEBOUNCE_MS/10;
            } else if (--enc_cnt_debounce == 0) {
                // State change validated
                enc_sw_pressed = pressed;
            if (pressed) {
                // Store KEY_ENTER when pressed
                storeKey (KEY_ENTER);
            }
        }
    }
    return true; // key calling
}

// Handle PIO interrupt
static void pio_interrupt_handler() {
    enum StepDir step;

    // Handle data in input queue
    while(enc_pio->ints1 & (PIO_IRQ1_INTS_SM0_RXNEMPTY_BITS << enc_sm)) {
        uint32_t received = pio_sm_get(enc_pio, enc_sm);

        // Extract current and previous states from the value from the queue
        enc_state_a = (bool)(received & STATE_A_MASK);
        enc_state_b = (bool)(received & STATE_B_MASK);
        uint8_t states = (received & STATES_MASK) >> 28;

        step = NO_DIR;

        // Handle step, we only care gor two conditions
        if ((LAST_STATE(states) == MICROSTEP_0) && (CURR_STATE(states) == MICROSTEP_1)) {
            // A ____|‾‾‾‾
            // B _________
            step = INCREASING;
        } else if ((LAST_STATE(states) == MICROSTEP_3) && (CURR_STATE(states) == MICROSTEP_2)) {
            // A ____|‾‾‾‾
            // B ‾‾‾‾‾‾‾‾‾
            step = DECREASING;
        }
        
        if (step != NO_DIR) {
            // Generate UP or DN key
            storeKey(step == INCREASING? KEY_UP: KEY_DN);
        }
    }    
}


// Encoder initialization
void encoderInit (PIO pio, uint pin_a, uint pin_b, uint pin_sw) {
    // Save parameters
    enc_pio = pio;
    enc_pin_a = pin_a;
    enc_pin_b = pin_b;
    enc_pin_sw = pin_sw;

    // Init queue
    q_input = q_output = 0; 

    // Init switch monitoring
    gpio_init(pin_sw);
    gpio_set_dir(pin_sw, GPIO_IN);
    gpio_pull_up(pin_sw);
    enc_sw_pressed = false;
    enc_cnt_debounce = 0;
    add_repeating_timer_ms(10, checkSwitch, NULL, &timer);

    // Alocate a state machine
    enc_sm = pio_claim_unused_sm(pio, true);
    prtdbg("ENCODER: sm %d\n", enc_sm);

    // Load PIO program
    int offset = pio_add_program(pio, &encoder_program);
    if (offset < 0) {
        prtdbg("ENCODER: PIO memory full\n");
    }

    // Init encoder pins
    pio_gpio_init(pio, enc_pin_a);
    pio_gpio_init(pio, enc_pin_b);
    gpio_pull_up(enc_pin_a);
    gpio_pull_up(enc_pin_b);
    pio_sm_set_consecutive_pindirs(pio, enc_sm, enc_pin_a, 1, false);
    pio_sm_set_consecutive_pindirs(pio, enc_sm, enc_pin_b, 1, false);

    // Setup state machine
    pio_sm_config c = encoder_program_get_default_config(offset);
    sm_config_set_jmp_pin(&c, enc_pin_a);
    sm_config_set_in_pins(&c, enc_pin_b);
    sm_config_set_in_shift(&c, false, false, 1);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv_int_frac(&c, 250, 0);
    pio_sm_init(pio, enc_sm, offset, &c);

    // Setup interrupt
    hw_set_bits(&pio->inte1, PIO_IRQ1_INTE_SM0_RXNEMPTY_BITS << enc_sm);
    if(pio_get_index(pio) == 0) {
        irq_add_shared_handler(PIO0_IRQ_1, pio_interrupt_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(PIO0_IRQ_1, true);
    } else {
        irq_add_shared_handler(PIO1_IRQ_1, pio_interrupt_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(PIO1_IRQ_1, true);
    }

    // Set initial state
    enc_state_a = gpio_get(enc_pin_a);
    enc_state_b = gpio_get(enc_pin_b);

    // Set up X register, executing "SET X,state" instruction
    pio_sm_exec(pio, enc_sm, pio_encode_set(pio_x, (uint)enc_state_a << 1 | (uint)enc_state_b));

    // Start state machine execution
    pio_sm_set_enabled(pio, enc_sm, true);    
}

// get next key from queue, returns -1 is queue is empty
int getKey () {
    if (q_output == q_input) {
        return -1;
    }
    int key = key_queue[q_output];
    q_output = (q_output + 1) % T_QUEUE;
    return key;
}
