/*
 * asm_core.c - the assembler for any processor. See asm_core.h.
 *
 * Two passes over the source: the first finds the address of every label (so a
 * branch can go forward), the second makes the bytes. The processor's isa_t
 * turns an instruction line into bytes.
 *
 * Author: Thomas Dzubin
 */
#include <stdio.h>
#include <string.h>

#include "asm_core.h"
#include "asm_const.h"

/* ------------------------------------------------------------------ */
/*  Characters and numbers                                              */
/* ------------------------------------------------------------------ */
char asm_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool is_letter(char c)   { c = asm_upper(c); return c >= 'A' && c <= 'Z'; }
bool asm_is_digit(char c)       { return c >= '0' && c <= '9'; }
bool asm_is_name_start(char c)  { return is_letter(c) || c == '_'; }
bool asm_is_name_char(char c)   { return asm_is_name_start(c) || asm_is_digit(c); }

int asm_hex_digit(char c)
{
    c = asm_upper(c);
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

const char *asm_skip_space(const char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

/* ------------------------------------------------------------------ */
/*  Errors and the symbol table                                         */
/* ------------------------------------------------------------------ */
void asm_error(asm_t *a, const char *text)
{
    asm_line_t *l = &a->lines[a->current];

    if (l->flags & ASM_LINE_BAD)
        return;
    l->flags |= ASM_LINE_BAD;
    if (a->error_count < ASM_MAX_ERRORS) {
        a->errors[a->error_count].line = (uint16_t)(a->current + 1);
        strncpy(a->errors[a->error_count].text, text, ASM_ERROR_TEXT_MAX);
        a->errors[a->error_count].text[ASM_ERROR_TEXT_MAX] = '\0';
    }
    a->error_count++;
}

uint32_t asm_location(const asm_t *a) { return a->location; }
int      asm_pass(const asm_t *a)     { return a->pass; }

bool asm_forward(const asm_t *a)
{
    if (a->pass == 1)
        return a->unresolved;
    return (a->lines[a->current].flags & ASM_LINE_FORWARD) != 0;
}

static asm_symbol_t *find_symbol(asm_t *a, const char *name)
{
    int i;

    for (i = 0; i < a->symbol_count; i++)
        if (strcmp(a->symbols[i].name, name) == 0)
            return &a->symbols[i];
    return NULL;
}

/* In a processor whose numbers are hex by default, a name that can be read as a
 * number cannot be a label, since an operand would then mean two things. A name
 * that the processor keeps for itself (a register) cannot be one either. */
static bool name_is_reserved(const asm_t *a, const char *name)
{
    const char *p;

    if (a->isa->reserved && a->isa->reserved(name))
        return true;
    if (!(a->isa->numbers & ISA_NUM_HEX_DEFAULT))
        return false;
    for (p = name; *p; p++)
        if (asm_hex_digit(*p) < 0)
            return false;
    return true;
}

/* Author: Thomas Dzubin */
static void define_symbol(asm_t *a, const char *name, uint16_t value)
{
    asm_symbol_t *s;

    if (a->pass != 1)
        return;
    if (name_is_reserved(a, name)) {
        asm_error(a, ASM_ERR_LABEL_NUM);
        return;
    }
    if (find_symbol(a, name)) {
        asm_error(a, ASM_ERR_DUPLICATE);
        return;
    }
    if (a->symbol_count >= ASM_MAX_SYMBOLS) {
        asm_error(a, ASM_ERR_SYMBOLS);
        return;
    }
    s = &a->symbols[a->symbol_count++];
    strncpy(s->name, name, ASM_NAME_MAX);
    s->name[ASM_NAME_MAX] = '\0';
    s->value = value;
}

/* ------------------------------------------------------------------ */
/*  Expressions                                                         */
/* ------------------------------------------------------------------ */
/* Read a word of letters, digits and underscores into word (in capitals);
 * returns its length (it is cut at size - 1 characters, enough to see that it
 * was too long). */
size_t asm_read_word(const char **pp, char *word, size_t size)
{
    const char *p = *pp;
    size_t n = 0;

    while (asm_is_name_char(*p)) {
        if (n + 1 < size)
            word[n++] = asm_upper(*p);
        p++;
    }
    word[n] = '\0';
    *pp = p;
    return n;
}

/* The value of a word of digits in a radix (2, 10 or 16), with an optional
 * suffix letter to ignore; false if it is not all digits of that radix. */
static bool word_value(const char *word, size_t n, unsigned radix, uint32_t *value)
{
    size_t i;
    uint32_t v = 0;

    if (n == 0)
        return false;
    for (i = 0; i < n; i++) {
        int d = asm_hex_digit(word[i]);

        if (d < 0 || (unsigned)d >= radix)
            return false;
        v = v * radix + (uint32_t)d;
        if (v > 0xFFFFu)
            v = 0x10000u;                   /* too big, stays too big          */
    }
    *value = v;
    return true;
}

/* A number or a name made of the word read, in the processor's spelling of
 * numbers; sets *is_number.
 *
 * Author: Thomas Dzubin */
static bool word_number(const asm_t *a, const char *word, uint32_t *value)
{
    unsigned num = a->isa->numbers;
    size_t n = strlen(word);

    if ((num & ISA_NUM_0X) && word[0] == '0' && word[1] == 'X' && n > 2)
        return word_value(word + 2, n - 2, 16, value);
    if (num & ISA_NUM_HEX_DEFAULT) {                    /* 2F, FF, 2FH */
        if (n > 1 && word[n - 1] == 'H' && (num & ISA_NUM_H_SUFFIX) &&
            word_value(word, n - 1, 16, value))
            return true;
        return word_value(word, n, 16, value);
    }
    if (!asm_is_digit(word[0]))
        return false;
    if ((num & ISA_NUM_H_SUFFIX) && word[n - 1] == 'H')
        return word_value(word, n - 1, 16, value);
    if ((num & ISA_NUM_B_SUFFIX) && word[n - 1] == 'B' && word_value(word, n - 1, 2, value))
        return true;
    return word_value(word, n, 10, value);
}

/* One value: a number, a character, the location counter (*), or a label.
 *
 * Author: Thomas Dzubin */
static bool primary(asm_t *a, const char **pp, uint32_t *value)
{
    const char *p = asm_skip_space(*pp);
    unsigned num = a->isa->numbers;
    char word[ASM_NAME_MAX + 2];

    if (*p == '*') {
        *value = a->location;
        *pp = p + 1;
        return true;
    }
    if (*p == '\'') {
        if (p[1] == '\0' || p[2] != '\'') {
            asm_error(a, ASM_ERR_NUMBER);
            return false;
        }
        *value = (uint8_t)p[1];
        *pp = p + 3;
        return true;
    }
    if ((*p == '$' && (num & ISA_NUM_DOLLAR_HEX)) ||
        (*p == '#' && (num & ISA_NUM_HASH_DECIMAL)) ||
        (*p == '%' && (num & ISA_NUM_PERCENT_BINARY))) {
        unsigned radix = *p == '$' ? 16u : (*p == '#' ? 10u : 2u);
        uint32_t v = 0;
        int digits = 0;

        for (p++; asm_hex_digit(*p) >= 0 && (unsigned)asm_hex_digit(*p) < radix; p++, digits++) {
            v = v * radix + (uint32_t)asm_hex_digit(*p);
            if (v > 0xFFFFu)
                v = 0x10000u;
        }
        if (digits == 0) {
            asm_error(a, ASM_ERR_NUMBER);
            return false;
        }
        if (v > 0xFFFFu) {
            asm_error(a, ASM_ERR_BIG);
            return false;
        }
        *value = v;
        *pp = p;
        return true;
    }
    if (asm_is_name_char(*p)) {
        uint32_t v;

        asm_read_word(&p, word, sizeof word);
        *pp = p;
        if (word_number(a, word, &v)) {
            if (v > 0xFFFFu) {
                asm_error(a, ASM_ERR_BIG);
                return false;
            }
            *value = v;
            return true;
        }
        if (asm_is_digit(word[0]) || (word[0] == '0' && word[1] == 'X')) {   /* 2G: not a number */
            asm_error(a, ASM_ERR_NUMBER);
            return false;
        } else {
            const asm_symbol_t *s = find_symbol(a, word);

            if (s) {
                *value = s->value;
            } else {
                a->unresolved = true;
                *value = 0;
                if (a->pass == 2) {
                    asm_error(a, ASM_ERR_UNDEFINED);
                    return false;
                }
            }
            return true;
        }
    }
    asm_error(a, ASM_ERR_OPERAND);
    return false;
}

/* An expression: [< or >] [-] value { + or - value }, with no spaces before an
 * operator. The result is 16 bits (a negative number wraps).
 *
 * Author: Thomas Dzubin */
bool asm_expression(asm_t *a, const char **pp, uint32_t *value)
{
    const char *p = asm_skip_space(*pp);
    char shift = 0;
    bool negative = false;
    uint32_t v;
    uint32_t total;

    if (*p == '<' || *p == '>') {
        shift = *p;
        p = asm_skip_space(p + 1);
    }
    if (*p == '-') {
        negative = true;
        p++;
    }
    if (!primary(a, &p, &v))
        return false;
    total = negative ? (uint32_t)(-(int32_t)v) : v;
    while (*p == '+' || *p == '-') {
        char op = *p++;

        if (!primary(a, &p, &v))
            return false;
        total = (op == '+') ? total + v : total - v;
    }
    total &= 0xFFFFu;
    if (shift == '<')
        total &= 0xFFu;
    else if (shift == '>')
        total >>= 8;
    *value = total;
    *pp = p;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Making the bytes                                                    */
/* ------------------------------------------------------------------ */
/* Author: Thomas Dzubin */
void asm_emit(asm_t *a, uint8_t byte)
{
    asm_line_t *l = &a->lines[a->current];
    uint32_t at = a->location - a->base;        /* huge when below the window */

    if (a->location < a->base || at >= a->size) {
        asm_error(a, ASM_ERR_RAM);
        a->location++;
        return;
    }
    if (a->pass == 2) {
        if ((a->used[at >> 3] >> (at & 7)) & 1u)
            asm_error(a, ASM_ERR_OVERLAP);
        a->image[at] = byte;
        a->used[at >> 3] |= (uint8_t)(1u << (at & 7));
        a->bytes++;
        if ((int)a->location < a->low || a->low < 0)
            a->low = (int)a->location;
        if ((int)a->location > a->high)
            a->high = (int)a->location;
        if (l->count < ASM_LISTING_BYTES)
            l->bytes[l->count] = byte;
        l->count++;
    }
    a->location++;
}

bool asm_byte_value(asm_t *a, uint32_t v, uint8_t *out)
{
    if (v > 0xFFu && v < 0xFF80u) {
        asm_error(a, ASM_ERR_BIG);
        return false;
    }
    *out = (uint8_t)v;
    return true;
}

bool asm_at_end(asm_t *a, const char *p)
{
    if (*asm_skip_space(p) != '\0') {
        asm_error(a, ASM_ERR_EXTRA);
        return false;
    }
    return true;
}

/* DB: bytes, with "text" and 'c' allowed, separated by commas or spaces. DW:
 * 16-bit words, in the processor's byte order.
 *
 * Author: Thomas Dzubin */
static void data_list(asm_t *a, const char *p, bool words)
{
    bool any = false;

    for (;;) {
        uint32_t v;

        p = asm_skip_space(p);
        while (*p == ',')
            p = asm_skip_space(p + 1);
        if (*p == '\0')
            break;
        any = true;
        if (*p == '"' && !words) {
            for (p++; *p && *p != '"'; p++)
                asm_emit(a, (uint8_t)*p);
            if (*p != '"') {
                asm_error(a, ASM_ERR_STRING);
                return;
            }
            p++;
            continue;
        }
        if (!asm_expression(a, &p, &v))
            return;
        if (words) {
            if (a->isa->big_endian_words) {
                asm_emit(a, (uint8_t)(v >> 8));
                asm_emit(a, (uint8_t)(v & 0xFFu));
            } else {
                asm_emit(a, (uint8_t)(v & 0xFFu));
                asm_emit(a, (uint8_t)(v >> 8));
            }
        } else {
            uint8_t b;

            if (!asm_byte_value(a, v, &b))
                return;
            asm_emit(a, b);
        }
    }
    if (!any)
        asm_error(a, ASM_ERR_MISSING);
}

/* ------------------------------------------------------------------ */
/*  A source line                                                       */
/* ------------------------------------------------------------------ */
/* Cut the comment off a line (a semicolon outside quotes). */
static void strip_comment(char *line)
{
    char *p;
    char quote = 0;

    for (p = line; *p; p++) {
        if (quote) {
            if (*p == quote)
                quote = 0;
        } else if (*p == '"' || (*p == '\'' && p[1] && p[2] == '\'')) {
            quote = *p;
        } else if (*p == ';') {
            *p = '\0';
            return;
        }
    }
}

/* A mnemonic: a word, and a suffix after a dot (LDA.W). */
static void read_mnemonic(const char **pp, char *word, size_t size)
{
    size_t n = asm_read_word(pp, word, size);

    if (**pp == '.' && n + 1 < size) {
        word[n++] = '.';
        (*pp)++;
        while (asm_is_name_char(**pp)) {
            if (n + 1 < size)
                word[n++] = asm_upper(**pp);
            (*pp)++;
        }
        word[n] = '\0';
    }
}

/* Does the value of a directive's operand have to be known? (ORG, DS, EQU) */
static bool known_value(asm_t *a, const char **p, uint32_t *v)
{
    if (!asm_expression(a, p, v) || !asm_at_end(a, *p))
        return false;
    if (a->pass == 1 && a->unresolved) {
        asm_error(a, ASM_ERR_KNOWN);
        return false;
    }
    return true;
}

/* Author: Thomas Dzubin */
static void assemble_line(asm_t *a, char *line)
{
    const char *p;
    char label[ASM_NAME_MAX + 2];
    char word[ASM_NAME_MAX + 2];
    bool have_label = false;
    uint32_t v;

    a->lines[a->current].address = (uint16_t)a->location;
    if (a->ended)
        return;
    strip_comment(line);
    p = asm_skip_space(line);
    if (*p == '\0')
        return;

    /* a label: NAME: */
    if (asm_is_name_start(*p)) {
        const char *q = p;
        size_t n = asm_read_word(&q, label, sizeof label);

        if (*q == ':') {
            if (n > ASM_NAME_MAX) {
                asm_error(a, ASM_ERR_LABEL);
                return;
            }
            have_label = true;
            p = asm_skip_space(q + 1);
        }
    } else {
        asm_error(a, ASM_ERR_MNEMONIC);
        return;
    }

    /* NAME EQU value (the name has no colon) */
    if (!have_label) {
        const char *q = p;
        char second[ASM_NAME_MAX + 2];

        asm_read_word(&q, label, sizeof label);
        q = asm_skip_space(q);
        asm_read_word(&q, second, sizeof second);
        if (strcmp(second, ASM_DIR_EQU) == 0) {
            if (strlen(label) > ASM_NAME_MAX) {
                asm_error(a, ASM_ERR_LABEL);
                return;
            }
            p = asm_skip_space(q);
            if (known_value(a, &p, &v))
                define_symbol(a, label, (uint16_t)v);
            return;
        }
    }

    if (*p == '\0') {                                   /* only a label */
        define_symbol(a, label, (uint16_t)a->location);
        return;
    }
    if (!asm_is_name_start(*p)) {
        if (have_label)
            define_symbol(a, label, (uint16_t)a->location);
        asm_error(a, ASM_ERR_MNEMONIC);
        return;
    }
    read_mnemonic(&p, word, sizeof word);
    p = asm_skip_space(p);

    /* LABEL: EQU value */
    if (have_label && strcmp(word, ASM_DIR_EQU) == 0) {
        if (known_value(a, &p, &v))
            define_symbol(a, label, (uint16_t)v);
        return;
    }

    /* ORG: the label (if any) is the new address */
    if (strcmp(word, ASM_DIR_ORG) == 0) {
        if (!known_value(a, &p, &v))
            return;
        a->location = v;
        a->lines[a->current].address = (uint16_t)v;
        if (have_label)
            define_symbol(a, label, (uint16_t)v);
        return;
    }
    if (have_label)
        define_symbol(a, label, (uint16_t)a->location);

    if (strcmp(word, ASM_DIR_END) == 0) {
        a->ended = true;
        return;
    }
    if (strcmp(word, ASM_DIR_DB) == 0 || strcmp(word, ASM_DIR_FCB) == 0) {
        data_list(a, p, false);
        return;
    }
    if (strcmp(word, ASM_DIR_DW) == 0 || strcmp(word, ASM_DIR_FDB) == 0) {
        data_list(a, p, true);
        return;
    }
    if (strcmp(word, ASM_DIR_DS) == 0 || strcmp(word, ASM_DIR_RMB) == 0) {
        if (known_value(a, &p, &v))
            a->location += v;
        return;
    }
    if (!a->isa->encode(a, word, p))
        asm_error(a, ASM_ERR_MNEMONIC);
}

/* Author: Thomas Dzubin */
int asm_assemble(const isa_t *isa, asm_t *a, uint32_t base, uint32_t size,
                 const char *source, size_t length)
{
    char line[ASM_LINE_MAX + 2];
    size_t i;
    int pass;

    a->isa = isa;
    a->base = base;
    a->size = size < ASM_IMAGE_SIZE ? size : ASM_IMAGE_SIZE;
    memset(a->image, 0, sizeof a->image);
    memset(a->used, 0, sizeof a->used);
    memset(a->lines, 0, sizeof a->lines);
    a->line_count = 0;
    a->symbol_count = 0;
    a->error_count = 0;
    a->bytes = 0;
    a->low = -1;
    a->high = -1;

    for (pass = 1; pass <= 2; pass++) {
        size_t n = 0;                   /* characters in the line so far        */
        bool too_long = false;

        a->pass = pass;
        a->location = 0;
        a->ended = false;
        a->current = 0;
        for (i = 0; i <= length; i++) {
            char c = i < length ? source[i] : '\n';

            if (c == '\r')
                continue;
            if (c != '\n') {
                if (n < ASM_LINE_MAX)
                    line[n++] = c;
                else
                    too_long = true;
                continue;
            }
            if (i == length && n == 0 && !too_long)
                break;                  /* no line after the last line feed    */
            if (a->current >= ASM_MAX_LINES) {
                a->current = ASM_MAX_LINES - 1;
                asm_error(a, ASM_ERR_LINES);
                break;
            }
            line[n] = '\0';
            a->unresolved = false;
            if (too_long)
                asm_error(a, ASM_ERR_LONG);
            assemble_line(a, line);
            if (pass == 1 && a->unresolved)
                a->lines[a->current].flags |= ASM_LINE_FORWARD;
            a->current++;
            n = 0;
            too_long = false;
        }
        if (pass == 1)
            a->line_count = a->current;
    }
    return a->error_count;
}
