/*
 * hexfile.h - reading Intel HEX files, the usual way 1802 programs were
 * published as text: lines of ":" then a byte count, a 16-bit address, a record
 * type, the data and a checksum, in hexadecimal.
 *
 * Portable ISO C. Constants are in elf_const.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef HEXFILE_H
#define HEXFILE_H

#include <stdint.h>

#include "elf_const.h"

/* Put the data records of a HEX file into image, which is image_size bytes and
 * stands for addresses 0 to image_size - 1. Data for other addresses (a ROM
 * above the RAM, say) is not stored, only counted. The text need not end in a
 * newline and may use LF or CRLF; anything before a ":" on a line is ignored.
 * The segment (02) and linear (04) address records are followed, the others
 * apart from end-of-file (01) are skipped.
 *
 * Returns HEX_OK, HEX_ERR_FORMAT or HEX_ERR_CHECKSUM. On success *stored is the
 * number of bytes put in the image and *skipped the number that did not fit.
 * An error stops the load at that record; what came before it is in the image. */
int hex_load(const char *text, uint32_t len, uint8_t *image, uint32_t image_size,
             uint32_t *stored, uint32_t *skipped);

#endif /* HEXFILE_H */
