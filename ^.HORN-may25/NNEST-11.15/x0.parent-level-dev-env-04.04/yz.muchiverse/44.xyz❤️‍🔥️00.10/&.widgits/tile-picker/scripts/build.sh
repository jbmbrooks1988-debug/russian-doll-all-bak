#!/bin/bash
# scripts/build.sh - build tile-picker widget ops + copy system pipeline,
# modeled directly on &.widgits/file-menu/scripts/build.sh.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x

CFLAGS="-Wall -Wextra -O2"

echo "--- Building tile-picker ops ---"
gcc $CFLAGS -o "ops/+x/tp_set_brush.+x" "ops/tp_set_brush.c"
gcc $CFLAGS -o "ops/+x/tp_place.+x" "ops/tp_place.c"
gcc $CFLAGS -o "ops/+x/tp_place_desktop.+x" "ops/tp_place_desktop.c"
gcc $CFLAGS -o "ops/+x/tp_import_from_desktop.+x" "ops/tp_import_from_desktop.c"
gcc $CFLAGS -o "ops/+x/tp_desktop_window.+x" "ops/tp_desktop_window.c" -lX11 -lXext -lGL -lGLX
gcc $CFLAGS -o "ops/+x/tp_compose_frame.+x" "ops/tp_compose_frame.c"
gcc $CFLAGS -o "ops/+x/tp_menu_input.+x" "ops/tp_menu_input.c"
gcc $CFLAGS -o "ops/+x/tp_test_send_key.+x" "ops/tp_test_send_key.c" -lX11 -lXtst
gcc $CFLAGS -o "ops/+x/tp_find_window_by_pid.+x" "ops/tp_find_window_by_pid.c" -lX11
gcc $CFLAGS -o "ops/+x/tp_set_wm_pid.+x" "ops/tp_set_wm_pid.c" -lX11
gcc $CFLAGS -o "ops/+x/ledger_peers.+x" "ops/ledger_peers.c"
gcc $CFLAGS -o "ops/+x/tp_arm_placer.+x" "ops/tp_arm_placer.c" -lX11
gcc -Wall -O2 -o "ops/+x/tp_asset_to_sprite.+x" "ops/tp_asset_to_sprite.c" -lm
gcc -Wall -O2 -o "ops/+x/tp_rmmv_character_extract.+x" "ops/tp_rmmv_character_extract.c" -lm

echo "--- Copying system binaries (local copies for dev) ---"
WSR="$(cd "$SCRIPT_DIR/../.." && pwd)/014.wsr-pal💸️📌️+2"
if [ -d "$WSR" ]; then
    mkdir -p system
    cp "$WSR/system/prisc+x" system/prisc+x 2>/dev/null || true
    cp "$WSR/system/chtpm_parser_pal" system/chtpm_parser_pal 2>/dev/null || true
    cp "$WSR/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    cp "$WSR/system/keyboard_input" system/keyboard_input 2>/dev/null || true
    cp "$WSR/system/renderer" system/renderer 2>/dev/null || true
    cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
    cp "$WSR/default_op.txt" system/default_op.txt 2>/dev/null || true
    chmod +x system/*
    echo "copied from wsr-pal"

    echo "--- Copying emoji_gen_atlas.+x + emoji_xtract.+x (real, already-working on-demand emoji rendering chtpm_rgb_render calls) ---"
    mkdir -p ops/+x
    cp "$WSR/ops/+x/emoji_gen_atlas.+x" ops/+x/emoji_gen_atlas.+x 2>/dev/null || true
    cp "$WSR/ops/+x/emoji_xtract.+x" ops/+x/emoji_xtract.+x 2>/dev/null || true
    chmod +x ops/+x/emoji_gen_atlas.+x ops/+x/emoji_xtract.+x 2>/dev/null || true
    echo "copied emoji_gen_atlas.+x + emoji_xtract.+x from wsr-pal"

    echo "--- Copying font glyph registry ---"
    mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii"
    for dir in "$WSR/pieces/registry/fonts/ascii/"*/; do
        [ -d "$dir" ] || continue
        code="$(basename "$dir")"
        mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code"
        cp "$dir/glyph.txt" "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code/glyph.txt"
    done
    echo "glyphs: $(ls "$SCRIPT_DIR/pieces/registry/fonts/ascii/" | wc -l)"
else
    echo "WARN: wsr-pal not found, system binaries not linked"
fi

echo "build ok"
