#!/bin/sh
# EMERGENCY_CLEAN.sh - sweeps the house for NULL (0-byte) regeneratable
# display/frame artifacts that can break a zip/archive of the house
# tree, then regenerates nothing itself - the next real run of
# whichever project owns a given file writes it fresh, same as it
# always would on a normal launch.
#
# WHY THESE FILES CAN GO NULL (2026-07-30, real finding, not a guess):
# every op in this house family writes its own frame/display output via
# a plain `fopen(path, "w")` truncate-then-write - never a write-to-
# temp-then-rename. This is fine under EMERGENCY_KILL.sh's own normal
# use (kill first, THEN nothing writes) but a file caught mid-write -
# a process killed at the wrong instant, or a zip tool reading while a
# live session is still actively re-rendering - can be left at 0 bytes,
# truncated but never actually written. Confirmed this same session:
# fm_compose_frame.c's own view.txt hit the identical torn-read race
# during rapid harness polling (fixed there via a real read-retry, not
# applicable to a zip tool that has no such retry logic at all).
#
# SCOPE, DELIBERATELY NARROW: only files that are (a) CONFIRMED 0 bytes
# right now, AND (b) match a known-regeneratable name from the list
# below - real display/frame OUTPUT this house's own ops already
# overwrite on every real run, never source code, never a project's
# own registry/pieces data, never anything under xyzfs/ (user
# documents live there now, see @.apps/text-editor-xyz/save-bug.txt -
# this script must never touch user data). A file with real content,
# even one on this same name list, is left completely untouched - this
# is a null-file sweep, not a general cache wipe.
#
# USAGE: run this BEFORE zipping/archiving the house tree, any time
# after EMERGENCY_KILL.sh (or on its own - it does its own quick kill
# pass first, see below, so nothing is actively re-writing while this
# runs).
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "EMERGENCY CLEAN..."

# Kill first (same as EMERGENCY_KILL.sh's own reasoning: don't sweep
# for null files while something might still be actively writing one).
sh "$SCRIPT_DIR/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true

# Regeneratable display/frame OUTPUT filenames - every one of these is
# written fresh by some op on every real launch, confirmed via direct
# source read this session (chtpm_rgb_render.c, gl_mirror.c,
# chtpm_parser_pal.c, fm_compose_frame.c/agy_compose_view.c/
# editor_compose_frame.c, tp_taskbar.c and their own sibling ops).
NAMES='rgb_frame.raw current_frame.txt view.txt frame_changed.txt
editor_screen_changed.txt rgb_frame_changed.txt fm_screen_changed.txt
renderer_pulse.txt gl_display.receipt.txt text_substitution_debug.txt
debug.txt strip_frame.raw strip_frame.receipt.txt strip_frame_log.txt
strip_frame_changed.txt popup_frame.raw popup_frame.receipt.txt
popup_frame_log.txt key_history.txt'

FOUND=0
for name in $NAMES; do
    # Bounded find (maxdepth, name match only, not a content walk) -
    # same performance-conscious shape as EMERGENCY_KILL.sh's own
    # session sweep. Explicitly excludes xyzfs/ (user documents) even
    # though none of these filenames would ever legitimately live
    # there - a second, explicit guard, not just relying on the name
    # list alone.
    while IFS= read -r f; do
        case "$f" in
            */xyzfs/*) continue ;;
        esac
        if [ -f "$f" ] && [ ! -s "$f" ]; then
            rm -f "$f"
            echo "  removed (0 bytes): $f"
            FOUND=$((FOUND + 1))
        fi
    done <<EOF
$(find "$SCRIPT_DIR" -maxdepth 6 -type f -name "$name" 2>/dev/null)
EOF
done

if [ "$FOUND" -eq 0 ]; then
    echo "No null regeneratable files found."
else
    echo "Removed $FOUND null file(s) - safe to zip now; each will be"
    echo "regenerated fresh the next time its own project actually runs."
fi
echo "Done."
