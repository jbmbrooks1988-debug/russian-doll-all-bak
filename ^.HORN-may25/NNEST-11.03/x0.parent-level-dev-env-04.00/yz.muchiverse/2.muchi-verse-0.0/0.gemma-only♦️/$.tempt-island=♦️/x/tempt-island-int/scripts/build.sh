#!/bin/sh
# tempt-island R0 build - each TU its own binary (TPMOS / mutaclsym doctrine)
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "-- prisc+x (VM)"
$CC $CFLAGS -o system/prisc+x system/prisc+x.c

echo "-- keyboard_input (raw termios, no ncurses)"
$CC $CFLAGS -o system/keyboard_input system/keyboard_input.c

echo "-- renderer (plain stdout, no ncurses)"
$CC $CFLAGS -o system/renderer system/renderer.c

echo "-- gl_mirror (optional best-effort)"
if $CC $CFLAGS -o system/gl_mirror system/gl_mirror.c -lglut -lGL -lGLU 2>/tmp/tempt-island_gl_build.log; then
    echo "   ok"
else
    echo "   skipped (see /tmp/tempt-island_gl_build.log)"
fi

echo "-- ops (R0 minimal)"
mkdir -p ops/+x
$CC $CFLAGS -o ops/+x/move_player.+x ops/move_player.c
$CC $CFLAGS -o ops/+x/end_turn.+x ops/end_turn.c
$CC $CFLAGS -o ops/+x/compose_frame.+x ops/compose_frame.c
$CC $CFLAGS -o ops/+x/pdl_reader.+x ops/pdl_reader.c

echo "-- ops (R2 nav + RGB mirror)"
$CC $CFLAGS -o ops/+x/choice.+x ops/choice.c
$CC $CFLAGS -o ops/+x/compose_rgb_frame.+x ops/compose_rgb_frame.c -lm

echo "-- ops (R2 TEMPT ISLAND verbs)"
$CC $CFLAGS -o ops/+x/talk.+x ops/talk.c
$CC $CFLAGS -o ops/+x/activity.+x ops/activity.c
$CC $CFLAGS -o ops/+x/scene_reader.+x ops/scene_reader.c

echo "-- ops (R3 items)"
$CC $CFLAGS -o ops/+x/pickup.+x ops/pickup.c
$CC $CFLAGS -o ops/+x/drop.+x ops/drop.c

echo "-- ops (R5 save/title)"
$CC $CFLAGS -o ops/+x/save_game.+x ops/save_game.c
$CC $CFLAGS -o ops/+x/title_input.+x ops/title_input.c
$CC $CFLAGS -o ops/+x/compose_title_frame.+x ops/compose_title_frame.c

echo "build ok"
