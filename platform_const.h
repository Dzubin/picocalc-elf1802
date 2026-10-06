/*
 * platform_const.h - constants shared by platform.h and its implementations:
 * where saved programs live and how they are named. Nothing here depends on
 * the machine.
 *
 * Author: Thomas Dzubin
 */
#ifndef PLATFORM_CONST_H
#define PLATFORM_CONST_H

#define PLAT_PROGRAM_DIR    "ELF1802"   /* folder for saved programs (on the
                                           SD card root, or beside the program
                                           on a PC)                          */
#define PLAT_PROGRAM_EXT    ".BIN"      /* a program is a raw memory image   */
#define PLAT_HEX_EXT        ".HEX"      /* or an Intel HEX text file         */
#define PLAT_SOURCE_EXT     ".ASM"      /* assembly source is a text file    */
#define PLAT_ROM_EXT        ".ROM"      /* a ROM image is a raw file         */

/* Which files plat_file_list() lists. */
#define PLAT_KIND_PROGRAM   0           /* .BIN images, .HEX files, .ASM source */
#define PLAT_KIND_SOURCE    1           /* .ASM source files                 */
#define PLAT_KIND_ROM       2           /* .ROM images                       */
#define PLAT_NAME_MAX       13          /* an 8.3 file name and its NUL      */
#define PLAT_PATH_MAX       40          /* "/ELF1802/" + a name, with room   */

/* Results of the plat_file_*() calls. */
#define PLAT_FILE_OK        0
#define PLAT_FILE_NO_CARD   1           /* no SD card (or not a FAT32 card)  */
#define PLAT_FILE_NOT_FOUND 2
#define PLAT_FILE_FAILED    3           /* any other error                   */

/* The sound: signed 16-bit mono samples, played at a fixed rate. The platform
 * keeps a short queue so that the program can hand samples over in bursts: it
 * starts with PLAT_AUDIO_PRIME samples of silence in hand (the cushion against
 * a late burst) and drops what would take the queue past PLAT_AUDIO_MAX_FILL
 * (so the sound never lags far behind the Elf). */
#define PLAT_AUDIO_RATE     22050
#define PLAT_AUDIO_PRIME    512
#define PLAT_AUDIO_MAX_FILL 1536

#endif /* PLATFORM_CONST_H */
