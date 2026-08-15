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

# Sync shared files from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md for why this is a
# build-time copy, not a runtime shared include path).
SHARED="$(cd "$(dirname "$0")/../../../&.widgits/_shared-lib" && pwd)"
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h

echo "-- khtpm manager driver (pure logic, no Xlib) -> +x/khtpm_taskbar_manager_main.+x"
$CC $CFLAGS -o +x/khtpm_taskbar_manager_main.+x \
  khtpm_taskbar_manager_main.c khtpm_taskbar_manager.c

echo "-- khtpm strip parser (Xlib + layout engine + hit-testing + manager fork/exec) -> +x/khtpm_strip_parser.+x"
# REAL FIX 2026-08-13: now links Xft (same flags as build_db_hq.sh) -
# khtpm_strip_parser.c grew real XftDrawStringUtf8-based CJK/UTF-8
# text rendering this session (see that file's own header comment on
# its Xft include), replacing plain XDrawString which could only
# render Latin-1 correctly.
$CC $CFLAGS $(pkg-config --cflags xft) -o +x/khtpm_strip_parser.+x \
  khtpm_strip_parser.c khtpm_strip_layout.c khtpm_taskbar_manager.c \
  -lX11 $(pkg-config --libs xft) -lm

# 2026-08-14 consolidation: the livedesk entity renderer + its helper
# set moved OUT of &.widgits/tile-picker into this runtime folder (the
# entity window is a livedesk-taskbar concern, not a tile-picker one).
# Built here now so the whole runtime is one folder + one build script.
echo "-- entity renderer tp_desktop_window_rgb.c -> +x/tp_desktop_window_rgb.+x"
$CC $CFLAGS -o +x/tp_desktop_window_rgb.+x tp_desktop_window_rgb.c -lX11 -lXext

echo "-- emoji->sprite helper tp_asset_to_sprite.c -> +x/tp_asset_to_sprite.+x"
$CC $CFLAGS -o +x/tp_asset_to_sprite.+x tp_asset_to_sprite.c -lm

echo "-- emoji atlas helpers emoji_gen_atlas/emoji_xtract (copied from wsr-pal)"
# emoji_gen_atlas.+x + emoji_xtract.+x ship as prebuilt binaries from the
# 014.wsr-pal toolchain (same source every other widget copies them from)
# - the entity calls them via ops_dir at runtime, so they must sit in the
# same +x/ dir as the entity binary.
WSR="$(cd "$(dirname "$0")/../../../014.wsr-pal💸️📌️+2" 2>/dev/null && pwd)"
for t in emoji_gen_atlas emoji_xtract; do
    if [ ! -x "+x/$t.+x" ]; then
        if [ -n "$WSR" ] && [ -x "$WSR/ops/+x/$t.+x" ]; then
            cp "$WSR/ops/+x/$t.+x" "+x/$t.+x"
            chmod +x "+x/$t.+x"
            echo "    $t.+x copied from wsr-pal"
        else
            echo "WARN: +x/$t.+x missing (wsr-pal copy unavailable)"
        fi
    fi
done

echo "-- window-position/range-grid helper tp_range_grid.c -> +x/tp_range_grid.+x"
$CC $CFLAGS -o +x/tp_range_grid.+x tp_range_grid.c -lX11 -lXext

echo "OK +x/khtpm_taskbar_manager_main.+x and +x/khtpm_strip_parser.+x (plus entity renderer + helpers)"
