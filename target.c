/*
 * target.c - the program memory of a machine. See target.h.
 *
 * Author: Thomas Dzubin
 */
#include <string.h>

#include "target.h"

void program_target_load(const program_target_t *t, const uint8_t *data, uint32_t len)
{
    if (len > t->size)
        len = t->size;
    if (len > 0 && data != t->ram)              /* (the file may have been read into it) */
        memcpy(t->ram, data, len);
    memset(t->ram + len, 0, t->size - len);
    if (t->loaded)
        t->loaded(t->ctx);
}
