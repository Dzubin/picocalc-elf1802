/*
 * membus_const.h - constants of the memory map (membus.h).
 *
 * Author: Thomas Dzubin
 */
#ifndef MEMBUS_CONST_H
#define MEMBUS_CONST_H

#define MEMBUS_PAGE_SHIFT   8                       /* a page is 256 bytes        */
#define MEMBUS_PAGE_SIZE    (1u << MEMBUS_PAGE_SHIFT)
#define MEMBUS_PAGES        256                     /* 64K of address space       */
#define MEMBUS_SPACE        (MEMBUS_PAGE_SIZE * MEMBUS_PAGES)

#endif /* MEMBUS_CONST_H */
