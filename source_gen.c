/*
 * source_gen.c - source text from a program's bytes, for any processor. See
 * source_gen.h.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_const.h"
#include "source_gen.h"

/* ------------------------------------------------------------------ */
/*  Bits                                                                */
/* ------------------------------------------------------------------ */
static bool bit_get(const uint8_t *map, uint32_t i)
{
    return (map[i >> 3] >> (i & 7)) & 1u;
}

static void bit_set(uint8_t *map, uint32_t i)
{
    map[i >> 3] |= (uint8_t)(1u << (i & 7));
}

void srcgen_hex(const isa_t *isa, char *out, size_t size, unsigned value, int digits)
{
    if (isa->numbers & ISA_NUM_HEX_DEFAULT)
        snprintf(out, size, "%0*X", digits, value);
    else if (isa->numbers & ISA_NUM_H_SUFFIX)           /* Intel's: 7FH, 0FFH */
        snprintf(out, size, "%s%0*XH", value >> (4 * (digits - 1)) > 9 ? "0" : "", digits, value);
    else
        snprintf(out, size, "$%0*X", digits, value);
}

/* The bytes at addr, as many as the longest instruction, with zeros past the
 * end of the image. */
static void fetch(const uint8_t *image, uint32_t size, uint32_t addr, uint8_t *b)
{
    int i;

    for (i = 0; i < ISA_MAX_BYTES; i++)
        b[i] = addr + (uint32_t)i < size ? image[addr + (uint32_t)i] : 0;
}

/* ------------------------------------------------------------------ */
/*  Which bytes are code?                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    const isa_t *isa;
    const uint8_t *image;
    uint32_t size, limit;
    uint8_t *starts;                    /* a bit for each instruction found        */
    uint8_t *targets;                   /* a bit for each address a branch goes to */
    uint16_t stack[ASM_FLOW_STACK];     /* addresses still to follow               */
    int      sp;
} trace_t;

/* An address that code goes to: it gets a label if label is set, and the code
 * there is followed too. */
static void follow(trace_t *t, uint32_t target, bool label)
{
    if (target >= t->size)
        return;
    if (label)
        bit_set(t->targets, target);
    if (target < t->limit && !bit_get(t->starts, target) && t->sp < ASM_FLOW_STACK)
        t->stack[t->sp++] = (uint16_t)target;
}

/* Follow the code from pc: every instruction reached gets its bit in starts, and
 * every address a branch goes to a bit in targets. The run ends at an
 * instruction that does not go on, at one that was seen before, or at the end of
 * the code.
 *
 * Author: Thomas Dzubin */
static void trace_run(trace_t *t, uint32_t pc)
{
    while (pc < t->limit && !bit_get(t->starts, pc)) {
        uint8_t b[ISA_MAX_BYTES];
        isa_flow_t f;

        fetch(t->image, t->size, pc, b);
        t->isa->flow((uint16_t)pc, b, &f);
        bit_set(t->starts, pc);
        if (f.length < 1)
            f.length = 1;
        switch (f.kind) {
        case ISA_FLOW_BAD:
        case ISA_FLOW_STOP:
            return;
        case ISA_FLOW_JUMP:
            follow(t, (uint32_t)f.target, true);
            return;
        case ISA_FLOW_SKIP:
            follow(t, pc + (uint32_t)f.length + (uint32_t)f.skip, false);
            return;
        case ISA_FLOW_BRANCH:
        case ISA_FLOW_CALL:
            follow(t, (uint32_t)f.target, true);
            break;
        default:                                    /* NEXT and DATA */
            if (f.skip > 0)
                follow(t, pc + (uint32_t)f.length + (uint32_t)f.skip, false);
            break;
        }
        pc += (uint32_t)f.length;
    }
}

/* Find the code: from address 0, then from every address that the processor's
 * description says is gone to some other way, until nothing new turns up.
 *
 * Author: Thomas Dzubin */
