#!/bin/sh
#
# clean_wraith.sh -- deletes/truncates regenerable, expensive-to-carry
# runtime output under projects/wraith-alpha/ (video frame captures, raw
# frame buffers, recordings, and unbounded logs) before pushing to
# GitHub. Everything this script removes is recreated automatically the
# next time the relevant Wraith project runs -- none of it is source.
#
# POSIX sh only (no bashisms) -- runs fine as `sh clean_wraith.sh`,
# `bash clean_wraith.sh`, or `./clean_wraith.sh`. Avoids ${BASH_SOURCE[0]},
# process substitution, and `read -d ''`, all bash-only.
#
# Usage:
#   ./clean_wraith.sh              deletes/truncates everything below
#   ./clean_wraith.sh --dry-run    lists what would be removed + total
#                                  size freed, without touching anything
#
# Scope: projects/wraith-alpha/ only (this repo has other, unrelated
# projects with their own history.txt/log bloat -- out of scope for a
# script literally named clean_wraith.sh; see the two top-level
# exceptions called out explicitly below, which ARE wraith-driven).

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="${TPMOS_ROOT:-$SCRIPT_DIR/1.TPMOS_c_+rmmp.0102.0027}"
WRAITH="$ROOT/projects/wraith-alpha"

DRY_RUN=0
if [ "${1:-}" = "--dry-run" ] || [ "${1:-}" = "-n" ]; then
    DRY_RUN=1
fi

if [ ! -d "$WRAITH" ]; then
    echo "Error: wraith-alpha not found at $WRAITH" >&2
    echo "(set TPMOS_ROOT to override where the TPMOS project root is)" >&2
    exit 1
fi

TMP_LIST="$(mktemp 2>/dev/null || echo "/tmp/clean_wraith.$$")"
trap 'rm -f "$TMP_LIST"' EXIT INT TERM

total_bytes=0
removed_count=0
truncated_count=0

# du in bytes for a single file, 0 if missing (portable enough for the
# GNU coreutils this repo's other tools already assume).
size_of() {
    du -sb "$1" 2>/dev/null | cut -f1
}

# $1 = human label, $2 = search root, $3 = find -path pattern,
# $4 = optional find -path pattern to EXCLUDE (find's -path uses glob
# matching where '*' also matches '/', so a broad pattern like
# "*/fs_watch.marker" matches both a project's root-level copy AND its
# session/ copy -- pass an exclusion to keep passes disjoint).
#
# Uses a temp file + `while read` (not a pipe) so the loop runs in THIS
# shell, not a subshell -- a `find ... | while read` pipeline would run
# the loop in a subshell under POSIX sh/dash, silently discarding all the
# total_bytes/removed_count/truncated_count updates once the loop ends.
delete_matches() {
    label="$1"; search_root="$2"; pattern="$3"
    find "$search_root" -path "$pattern" -type f > "$TMP_LIST" 2>/dev/null
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        sz=$(size_of "$f")
        sz=${sz:-0}
        total_bytes=$((total_bytes + sz))
        removed_count=$((removed_count + 1))
        if [ "$DRY_RUN" = "1" ]; then
            printf '  [would delete] %s (%s bytes)\n' "$f" "$sz"
        else
            rm -f -- "$f"
        fi
    done < "$TMP_LIST"
    echo "$label: done"
}

truncate_matches() {
    label="$1"; search_root="$2"; pattern="$3"; exclude="${4:-}"
    find "$search_root" -path "$pattern" -type f > "$TMP_LIST" 2>/dev/null
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        if [ -n "$exclude" ]; then
            case "$f" in
                $exclude) continue ;;
            esac
        fi
        sz=$(size_of "$f")
        sz=${sz:-0}
        if [ "$sz" = "0" ]; then
            continue
        fi
        total_bytes=$((total_bytes + sz))
        truncated_count=$((truncated_count + 1))
        if [ "$DRY_RUN" = "1" ]; then
            printf '  [would truncate] %s (%s bytes)\n' "$f" "$sz"
        else
            : > "$f"
        fi
    done < "$TMP_LIST"
    echo "$label: done"
}

