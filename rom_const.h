/*
 * rom_const.h - constants of the ROM images (rom.h).
 *
 * Author: Thomas Dzubin
 */
#ifndef ROM_CONST_H
#define ROM_CONST_H

#define ROM_SLOTS           2               /* ROM images that can be fitted         */
#define ROM_SLOT_SIZE       16384           /* the biggest, in bytes                 */
#define ROM_NAME_MAX        13              /* an 8.3 file name and its NUL          */
#define ROM_CONFIG_FILE     "ROMS.CFG"      /* which ROMs, and where, are remembered here */

/* Results (anything else is a PLAT_FILE_* result of reading the file). */
#define ROM_OK              0
#define ROM_ERR_ADDRESS     (-1)            /* not on a page, or past the end of memory */
#define ROM_ERR_SLOT        (-2)            /* there is no such slot                  */
#define ROM_ERR_EMPTY       (-3)            /* the file has nothing in it             */

#endif /* ROM_CONST_H */
