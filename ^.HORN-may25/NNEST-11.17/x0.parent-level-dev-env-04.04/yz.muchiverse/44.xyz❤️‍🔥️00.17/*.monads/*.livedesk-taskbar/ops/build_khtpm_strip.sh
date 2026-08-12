#!/bin/sh
# build_khtpm_strip.sh — build the two-process strip architecture
# (khtpm_taskbar_manager_main.c manager driver + khtpm_strip_parser.c
# outer parser), per khtpm-strip-parser-design.md.
#
# PRODUCTION BINARY NAMES (2026-08-11): legacy tp_taskbar.c has been
# retired (archived to
# *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip, originals
# deleted — direct instruction: "id like to deprecate the old toolbar
# system now"). This khtpm pair is now the real, only taskbar — dropped
# the "_test" suffix these binaries carried through the whole build-out
# session, now that there's no live legacy binary left to avoid
# clobbering.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- khtpm manager driver (pure logic, no Xlib) -> +x/khtpm_taskbar_manager_main.+x"
$CC $CFLAGS -o +x/khtpm_taskbar_manager_main.+x \
  khtpm_taskbar_manager_main.c khtpm_taskbar_manager.c

echo "-- khtpm strip parser (Xlib + layout engine + hit-testing + manager fork/exec) -> +x/khtpm_strip_parser.+x"
$CC $CFLAGS -o +x/khtpm_strip_parser.+x \
  khtpm_strip_parser.c khtpm_strip_layout.c khtpm_taskbar_manager.c \
  -lX11 -lm

echo "OK +x/khtpm_taskbar_manager_main.+x and +x/khtpm_strip_parser.+x"
