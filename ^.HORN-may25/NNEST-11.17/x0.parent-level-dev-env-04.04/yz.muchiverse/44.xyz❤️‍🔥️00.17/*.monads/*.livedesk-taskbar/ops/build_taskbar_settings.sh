#!/bin/sh
# build_taskbar_settings.sh — build the taskbar HQ menu's "Settings" window
# (khtpm_taskbar_settings_render.c), a separate standalone binary/process,
# same shape as build_db_hq.sh - never touches the taskbar's own two
# binaries.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 $(pkg-config --libs xft) -lm"

echo "-- taskbar-settings renderer -> +x/khtpm_taskbar_settings_render.+x"
$CC $CFLAGS -o +x/khtpm_taskbar_settings_render.+x \
  khtpm_taskbar_settings_render.c $LIBS

echo "OK +x/khtpm_taskbar_settings_render.+x"
