# Elf1802 - desktop (SDL2) build for Windows and Linux, by Thomas Dzubin.
#
# Builds the same elf1802.c as the firmware. Instead of the PicoCalc drivers it
# links the small SDL2 shim in this folder, which re-implements the
# picocalc-text-starter driver API (lcd / audio / south-bridge keyboard, by
# Blair Leduc) - see shim.h. The vendored starter's headers and fonts are used
# as-is. The shared build logic (SDL2, output names, copying the result) is in
# shim_desktop.cmake.
#
# Build on Windows (MSYS2 UCRT64 shell with SDL2 installed):
#     cmake -S desktop -B build-windows -G Ninja
#     ninja -C build-windows
# Output: elf1802-Windows.exe, copied to the top-level project folder. SDL2 is
# linked statically, so it is one self-contained file.
#
# Build on Linux (needs the SDL2 development package, e.g. libsdl2-dev):
#     cmake -S desktop -B build-linux
#     cmake --build build-linux
# Output: elf1802-Linux, copied to the top-level project folder together with
# libSDL2-2.0.so.0 (SDL2 is NOT linked in statically on Linux, so keep the two
# files side by side; the executable finds the library through an $ORIGIN
# run-path).

cmake_minimum_required(VERSION 3.13)

project(picocalc-elf1802-desktop C)

set(CMAKE_C_STANDARD 11)

get_filename_component(TOP ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
set(STARTER ${TOP}/picocalc-text-starter-main)

# Shared build logic (SDL2, output names, copying the result to the top level).
include(${CMAKE_CURRENT_LIST_DIR}/shim_desktop.cmake)

picocalc_desktop_app(
    NAME     elf1802
    TITLE    "Elf1802"
    NO_AUDIO
    SOURCES
        ${TOP}/elf1802.c
        ${TOP}/ui.c
        ${TOP}/title.c
        ${TOP}/helpscreen.c
        ${TOP}/filemenu.c
        ${TOP}/debugger.c
        ${TOP}/picture.c
        ${TOP}/vdu.c
        ${TOP}/regview.c
        ${TOP}/asm_core.c
        ${TOP}/isa_1802.c
        ${TOP}/source_gen.c
        ${TOP}/editor.c
        ${TOP}/elfpanel.c
        ${TOP}/cpu1802.c
        ${TOP}/cdp1861.c
        ${TOP}/elf.c
        ${TOP}/rom.c
        ${TOP}/target.c
        ${TOP}/membus.c
        ${TOP}/hexfile.c
        ${TOP}/qaudio.c
        ${TOP}/disasm1802.c
        ${CMAKE_CURRENT_LIST_DIR}/platform_desktop.c
        ${STARTER}/drivers/font-8x10.c
        ${STARTER}/drivers/font-5x10.c
    INCLUDES
        ${TOP}
        ${STARTER}/drivers
)
