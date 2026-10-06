/*
 * hexfile.c - Intel HEX loading. See hexfile.h.
 *
 * Author: Thomas Dzubin
 */
#include "hexfile.h"

#define REC_DATA        0x00
#define REC_EOF         0x01
#define REC_SEGMENT     0x02
#define REC_LINEAR      0x04
#define REC_MAX_DATA    255

static int digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* The byte made of the two hex digits at p, or -1. */
static int byte_at(const char *p, const char *end)
{
    int hi, lo;

    if (p + 1 >= end)
        return -1;
    hi = digit(p[0]);
    lo = digit(p[1]);
    return (hi < 0 || lo < 0) ? -1 : (hi << 4) | lo;
}

/* Author: Thomas Dzubin */
int hex_load(const char *text, uint32_t len, uint8_t *image, uint32_t image_size,
             uint32_t *stored, uint32_t *skipped)
{
    const char *p = text;
    const char *end = text + len;
    uint32_t base = 0;                  /* from the segment or linear record */

    *stored = 0;
    *skipped = 0;

    for (;;) {
        int count, hi, lo, type, sum, i;
        uint8_t data[REC_MAX_DATA];
        uint32_t address;

        while (p < end && *p != ':')    /* find the start of a record */
            p++;
        if (p >= end)
            return HEX_OK;              /* ran out of text without an EOF record */
        p++;

        count = byte_at(p, end);
        hi = byte_at(p + 2, end);
        lo = byte_at(p + 4, end);
        type = byte_at(p + 6, end);
        if (count < 0 || hi < 0 || lo < 0 || type < 0)
            return HEX_ERR_FORMAT;
        sum = count + hi + lo + type;
        p += 8;

        for (i = 0; i < count; i++) {
            int b = byte_at(p, end);

            if (b < 0)
                return HEX_ERR_FORMAT;
            data[i] = (uint8_t)b;
            sum += b;
            p += 2;
        }
        i = byte_at(p, end);            /* the checksum byte */
        if (i < 0)
            return HEX_ERR_FORMAT;
        p += 2;
        if (((sum + i) & 0xFF) != 0)
            return HEX_ERR_CHECKSUM;

        address = base + (uint32_t)((hi << 8) | lo);
        switch (type) {
        case REC_DATA:
            for (i = 0; i < count; i++) {
                if (address + (uint32_t)i < image_size) {
                    image[address + (uint32_t)i] = data[i];
                    (*stored)++;
                } else {
                    (*skipped)++;
                }
            }
            break;
        case REC_EOF:
            return HEX_OK;
        case REC_SEGMENT:
            if (count != 2)
                return HEX_ERR_FORMAT;
            base = (uint32_t)((data[0] << 8) | data[1]) << 4;
            break;
        case REC_LINEAR:
            if (count != 2)
                return HEX_ERR_FORMAT;
            base = (uint32_t)((data[0] << 8) | data[1]) << 16;
            break;
        default:                        /* start addresses and the like */
            break;
        }
    }
}
