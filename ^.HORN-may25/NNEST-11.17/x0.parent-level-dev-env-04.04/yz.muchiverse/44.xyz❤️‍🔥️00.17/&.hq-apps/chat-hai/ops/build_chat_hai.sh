#!/bin/sh
# build_chat_hai.sh — build the chat-hai side-bar renderer
# (chat_hai_hq_render.c), on the SAME pattern as build_db_hq.sh: sync the
# shared khtpm_css_parser/stb sources from &.widgits/_shared-lib plus the
# taskbar manager from *.monads/*.livedesk-taskbar/ops, then link one
# standalone binary. chat-hai is its own process, so this never touches
# the taskbar's own binaries.
set -e
cd "$(dirname "$0")"
mkdir -p +x lib
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 $(pkg-config --libs xft) -lm"

HOUSE="$(cd ../../../ && pwd)"
SHARED="$(cd "$HOUSE/&.widgits/_shared-lib" && pwd)"
LIVEDESK="$(cd "$HOUSE/*.monads/*.livedesk-taskbar/ops" && pwd)"

# Sync shared files from the single canonical source (same dedup pass as
# build_db_hq.sh).
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h
cp "$SHARED/stb_image_write.h" lib/stb_image_write.h
cp "$LIVEDESK/khtpm_taskbar_manager.c" khtpm_taskbar_manager.c
cp "$LIVEDESK/khtpm_taskbar_manager.h" khtpm_taskbar_manager.h

echo "-- chat-hai renderer -> +x/chat_hai_hq_render.+x"
$CC $CFLAGS -o +x/chat_hai_hq_render.+x \
  chat_hai_hq_render.c khtpm_css_parser.c khtpm_taskbar_manager.c $LIBS

echo "OK +x/chat_hai_hq_render.+x"
