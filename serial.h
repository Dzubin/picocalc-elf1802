/*
 * serial.h - a bit-banged serial line for a program that sends characters by
 * switching Q and reads them by testing a flag line (EF3), the way the 1802
 * Membership Card and Elf/OS talk to a terminal. serial_tick() watches Q once
 * a machine cycle and turns the bits into characters (8 data bits, no parity,
 * one stop bit); typed characters are turned into bits on the flag line.
 *
 * The line is "marking" (idle) at logic 1. With inverted set the output is
 * the opposite of Q (Q low is idle, which is how a driver that inverts Q turns
 * it into an RS-232 line); the flag line is low for a 0 bit and high for a 1,
 * so it is high when idle. Which one a real board uses is not confirmed, so
 * the program lets the user choose.
 *
 * Portable ISO C.
 *
 * Author: Thomas Dzubin
 */
#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#include "serial_const.h"

typedef struct {
    bool     enabled;
    bool     inverted;                  /* Q is the opposite of the line level  */
    uint32_t bit_cycles;                /* machine cycles to a bit              */

    uint8_t  tx_state;                  /* receiving what the program sends     */
    uint32_t tx_timer;
    uint8_t  tx_byte;
    uint8_t  tx_bit;
    uint8_t  out[SERIAL_OUT_FIFO];      /* characters from the program          */
    uint8_t  out_head, out_count;

    uint8_t  in[SERIAL_IN_FIFO];        /* characters to send to the program    */
    uint8_t  in_head, in_count;
    bool     rx_busy;                   /* a character is being sent            */
    uint8_t  rx_bit;                    /* which bit slot of it                 */
    uint8_t  rx_byte;
    uint32_t rx_timer;
    bool     rx_level;                  /* the flag line: true is high          */
} serial_t;

/* Off, normal polarity. */
void serial_init(serial_t *s);

/* Set the speed (baud 0 turns the line off); cycles_per_sec is the machine
 * cycles in a second. Anything in progress is dropped. */
void serial_set_speed(serial_t *s, uint32_t baud, uint32_t cycles_per_sec);

/* One machine cycle has gone by with Q at this level. */
void serial_tick(serial_t *s, bool q);

/* The level of the flag line (EF3) now: true is high. High when off. */
bool serial_flag_high(const serial_t *s);

/* Queue a typed character for the program; false if the queue is full. */
bool serial_send(serial_t *s, uint8_t c);

/* The next character the program has sent, or -1 if there is none. */
int serial_receive(serial_t *s);

#endif /* SERIAL_H */
