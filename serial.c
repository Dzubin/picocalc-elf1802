/*
 * serial.c - the bit-banged serial line. See serial.h.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "serial.h"

void serial_init(serial_t *s)
{
    memset(s, 0, sizeof *s);
    s->inverted = true;
    s->rx_level = true;
}

void serial_set_speed(serial_t *s, uint32_t baud, uint32_t cycles_per_sec)
{
    bool inverted = s->inverted;

    memset(s, 0, sizeof *s);
    s->inverted = inverted;
    s->rx_level = true;
    if (baud == 0)
        return;
    s->bit_cycles = (cycles_per_sec + baud / 2) / baud;
    if (s->bit_cycles < 2)
        s->bit_cycles = 2;
    s->enabled = true;
}

/* Author: Thomas Dzubin */
void serial_tick(serial_t *s, bool q)
{
    bool line;

    if (!s->enabled)
        return;

    /* what the program sends: a start bit (0), 8 data bits least significant
     * first, a stop bit (1); each bit is read in the middle of its time */
    line = s->inverted ? !q : q;
    switch (s->tx_state) {
    case SERIAL_TX_IDLE:
        if (!line) {
            s->tx_state = SERIAL_TX_START;
            s->tx_timer = s->bit_cycles / 2;
        }
        break;
    case SERIAL_TX_START:
        if (s->tx_timer > 0)
            s->tx_timer--;
        if (s->tx_timer == 0) {
            if (line) {                         /* a glitch, not a start bit */
                s->tx_state = SERIAL_TX_IDLE;
            } else {
                s->tx_state = SERIAL_TX_DATA;
                s->tx_timer = s->bit_cycles;
                s->tx_bit = 0;
                s->tx_byte = 0;
            }
        }
        break;
    case SERIAL_TX_DATA:
        if (--s->tx_timer == 0) {
            if (line)
                s->tx_byte = (uint8_t)(s->tx_byte | (1u << s->tx_bit));
            s->tx_timer = s->bit_cycles;
            if (++s->tx_bit == SERIAL_DATA_BITS)
                s->tx_state = SERIAL_TX_STOP;
        }
        break;
    default:                                    /* SERIAL_TX_STOP */
        if (--s->tx_timer == 0) {
            if (s->out_count < SERIAL_OUT_FIFO) {
                s->out[(s->out_head + s->out_count) % SERIAL_OUT_FIFO] = s->tx_byte;
                s->out_count++;
            }
            s->tx_state = SERIAL_TX_IDLE;
        }
        break;
    }

    /* what the program is sent, a bit slot at a time */
    if (!s->rx_busy) {
        if (s->in_count > 0) {
            s->rx_byte = s->in[s->in_head];
            s->in_head = (uint8_t)((s->in_head + 1) % SERIAL_IN_FIFO);
            s->in_count--;
            s->rx_busy = true;
            s->rx_bit = 0;
            s->rx_timer = s->bit_cycles;
            s->rx_level = false;                /* the start bit */
        }
    } else if (--s->rx_timer == 0) {
        s->rx_timer = s->bit_cycles;
        s->rx_bit++;
        if (s->rx_bit <= SERIAL_DATA_BITS)
            s->rx_level = ((s->rx_byte >> (s->rx_bit - 1)) & 1) != 0;
        else
            s->rx_level = true;                 /* the stop bits */
        if (s->rx_bit >= SERIAL_FRAME_SLOTS) {
            s->rx_busy = false;
            s->rx_level = true;
        }
    }
}

bool serial_flag_high(const serial_t *s)
{
    return s->enabled ? s->rx_level : true;
}

bool serial_send(serial_t *s, uint8_t c)
{
    if (s->in_count >= SERIAL_IN_FIFO)
        return false;
    s->in[(s->in_head + s->in_count) % SERIAL_IN_FIFO] = c;
    s->in_count++;
    return true;
}

int serial_receive(serial_t *s)
{
    int c;

    if (s->out_count == 0)
        return -1;
    c = s->out[s->out_head];
    s->out_head = (uint8_t)((s->out_head + 1) % SERIAL_OUT_FIFO);
    s->out_count--;
    return c;
}
