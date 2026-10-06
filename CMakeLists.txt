# Elf1802  (by Thomas Dzubin)
#
# Reuses the drivers from ./picocalc-text-starter-main (lcd, southbridge, SD
# card, FAT32), which is picocalc-text-starter by Blair Leduc, plus the
# Raspberry Pi Pico SDK. The sound is the project's own (platform_pico.c).
# Build it with the Raspberry Pi Pico VS Code extension, or from a terminal that
# has PICO_SDK_PATH set (see README.md).

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in the Pico SDK (must be before project())
include(pico_sdk_import.cmake)

project(picocalc-elf1802 C CXX ASM)

pico_sdk_init()

# Location of the picocalc-text-starter drivers we build against
set(STARTER ${CMAKE_CURRENT_LIST_DIR}/picocalc-text-starter-main)

add_executable(picocalc-elf1802
    elf1802.c
    ui.c
    title.c
    helpscreen.c
    filemenu.c
    debugger.c
    picture.c
    vdu.c
    regview.c
    asm_core.c
    isa_1802.c
    source_gen.c
    editor.c
    elfpanel.c
    cpu1802.c
    cdp1861.c
    elf.c
    rom.c
    target.c
    membus.c
    hexfile.c
    qaudio.c
    disasm1802.c
    platform_pico.c
    ${STARTER}/drivers/lcd.c
    ${STARTER}/drivers/southbridge.c
    ${STARTER}/drivers/sdcard.c
    ${STARTER}/drivers/fat32.c
    ${STARTER}/drivers/font-8x10.c
    ${STARTER}/drivers/font-5x10.c
)

pico_set_program_name(picocalc-elf1802 "picocalc-elf1802")

# The program version comes from the VERSION line in elf1802_const.h, so it is
# written down in one place only (a change there re-runs CMake).
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/elf1802_const.h VERSION_LINE
     REGEX "^#define VERSION ")
string(REGEX REPLACE "^#define VERSION \"V?([^\"]*)\".*" "\\1" PROGRAM_VERSION "${VERSION_LINE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/elf1802_const.h)
pico_set_program_version(picocalc-elf1802 "${PROGRAM_VERSION}")

# No serial console needed; keep USB free so ~ -> BOOTSEL is clean
pico_enable_stdio_uart(picocalc-elf1802 0)
pico_enable_stdio_usb(picocalc-elf1802 0)

target_compile_options(picocalc-elf1802 PRIVATE -Wall)

target_include_directories(picocalc-elf1802 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${STARTER}/drivers
)

target_link_libraries(picocalc-elf1802
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_pwm
    hardware_dma
    hardware_clocks
    hardware_watchdog
)

# The chip is part of every output file name (elf1802-RP2040.uf2 /
# elf1802-RP2350.uf2, and the matching .elf/.bin/.hex), so a downloaded file
# always says which board it was built for.
if(PICO_PLATFORM MATCHES "^rp2040")
    set(CHIP_NAME RP2040)
elseif(PICO_PLATFORM MATCHES "^rp2350")
    set(CHIP_NAME RP2350)
else()
    set(CHIP_NAME ${PICO_PLATFORM})
endif()
# An experiment: with -DCOPY_TO_RAM=ON the whole program is copied to RAM when it
# starts and runs from there, not from flash through the 16 KB instruction cache.
# It costs about 67 KB of RAM. The file is named elf1802-<chip>-ram.uf2 so it
# cannot be mistaken for the normal build, and the EMU figure on the registers
# screen (the share of the time spent running the machine) shows what it saved.
option(COPY_TO_RAM "Run the program from RAM instead of flash" OFF)
set(RAM_SUFFIX "")
if(COPY_TO_RAM)
    pico_set_binary_type(picocalc-elf1802 copy_to_ram)
    set(RAM_SUFFIX "-ram")
endif()
set_target_properties(picocalc-elf1802 PROPERTIES OUTPUT_NAME elf1802-${CHIP_NAME}${RAM_SUFFIX})

pico_add_extra_outputs(picocalc-elf1802)

# Copy the finished .uf2 (same name) to the top-level project folder, so the
# RP2040 and RP2350 builds can sit side by side.
add_custom_command(TARGET picocalc-elf1802 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:picocalc-elf1802>/$<TARGET_FILE_BASE_NAME:picocalc-elf1802>.uf2
            ${CMAKE_CURRENT_LIST_DIR}/
)
