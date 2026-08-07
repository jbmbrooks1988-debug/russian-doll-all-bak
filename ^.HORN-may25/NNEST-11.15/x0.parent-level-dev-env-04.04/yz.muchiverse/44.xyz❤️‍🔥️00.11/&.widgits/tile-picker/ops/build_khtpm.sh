#!/bin/sh
# Build shared KHTPM entity + taskbar (Linux). Design = khtpm_*_core only.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- entity (core + plat_x11)"
$CC $CFLAGS -o +x/tp_desktop_window.+x \
  khtpm_main.c khtpm_core.c khtpm_plat_x11.c \
  -lX11 -lXext -lGL -lm

echo "-- taskbar (core + plat_x11)"
$CC $CFLAGS -o +x/tp_taskbar.+x \
  khtpm_taskbar_main.c khtpm_taskbar_core.c khtpm_taskbar_plat_x11.c \
  -lX11 -lm

# also install taskbar binary where crypts expects it
TB_DIR="../../livedesk-taskbar/ops/+x"
# path from tile-picker/ops is ../livedesk-taskbar
TB_DIR="../livedesk-taskbar/ops/+x"
if [ ! -d "$TB_DIR" ]; then
  TB_DIR="../../livedesk-taskbar/ops/+x"
fi
# relative from &.widgits/tile-picker/ops -> &.widgits/livedesk-taskbar/ops/+x
TB_DIR="../livedesk-taskbar/ops/+x"
mkdir -p "$TB_DIR"
cp -f +x/tp_taskbar.+x "$TB_DIR/tp_taskbar.+x"
echo "OK $TB_DIR/tp_taskbar.+x"
echo "build_khtpm ok"
