#!/bin/sh
# Builds every binary independently - no shared object files, no shared
# headers, matching cdda-tpm-std-fast.txt sec. 1. Each translation unit
# is compiled and linked on its own line.
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"

echo "-- prisc+x (VM)"
$CC $CFLAGS -o system/prisc+x system/prisc+x.c

echo "-- manager"
$CC $CFLAGS -o manager/mutaclsym_manager manager/mutaclsym_manager.c -lncurses

echo "-- ops"
$CC $CFLAGS -o ops/+x/move_player.+x ops/move_player.c
$CC $CFLAGS -o ops/+x/end_turn.+x ops/end_turn.c

echo "build ok"
