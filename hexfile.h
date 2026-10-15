/*
 * hexfile.h - reading Intel HEX files, the usual way 1802 programs were
 * published as text: lines of ":" then a byte count, a 16-bit address, a record
 * type, the data and a checksum, in hexadecimal.
 *
 * The file is read a piece at a time (hex_stream_*), so a file of any length
 * can be loaded with a small buffer; hex_load() does a whole text in memory.
 *
 * Portable ISO C. Constants are in elf_const.h.
 *
 * Author: Thomas Dzubin
 */
#ifndef HEXFILE_H
#define HEXFILE_H

#include <stdbool.h>
#include <stdint.h>

#include "elf_const.h"

/* A load in progress. The fields are the loader's own; the caller reads
 * stored, skipped and done. */
typedef struct {
    uint8_t  *image;                    /* where the data goes                     */
    uint32_t  image_size;               /* addresses 0 to image_size - 1           */
    uint32_t  base;                     /* from the segment or linear record       */
    uint32_t  stored;                   /* bytes put in the image                  */
    uint32_t  skipped;                  /* bytes that did not fit                  */
    uint32_t  length;                   /* characters of the record held in line   */
    int       status;                   /* HEX_OK until something is wrong         */
    bool      in_record;                /* a ":" has been seen on this line        */
    bool      done;                     /* the end-of-file record has been read    */
    char      line[HEX_LINE_MAX];       /* the record's text after the ":"         */
} hex_stream_t;

/* Start a load into image, which is image_size bytes and stands for addresses 0
 * to image_size - 1. Data for other addresses (a ROM above the RAM, say) is not
 * stored, only counted. */
void hex_stream_init(hex_stream_t *h, uint8_t *image, uint32_t image_size);

/* Give the loader the next piece of the text (any length; a record may be split
 * between pieces). The text may use LF or CRLF, and anything before a ":" on a
 * line is ignored. The segment (02) and linear (04) address records are
 * followed, the others apart from end-of-file (01) are skipped. Returns
 * HEX_OK, HEX_ERR_FORMAT or HEX_ERR_CHECKSUM; after an error, or after the
 * end-of-file record, the rest of the text is ignored. An error stops the load
 * at that record; what came before it is in the image. */
int hex_stream_feed(hex_stream_t *h, const char *text, uint32_t len);

/* The text has ended: finish a last record that had no newline after it, and
 * return the final status. */
int hex_stream_end(hex_stream_t *h);

/* The whole text at once. On success *stored is the number of bytes put in the
 * image and *skipped the number that did not fit. */
int hex_load(const char *text, uint32_t len, uint8_t *image, uint32_t image_size,
             uint32_t *stored, uint32_t *skipped);

#endif /* HEXFILE_H */
