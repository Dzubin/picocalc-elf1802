/*
 * platform_pico_const.h - constants for platform_pico.c: leaving the program
 * for the PicoCalc UF2 Loader (pelrun/uf2loader).
 *
 * The loader has no call for an app to use, but its own menu hands commands to
 * its start-up code through the chip's watchdog scratch registers, which
 * survive a watchdog reboot: scratch 0 holds a magic number, 1 the boot mode,
 * 2 an argument. Asking for boot mode "SD" and then rebooting makes the loader
 * show its menu again. If the program was flashed straight to the chip with no
 * loader, nothing reads the request and the program simply restarts.
 *
 * Author: Thomas Dzubin
 */
#ifndef PLATFORM_PICO_CONST_H
#define PLATFORM_PICO_CONST_H

#define LOADER_COMMAND_MAGIC      0xE98CC638u /* PICOCALC_BL_MAGIC in the loader's proginfo.h */
#define LOADER_BOOT_MODE_SD       1           /* BOOT_SD: load the menu from the SD card */
#define LOADER_SCRATCH_MAGIC      0
#define LOADER_SCRATCH_MODE       1
#define LOADER_SCRATCH_ARGUMENT   2

/* The watchdog reboot happens this many ms after it is requested. */
#define LOADER_REBOOT_DELAY_MS    10

/* Sound. The speaker amplifier is fed by PWM on GP26 (left) and GP27 (right),
 * which share one PWM slice. The slice free-runs with a wrap of 1023 (a
 * carrier of 120 to 150 kHz, far above hearing); a DMA channel, paced by a DMA
 * timer at the sample rate, copies one sample after another from a ring buffer
 * into the slice's level register (left in the low half, right in the high
 * half), so the processor only has to fill the ring. */
#define AUDIO_PIN_LEFT            26
#define AUDIO_PIN_RIGHT           27
#define AUDIO_PWM_WRAP            1023
#define AUDIO_RING_BITS           11          /* 2048 samples, about 93 ms     */
#define AUDIO_RING_SIZE           (1u << AUDIO_RING_BITS)
#define AUDIO_RING_MASK           (AUDIO_RING_SIZE - 1u)
#define AUDIO_LEVEL_OFFSET        32768       /* a signed sample made unsigned */
#define AUDIO_LEVEL_SHIFT         6           /* 16 bits down to the 10 of the wrap */
#define AUDIO_DMA_COUNT           0x0FFFFFFFu /* transfers per start (3 hours);
                                                 the player starts it again    */

#endif /* PLATFORM_PICO_CONST_H */
