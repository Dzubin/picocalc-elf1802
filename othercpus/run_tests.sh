#!/bin/sh
# Build and run every test of the other processors (everything in this folder).
# Run it from the project folder:  sh othercpus/run_tests.sh
# The programs are built in a temporary folder that is removed afterwards.
# Author: Thomas Dzubin
set -e
FLAGS="-Wall -Wextra -I. -Iothercpus -DASM_IMAGE_SIZE=0x5000"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
O=othercpus

run() {
    name=$1
    shift
    gcc $FLAGS "$@" -o "$OUT/$name"
    printf '%s: ' "$name"
    "$OUT/$name" | tail -1
}

run test_isa_others $O/tests/test_isa_others.c asm_core.c source_gen.c \
    $O/isa_6502.c $O/isa_8080.c $O/isa_z80.c $O/isa_6800.c $O/isa_6809.c
run test_cpu6803 $O/tests/test_cpu6803.c $O/cpu6803.c asm_core.c $O/isa_6800.c
run test_mc6847 $O/tests/test_mc6847.c $O/mc6847.c
run test_mc10 $O/tests/test_mc10.c $O/mc10.c $O/cpu6803.c $O/mc6847.c membus.c rom.c \
    qaudio.c asm_core.c $O/isa_6800.c
run test_cpuz80 $O/tests/test_cpuz80.c $O/cpuz80.c asm_core.c $O/isa_z80.c