static void find_code(const isa_t *isa, const uint8_t *image, uint32_t size,
                      uint32_t limit, uint8_t *starts, uint8_t *targets)
{
    static trace_t t;
    bool more = true;

    memset(starts, 0, size / 8);
    memset(targets, 0, size / 8);
    if (limit == 0)
        return;
    t.isa = isa;
    t.image = image;
    t.size = size;
    t.limit = limit;
    t.starts = starts;
    t.targets = targets;
    t.sp = 0;
    t.stack[t.sp++] = 0;
    while (more) {
        uint16_t entries[ASM_FLOW_ENTRIES];
        int n, i;

        while (t.sp > 0) {
            uint32_t pc = t.stack[--t.sp];

            trace_run(&t, pc);
        }
        more = false;
        if (!isa->indirect_entries)
            break;
        n = isa->indirect_entries(image, size, limit, starts, entries, ASM_FLOW_ENTRIES);
        for (i = 0; i < n; i++) {
            if (entries[i] < limit && !bit_get(starts, entries[i])) {
                follow(&t, entries[i], true);
                more = true;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  The lines of the source                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    char text[48];                      /* what goes after the label field */
    int  length;                        /* bytes of the image it covers    */
    const char *note;                   /* a note to put after it, or NULL */
} unit_t;

static bool plain_char(uint8_t c)       /* a letter, digit or sign that goes in a string */
{
    return c >= 0x20 && c < 0x7F && c != '"' && c != ';';
}

/* How many of the bytes from addr on are the same kind of data: zeros, or
 * printable characters. Both stop at the start of code. */
static uint32_t zero_run(const uint8_t *image, uint32_t limit, const uint8_t *starts,
                         uint32_t addr)
{
    uint32_t n = 0;

    while (addr + n < limit && image[addr + n] == 0 && !bit_get(starts, addr + n))
        n++;
    return n;
}

static uint32_t text_run(const uint8_t *image, uint32_t limit, const uint8_t *starts,
                         uint32_t addr)
{
    uint32_t n = 0;

    while (addr + n < limit && plain_char(image[addr + n]) && !bit_get(starts, addr + n))
        n++;
    return n;
}

typedef struct {
    const isa_t   *isa;
    const uint8_t *image;
    uint32_t       size, limit;
    const uint8_t *targets;             /* addresses a branch goes to              */
    const uint8_t *code;                /* addresses where code was found          */
} source_t;

/* The line at addr: an instruction if the trace found one there, otherwise a few
 * bytes of data (DB), a string, or a run of zeros (DS). line_starts and reach
 * decide which branch targets are written as labels: only an address that starts
 * a line and is below reach. line_starts is NULL in the dry run, where every
 * marked target is taken to have a label (the longest way to write it).
 *
 * Author: Thomas Dzubin */
static void next_unit(const source_t *s, const uint8_t *line_starts, uint32_t reach,
                      uint32_t addr, unit_t *u)
{
    u->note = NULL;
    if (bit_get(s->code, addr)) {
        uint8_t b[ISA_MAX_BYTES];
        isa_flow_t f;
        char target_text[16];
        const char *target = NULL;

        fetch(s->image, s->size, addr, b);
        s->isa->flow((uint16_t)addr, b, &f);
        if (f.length < 1)
            f.length = 1;
        u->length = f.length;
        if (f.kind == ISA_FLOW_DATA || f.kind == ISA_FLOW_BAD) {    /* these bytes as data */
            size_t pos = (size_t)snprintf(u->text, sizeof u->text, "DB");
            char num[12];
            int i;

            for (i = 0; i < f.length && pos < sizeof u->text - 8; i++) {
                srcgen_hex(s->isa, num, sizeof num, b[i], 2);
                pos += (size_t)snprintf(u->text + pos, sizeof u->text - pos, " %s", num);
            }
            return;
        }
        if (f.target >= 0 && (uint32_t)f.target < s->size &&
            bit_get(s->targets, (uint32_t)f.target) &&
            (line_starts == NULL || ((uint32_t)f.target < reach &&
                                     bit_get(line_starts, (uint32_t)f.target)))) {
            snprintf(target_text, sizeof target_text, ASM_LABEL_PREFIX "%04X",
                     (unsigned)((uint32_t)f.target & 0xFFFFu));
            target = target_text;
        }
        s->isa->disasm((uint16_t)addr, b, target, u->text, sizeof u->text);
        if (s->isa->note)
            u->note = s->isa->note(b);
        return;
    }
    {
        uint32_t zeros = zero_run(s->image, s->limit, s->code, addr);
        uint32_t chars = text_run(s->image, s->limit, s->code, addr);
        uint32_t n = 0;
        size_t pos;

        if (zeros >= ASM_DS_MIN) {
            if (zeros > ASM_DS_MAX)
                zeros = ASM_DS_MAX;
            char num[12];

            srcgen_hex(s->isa, num, sizeof num, (unsigned)zeros, 1);
            snprintf(u->text, sizeof u->text, "DS   %s", num);
            u->length = (int)zeros;
            return;
        }
        if (chars >= ASM_TEXT_MIN) {
            if (chars > ASM_TEXT_MAX)
                chars = ASM_TEXT_MAX;
            pos = (size_t)snprintf(u->text, sizeof u->text, "DB   \"");
            memcpy(u->text + pos, s->image + addr, chars);
            u->text[pos + chars] = '"';
            u->text[pos + chars + 1] = '\0';
            u->length = (int)chars;
            return;
        }
        pos = (size_t)snprintf(u->text, sizeof u->text, "DB   ");
        while (n < ASM_DB_BYTES && addr + n < s->limit &&
               (n == 0 || !bit_get(s->code, addr + n))) {
            if (n > 0 && (zero_run(s->image, s->limit, s->code, addr + n) >= ASM_DS_MIN ||
                          text_run(s->image, s->limit, s->code, addr + n) >= ASM_TEXT_MIN))
                break;
            char num[12];

            srcgen_hex(s->isa, num, sizeof num, s->image[addr + n], 2);
            pos += (size_t)snprintf(u->text + pos, sizeof u->text - pos, "%s%s",
                                    n ? "," : "", num);
            n++;
        }
        u->length = (int)n;
    }
}

/* Append text to out; false when it will not fit (out is left as it was). */
static bool put(char *out, size_t out_size, size_t *length, const char *text)
{
    size_t n = strlen(text);

    if (*length + n + 1 > out_size)
        return false;
    memcpy(out + *length, text, n + 1);
    *length += n;
    return true;
}

/* The line that starts the source. */
static bool put_org(const isa_t *isa, char *out, size_t out_size, size_t *length)
{
    char num[12];
    char line[24];

    srcgen_hex(isa, num, sizeof num, 0, 4);
    snprintf(line, sizeof line, "ORG %s\n", num);
    return put(out, out_size, length, line);
}

/* One line of the source: label field, instruction, and a note if it has one. */
static void source_line(char *line, size_t size, const char *label, const unit_t *u)
{
    if (u->note)
        snprintf(line, size, "%-7s %-*s ; %s\n", label, ASM_NOTE_COLUMN, u->text, u->note);
    else
        snprintf(line, size, "%-7s %s\n", label, u->text);
}

static size_t line_length(const unit_t *u)
{
    size_t text = strlen(u->text);

    if (u->note)
        return 8 + (text > ASM_NOTE_COLUMN ? text : ASM_NOTE_COLUMN) + 3 + strlen(u->note) + 1;
    return 8 + text + 1;
}

static uint8_t g_targets[ASM_IMAGE_SIZE / 8];       /* branch targets             */
static uint8_t g_code[ASM_IMAGE_SIZE / 8];          /* addresses the trace found code at */
static uint8_t g_starts[ASM_IMAGE_SIZE / 8];        /* addresses that start a line */

static uint32_t code_limit(const uint8_t *image, uint32_t size)
{
    uint32_t limit = size;

    while (limit > 0 && image[limit - 1] == 0)
        limit--;
    return limit;
}

/* Author: Thomas Dzubin */
size_t srcgen_from_image(const isa_t *isa, const uint8_t *image, uint32_t size,
                         char *out, size_t out_size, uint32_t *done)
{
    source_t s;
    uint32_t addr = 0;
    uint32_t reach;
    size_t length = 0;
    size_t room;
    char line[96];

    out[0] = '\0';
    s.isa = isa;
    s.image = image;
    s.size = size;
    s.limit = code_limit(image, size);
    s.targets = g_targets;
    s.code = g_code;
    find_code(isa, image, size, s.limit, g_code, g_targets);
    memset(g_starts, 0, sizeof g_starts);
    *done = 0;
    if (!put_org(isa, out, out_size, &length))
        return length;

    /* A dry run first, to see how far the text will get if every branch has a
     * label, and which addresses start a line. Only a target below that point
     * and on a line start gets a label. */
    room = out_size - length;
    while (addr < s.limit) {
        unit_t u;
        size_t n;

        next_unit(&s, NULL, 0, addr, &u);
        n = line_length(&u);
        if (n + 1 > room)
            break;
        room -= n;
        bit_set(g_starts, addr);
        addr += (uint32_t)u.length;
    }
    reach = addr;

    for (addr = 0; addr < reach; ) {
        unit_t u;
        char label[16] = "";

        next_unit(&s, g_starts, reach, addr, &u);
        if (bit_get(g_targets, addr))
            snprintf(label, sizeof label, ASM_LABEL_PREFIX "%04X:", (unsigned)(addr & 0xFFFFu));
        source_line(line, sizeof line, label, &u);
        if (!put(out, out_size, &length, line))
            break;
        addr += (uint32_t)u.length;
    }
    *done = addr < s.limit ? addr : s.limit;
    return length;
}

/* ------------------------------------------------------------------ */
/*  The whole listing, in pieces                                        */
/* ------------------------------------------------------------------ */
static source_t l_source;                           /* the listing being made     */
static uint32_t l_addr;
static bool     l_header;                           /* the ORG line is out        */

/* Author: Thomas Dzubin */
void srcgen_listing_start(const isa_t *isa, const uint8_t *image, uint32_t size)
{
    uint32_t addr = 0;

    l_source.isa = isa;
    l_source.image = image;
    l_source.size = size;
    l_source.limit = code_limit(image, size);
    l_source.targets = g_targets;
    l_source.code = g_code;
    find_code(isa, image, size, l_source.limit, g_code, g_targets);

    /* every line starts somewhere: the same walk as the dry run, with no end */
    memset(g_starts, 0, sizeof g_starts);
    while (addr < l_source.limit) {
        unit_t u;

        next_unit(&l_source, NULL, 0, addr, &u);
        bit_set(g_starts, addr);
        addr += (uint32_t)u.length;
    }
    l_addr = 0;
    l_header = false;
}

/* Author: Thomas Dzubin */
size_t srcgen_listing_next(char *out, size_t out_size, bool *finished)
{
    size_t length = 0;
    char line[96];

    out[0] = '\0';
    if (!l_header) {
        if (!put_org(l_source.isa, out, out_size, &length))
            return 0;
        l_header = true;
    }
    while (l_addr < l_source.limit) {
        unit_t u;
        char label[16] = "";

        next_unit(&l_source, g_starts, l_source.limit, l_addr, &u);
        if (bit_get(g_targets, l_addr))
            snprintf(label, sizeof label, ASM_LABEL_PREFIX "%04X:", (unsigned)(l_addr & 0xFFFFu));
        source_line(line, sizeof line, label, &u);
        if (!put(out, out_size, &length, line))
            break;
        l_addr += (uint32_t)u.length;
    }
    *finished = l_addr >= l_source.limit;
    return length;
}

/* ------------------------------------------------------------------ */
/*  Targets, for a display                                              */
/* ------------------------------------------------------------------ */
void srcgen_mark_targets(const isa_t *isa, const uint8_t *image, uint32_t size,
                         uint8_t *marks)
{
    uint32_t limit = code_limit(image, size);
    uint32_t addr = 0;

    memset(marks, 0, size / 8);
    while (addr < limit) {
        uint8_t b[ISA_MAX_BYTES];
        isa_flow_t f;

        fetch(image, size, addr, b);
        isa->flow((uint16_t)addr, b, &f);
        if (f.target >= 0 && (uint32_t)f.target < size)
            bit_set(marks, (uint32_t)f.target);
        addr += f.length < 1 ? 1u : (uint32_t)f.length;
    }
}
