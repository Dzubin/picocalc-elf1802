/*
 * memview.h - a screen that shows the memory map of a machine: runs of RAM, ROM,
 * devices and empty address space, with the name of each ROM image fitted.
 *
 * Author: Thomas Dzubin
 */
#ifndef MEMVIEW_H
#define MEMVIEW_H

#include "membus.h"
#include "memview_const.h"

/* Show the map and wait for a key. */
void memview_run(const membus_t *bus);

#endif /* MEMVIEW_H */
