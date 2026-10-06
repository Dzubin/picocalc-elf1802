/*
 * source_gen.h - assembly source text made from the bytes of a program, for any
 * processor with an isa_t (isa.h): the listing the editor shows when a program
 * is opened and the file the menu's D key writes. Portable ISO C.
 *
 * It follows the program from address 0 through its branches, jumps and calls (and
 * through addresses that the processor's description says registers point at) to
 * tell code from data. What it reaches is written as instructions, with a label
 * (L0012:) on every address that a branch goes to; the rest is data, written as
 * compact DB lines of eight bytes, DB "TEXT" for printable text and DS n for runs
 * of zeros. A target that has no line of its own (in the middle of an instruction,
 * or past the end of the text) is written as a hex address, so the text always
 * assembles without errors to the same bytes as the image.
 *
 * Author: Thomas Dzubin
 */
#ifndef SOURCE_GEN_H
#define SOURCE_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "isa.h"

/* Write source text for the image (size bytes) into out: an ORG line, then the
 * lines for address 0 to the last byte that is not zero. If out is too small
 * the text stops at a whole line; the number of image bytes done is returned in
 * *done. Returns the length of the text (not counting the final NUL). */
size_t srcgen_from_image(const isa_t *isa, const uint8_t *image, uint32_t size,
                         char *out, size_t out_size, uint32_t *done);

/* The same source for the whole image, however long, a piece at a time (for
 * writing a file). srcgen_listing_start() looks at the image, which has to stay
 * as it is until the listing is done; each srcgen_listing_next() then puts as
 * many whole lines as fit into out (out_size at least 128) and returns their
 * length; *finished is set with the last of them. It shares its work space with
 * srcgen_from_image(), so do not mix the two. */
void   srcgen_listing_start(const isa_t *isa, const uint8_t *image, uint32_t size);
size_t srcgen_listing_next(char *out, size_t out_size, bool *finished);

/* A number as the processor's source text spells it (7F, $7F or 7FH, with a 0 in
 * front of a letter where that is needed), in the given number of digits. */
void srcgen_hex(const isa_t *isa, char *out, size_t size, unsigned value, int digits);

/* Mark the addresses that branches and jumps in the image go to, for a display
 * to show them. It decodes the image from address 0 to its last non-zero byte,
 * one instruction after another (data is decoded as instructions too, so it can
 * add a mark that is not a real target), and sets bit (address & 7) of
 * marks[address >> 3] for each target inside the image. marks has size / 8
 * bytes and is cleared first. */
void srcgen_mark_targets(const isa_t *isa, const uint8_t *image, uint32_t size,
                         uint8_t *marks);

#endif /* SOURCE_GEN_H */
