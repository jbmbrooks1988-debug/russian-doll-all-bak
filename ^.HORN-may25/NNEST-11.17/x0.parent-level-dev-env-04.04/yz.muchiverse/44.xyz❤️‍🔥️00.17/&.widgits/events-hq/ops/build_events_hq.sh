#!/bin/sh
# build_events_hq.sh — build events-hq, mirrors *.livedesk-taskbar/ops/
# build_db_hq.sh's own shape (2do-rgb-hq.md's own convention call).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2 $(pkg-config --cflags xft)"
LIBS="-lX11 $(pkg-config --libs xft) -lm"

# Sync shared files from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md).
SHARED="$(cd "$(dirname "$0")/../../_shared-lib" && pwd)"
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h
mkdir -p lib
cp "$SHARED/stb_image_write.h" lib/stb_image_write.h

echo "-- events-hq renderer -> +x/khtpm_events_hq_render.+x"
$CC $CFLAGS -o +x/khtpm_events_hq_render.+x \
  khtpm_events_hq_render.c khtpm_css_parser.c $LIBS

echo "OK +x/khtpm_events_hq_render.+x"
