/*
 * ================================================
 * AVR Register-Level Pin & Interrupt Control
 * ================================================
 * DDRx   (Data Direction Register) — Set pin as input/output
 * PORTx  — Output register: 
 *            - Output mode: Drives HIGH/LOW
 *            - Input mode: Enables/disables internal pull-up
 * PINx   — Input register: Reads pin state; writing 1 toggles the bit
 * ================================================
 * We avoid Arduino built-in functions to access hardware directly.
 * ================================================
 */

/* I have used the TINKER CAD for simulatoion, for other simulators or hardware may required proper headers */
#include <avr/interrupt.h>  
#include <stdbool.h>        
#include <stdint.h>         

/* ========== PUSH BUTTON (PD2 / Arduino pin 2) ========== */
#define PIND_ADDR     0x29  // PIN D register address (read pin state)
#define DDRD_ADDR     0x2A  // Data Direction Register D
#define PORTD_ADDR    0x2B  // PORT D register address
#define BUTTON_MASK   (1u << 2) // PD2 (bit 2) for push button

/* ========== LEDs (PB0, PB1, PB2 / Arduino pins 8, 9, 10) ========== */
#define PINB_ADDR     0x23  // PIN B register address
#define DDRB_ADDR     0x24  // Data Direction Register B
#define PORTB_ADDR    0x25  // PORT B register address

#define LED3_MASK     (1u << 0) // PB0 → Arduino pin 8
#define LED2_MASK     (1u << 1) // PB1 → Arduino pin 9
#define LED1_MASK     (1u << 2) // PB2 → Arduino pin 10

/* ========== External Interrupt Registers (INT0 on PD2) ========== */
#define EICRA_ADDR    0x69  // External Interrupt Control Register A
#define EIMSK_ADDR    0x3D  // External Interrupt Mask Register
#define ISC00_MASK    (1u << 0) // Interrupt Sense Control 0 bit 0
#define ISC01_MASK    (1u << 1) // Interrupt Sense Control 0 bit 1

/* ========== Global Interrupt Enable ========== */
#define SREG_ADDR     0x5F  // Status Register
#define SREG_I_MASK   (1u << 7) // Global Interrupt Enable bit

/* ========= Globals ========= */
volatile int button_count = 0;
volatile bool button_pressed = false;
volatile unsigned long now = 0;
volatile unsigned long last_time = 0;

/* ========= Low-level Register Access ========= */
static inline uint8_t read_reg(uint8_t address) {
    return *((volatile uint8_t *)(address));
}

static inline void write_reg(uint8_t address, uint8_t value) {
    *((volatile uint8_t *)(address)) = value;
}

static inline void clear_bits(uint8_t address, uint8_t mask) {
    write_reg(address, read_reg(address) & ~mask);
}

static inline void set_bits(uint8_t address, uint8_t mask) {
    write_reg(address, read_reg(address) | mask);
}

/* ========= Interrupt Service Routine ========= */
ISR(INT0_vect) {
    button_pressed = true; // Set flag for main loop
}

/* ========= Setup ========= */
void setup() { 
  	last_time = millis();
    /* Configure PD2 as input (button) with pull-up enabled */
    clear_bits(DDRD_ADDR, BUTTON_MASK); // Input mode
    set_bits(PORTD_ADDR, BUTTON_MASK);  // Pull-up enabled

    /* Configure LEDs as output, initially LOW */
    set_bits(DDRB_ADDR, LED1_MASK | LED2_MASK | LED3_MASK);
    clear_bits(PORTB_ADDR, LED1_MASK | LED2_MASK | LED3_MASK);

    /* Configure External Interrupt INT0 on falling edge */
    set_bits(EICRA_ADDR, ISC01_MASK);   // ISC01=1
    clear_bits(EICRA_ADDR, ISC00_MASK); // ISC00=0 → Falling edge

    /* Enable INT0 interrupt */
    set_bits(EIMSK_ADDR, (1u << 0));

    /* Enable Global Interrupts */
    set_bits(SREG_ADDR, SREG_I_MASK);
}

/* ========= Main Loop ========= */
void loop() {
	
  /* Check if the external interrupt service routine (ISR) has flagged a button press */
	if (button_pressed) {
      /* Record the current system time in milliseconds */
      now = millis();
      /* Debounce check:
       * If at least 50 ms have passed since the last valid button press
       * then treat this as a genuine press (filters out contact bounce noise)
      */
      if (now - last_time > 50) { 
          button_count++;/* Increment the press count */
          last_time = now;/* Update the timestamp of the last valid press */
      }
      /* Clear the button press flag so that we don't reprocess the same press */
      button_pressed = false; 
	}


    /* LED control based on button press count */
    switch(button_count) {
        case 1:
            set_bits(PORTB_ADDR, LED1_MASK);
            clear_bits(PORTB_ADDR, LED2_MASK | LED3_MASK);
            break;
        case 2:
            set_bits(PORTB_ADDR, LED2_MASK);
            clear_bits(PORTB_ADDR, LED1_MASK | LED3_MASK);
            break;
        case 3:
            set_bits(PORTB_ADDR, LED3_MASK);
            clear_bits(PORTB_ADDR, LED1_MASK | LED2_MASK);
            break; 
        case 4:
            set_bits(PORTB_ADDR, LED1_MASK | LED2_MASK | LED3_MASK);
            button_count = 0; // Reset cycle
            break;
    }
}
