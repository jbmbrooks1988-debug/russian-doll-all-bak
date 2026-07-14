#!/bin/sh
# Builds every binary independently - no shared object files, no shared
# headers, matching cdda-tpm-std-fast.txt sec. 1 (same convention as
# mutaclsym/scripts/build.sh). Each translation unit is compiled and
# linked on its own line.
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "-- prisc+x (VM)"
$CC $CFLAGS -o system/prisc+x system/prisc+x.c

echo "-- emoji_gen_atlas (FreeType color-bitmap emoji -> PNG)"
$CC $CFLAGS -o system/emoji_gen_atlas system/emoji_gen_atlas.c $(pkg-config --cflags --libs freetype2) -lm

echo "-- emoji_xtract (PNG -> plain-text pixel CSV)"
$CC $CFLAGS -o system/emoji_xtract system/emoji_xtract.c -lm

echo "-- egg_window (shaped GL window, still circular - see its own header comment)"
$CC $CFLAGS -o system/egg_window system/egg_window.c -lX11 -lXext -lGL -lm

echo "-- keyboard_input (raw termios, no ncurses)"
$CC $CFLAGS -o system/keyboard_input system/keyboard_input.c

echo "-- renderer (plain stdout, no ncurses)"
$CC $CFLAGS -o system/renderer system/renderer.c

echo "-- ops"
$CC $CFLAGS -o ops/+x/generate_egg.+x ops/generate_egg.c
$CC $CFLAGS -o ops/+x/claim_tokens.+x ops/claim_tokens.c
$CC $CFLAGS -o ops/+x/coin_flip.+x ops/coin_flip.c
$CC $CFLAGS -o ops/+x/buy_egg.+x ops/buy_egg.c
$CC $CFLAGS -o ops/+x/hatch_egg.+x ops/hatch_egg.c
$CC $CFLAGS -o ops/+x/menu_input.+x ops/menu_input.c
$CC $CFLAGS -o ops/+x/compose_menu.+x ops/compose_menu.c

echo "build ok"
