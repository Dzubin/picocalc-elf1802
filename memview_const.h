/*
 * memview_const.h - constants of the memory map screen (memview.c).
 *
 * Author: Thomas Dzubin
 */
#ifndef MEMVIEW_CONST_H
#define MEMVIEW_CONST_H

#define MEMVIEW_ROW_TITLE   1
#define MEMVIEW_ROW_FIRST   3
#define MEMVIEW_MAX_RUNS    24              /* rows of the map that fit        */
#define MEMVIEW_ROW_HINT    30

#define MEMVIEW_TITLE       "MEMORY MAP"
#define MEMVIEW_HINT        "ANY KEY: BACK"

/* The names of the kinds of memory, indexed by MEMBUS_KIND_*. */
#define MEMVIEW_KIND_NAMES  { "(NOTHING)", "RAM", "ROM", "DEVICE" }

#endif /* MEMVIEW_CONST_H */
