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

void hex_stream_init(hex_stream_t *h, uint8_t *image, uint32_t image_size)
{
    h->image = image;
    h->image_size = image_size;
    h->base = 0;
    h->stored = 0;
    h->skipped = 0;
    h->length = 0;
    h->status = HEX_OK;
    h->in_record = false;
    h->done = false;
}

/* One record, the text after its ":" being in h->line. Sets h->status on an
 * error and h->done at the end-of-file record.
 *
 * Author: Thomas Dzubin */
static void record(hex_stream_t *h)
{
    const char *p = h->line;
    const char *end = h->line + h->length;
    int count, hi, lo, type, sum, i;
    uint8_t data[REC_MAX_DATA];
    uint32_t address;

    count = byte_at(p, end);
    hi = byte_at(p + 2, end);
    lo = byte_at(p + 4, end);
    type = byte_at(p + 6, end);
    if (count < 0 || hi < 0 || lo < 0 || type < 0) {
        h->status = HEX_ERR_FORMAT;
        return;
    }
    sum = count + hi + lo + type;
    p += 8;

    for (i = 0; i < count; i++) {
        int b = byte_at(p, end);

        if (b < 0) {
            h->status = HEX_ERR_FORMAT;
            return;
        }
        data[i] = (uint8_t)b;
        sum += b;
        p += 2;
    }
    i = byte_at(p, end);                /* the checksum byte */
    if (i < 0) {
        h->status = HEX_ERR_FORMAT;
        return;
    }
    if (((sum + i) & 0xFF) != 0) {
        h->status = HEX_ERR_CHECKSUM;
        return;
    }

    address = h->base + (uint32_t)((hi << 8) | lo);
    switch (type) {
    case REC_DATA:
        for (i = 0; i < count; i++) {
            if (address + (uint32_t)i < h->image_size) {
                h->image[address + (uint32_t)i] = data[i];
                h->stored++;
            } else {
                h->skipped++;
            }
        }
        break;
    case REC_EOF:
        h->done = true;
        break;
    case REC_SEGMENT:
        if (count != 2)
            h->status = HEX_ERR_FORMAT;
        else
            h->base = (uint32_t)((data[0] << 8) | data[1]) << 4;
        break;
    case REC_LINEAR:
        if (count != 2)
            h->status = HEX_ERR_FORMAT;
        else
            h->base = (uint32_t)((data[0] << 8) | data[1]) << 16;
        break;
    default:                            /* start addresses and the like */
        break;
    }
}

/* The record held so far is complete (its line ended). */
static void end_record(hex_stream_t *h)
{
    if (h->in_record)
        record(h);
    h->in_record = false;
    h->length = 0;
}

int hex_stream_feed(hex_stream_t *h, const char *text, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len && h->status == HEX_OK && !h->done; i++) {
        char c = text[i];

        if (c == '\n' || c == '\r') {
            end_record(h);
        } else if (h->in_record) {
            if (h->length >= HEX_LINE_MAX)
                h->status = HEX_ERR_FORMAT;     /* longer than any record */
            else
                h->line[h->length++] = c;
        } else if (c == ':') {
            h->in_record = true;
            h->length = 0;
        }
    }
    return h->status;
}

int hex_stream_end(hex_stream_t *h)
{
    if (h->status == HEX_OK && !h->done)
        end_record(h);                  /* a last record with no newline */
    return h->status;
}

int hex_load(const char *text, uint32_t len, uint8_t *image, uint32_t image_size,
             uint32_t *stored, uint32_t *skipped)
{
    hex_stream_t h;
    int rc;

    hex_stream_init(&h, image, image_size);
    hex_stream_feed(&h, text, len);
    rc = hex_stream_end(&h);
    *stored = h.stored;
    *skipped = h.skipped;
    return rc;
}