echo "clean_wraith.sh -- scanning $WRAITH"
if [ "$DRY_RUN" = "1" ]; then
    echo "(--dry-run: nothing will actually be deleted)"
fi
echo

echo "--- Media / frame capture output (deleted entirely, regenerated on next run) ---"
delete_matches "video_frames PNGs"           "$WRAITH" "*/session/video_frames/*"
delete_matches "screen-record recordings"    "$WRAITH" "*/session/recordings/*"
delete_matches "current_frame.png previews"  "$WRAITH" "*/session/current_frame.png"
delete_matches "poster.png previews"         "$WRAITH" "*/session/poster.png"
delete_matches "captured audio (*.mp3)"      "$WRAITH" "*/session/*.mp3"
delete_matches "raw RGB frame buffer"        "$WRAITH" "*/session/rgb/current_frame.rgba32"
echo

echo "--- Unbounded logs (truncated to empty, file kept in place) ---"
truncate_matches "per-project debug_log.txt"  "$WRAITH" "*/session/debug_log.txt"
truncate_matches "per-project history.txt"    "$WRAITH" "*/session/history.txt"
truncate_matches "webcam ffmpeg logs"         "$WRAITH" "*/session/*.ffmpeg.log"
truncate_matches "fs_watch.marker (session)"  "$WRAITH" "*/session/fs_watch.marker"
truncate_matches "fs_watch.marker (project root)" "$WRAITH" "*/fs_watch.marker" "*/session/fs_watch.marker"
echo

echo "--- Two top-level exceptions: shared logs, but confirmed wraith-driven ---"
# debug.txt: wraith_parser_alpha.c's append-only click/launch log, lives
# at the TPMOS project root (not under projects/wraith-alpha/), found
# growing unbounded during an earlier disk-usage pass this same session.
if [ -f "$ROOT/debug.txt" ]; then
    sz=$(size_of "$ROOT/debug.txt"); sz=${sz:-0}
    if [ "$sz" != "0" ]; then
        total_bytes=$((total_bytes + sz))
        truncated_count=$((truncated_count + 1))
        if [ "$DRY_RUN" = "1" ]; then
            printf '  [would truncate] %s (%s bytes)\n' "$ROOT/debug.txt" "$sz"
        else
            : > "$ROOT/debug.txt"
        fi
    fi
fi
# pieces/debug/frames/*.txt: shared debug facility (windows_renderer.c /
# renderer.c also write here, not wraith-exclusive) but wraith-alpha_manager.c
# writes into it too and it's what grew large during this session's work.
truncate_matches "pieces/debug/frames/*.txt" "$ROOT/pieces/debug/frames" "$ROOT/pieces/debug/frames/*.txt"
echo

human_size() {
    bytes=$1
    if [ "$bytes" -ge $((1024*1024*1024)) ]; then
        awk "BEGIN { printf \"%.2f GB\", $bytes/1024/1024/1024 }"
    elif [ "$bytes" -ge $((1024*1024)) ]; then
        awk "BEGIN { printf \"%.2f MB\", $bytes/1024/1024 }"
    elif [ "$bytes" -ge 1024 ]; then
        awk "BEGIN { printf \"%.2f KB\", $bytes/1024 }"
    else
        echo "${bytes} B"
    fi
}

echo "======================================================================"
if [ "$DRY_RUN" = "1" ]; then
    echo "DRY RUN: would delete $removed_count file(s) and truncate $truncated_count file(s)"
    echo "Total space that would be freed: $(human_size "$total_bytes")"
    echo "Run again without --dry-run to actually apply."
else
    echo "Deleted $removed_count file(s), truncated $truncated_count file(s)"
    echo "Total space freed: $(human_size "$total_bytes")"
fi
