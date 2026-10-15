/*
 * serial_const.h - constants of the serial line (serial.c): the speeds on offer
 * and the sizes of its two queues.
 *
 * Author: Thomas Dzubin
 */
#ifndef SERIAL_CONST_H
#define SERIAL_CONST_H

#define SERIAL_OUT_FIFO     64      /* characters from the program, waiting to be shown */
#define SERIAL_IN_FIFO      32      /* typed characters waiting to be sent to the program */

/* The speeds the menu steps through; the first, 0, is off, and the next is the
 * 4800 baud of the Membership Card. */
#define SERIAL_BAUD_STEPS   6
#define SERIAL_BAUDS        { 0, 4800, 9600, 2400, 1200, 300 }

#define SERIAL_DATA_BITS    8       /* 8 data bits, no parity, one stop bit     */
#define SERIAL_FRAME_SLOTS  11      /* start, 8 data, 2 stop: the program is given
                                       an extra bit of rest between characters  */

/* The states of the receiver of what the program sends (on Q). */
#define SERIAL_TX_IDLE      0       /* waiting for a start bit                  */
#define SERIAL_TX_START     1       /* half a bit in, checking the start bit    */
#define SERIAL_TX_DATA      2
#define SERIAL_TX_STOP      3

#endif /* SERIAL_CONST_H */
