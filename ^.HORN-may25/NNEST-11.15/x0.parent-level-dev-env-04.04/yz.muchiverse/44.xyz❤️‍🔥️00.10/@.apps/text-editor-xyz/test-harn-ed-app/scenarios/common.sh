#!/bin/bash
# common.sh - shared setup/teardown/injection helpers for
# test-harn-ed-app scenarios (source, don't execute directly).
#
# LEVEL-2 (§36.6) BY CONSTRUCTION, WITH A REAL CORRECTION FOUND WHILE
# BUILDING THIS HARNESS (2026-07-30): §36.6 originally said key
# injection must ALWAYS go through pieces/keyboard/history.txt, never
# a direct write to interact_relay.txt. That is correct for
# chtpm-NATIVE button layouts (102.editor's own layout has a real
# <button onClick="INTERACT"> - confirmed live, keyboard/history.txt
# injection correctly flips active_gui_is_typing.txt). It is WRONG as
# a blanket rule for &.widgits/file-menu's own layout (and the same
# "dumb ASCII text menu, PAL-native" architecture used by mutaclsym/
# muchi-pals/pal-chain): that layout has ZERO <button>/<canvas>/
# <cli_io>/<scroller> elements (chtpm_parser_pal.c's own
# is_interactive() gates on exactly those four types), so
# chtpm_parser_pal's process_key() literally has nothing to navigate -
# confirmed live by direct source read (is_navigable() -> is_interactive()
# -> false for every element in this layout) AND by testing: injecting
# into keyboard/history.txt for 30+ seconds produced zero movement,
# while a direct bare-decimal write into interact_relay.txt (the EXACT
# format & mechanism gl_mirror.c's own append_key() uses for a real
# live GL keypress - see that file's own "Forward to PAL loop's
# interact_relay.txt in bare decimal format" comment) moved cursor_pos
# immediately. For this architecture, interact_relay.txt IS the real,
# only input channel - writing to it directly, in gl_mirror's own
# format, is not a shortcut around the real path, it reproduces the
# real path exactly (a live keypress never touches keyboard/history.txt
# at all for this layout shape - only chtpm-native button layouts
# populate that file's relay use). See !.xyzos-standards+1.txt §36.6's
# own 2026-07-30 correction for the permanent record.
set -u

HARNESS="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="$(cd "$HARNESS/.." && pwd)"
HOUSE="$(cd "$APP_DIR/../.." && pwd)"
EDITOR_DIR="$(ls -d "$HOUSE"/102.*editor* 2>/dev/null | head -1)"
FM_DIR="$(ls -d "$HOUSE"/\&.widgits/file-menu* 2>/dev/null | head -1)"

# --- key injection (§36.6 mandatory mechanics) ---------------------

# inject_key <history_file> <keycode>
inject_key() {
    local hist="$1" code="$2"
    local ts
    ts="$(date '+%Y-%m-%d %H:%M:%S')"
    echo "[$ts] KEY_PRESSED: $code" >> "$hist"
}

# inject_string <history_file> <string> - one KEY_PRESSED line per
# character, ASCII code via printf/od. Only 7-bit printable input
# expected (paths, plain-ASCII test text) - matches insert_char()'s
# own 32-126 acceptance range in editor_menu_input.c/fm_menu_input.c.
inject_string() {
    local hist="$1" str="$2"
    local i ch code
    for (( i=0; i<${#str}; i++ )); do
        ch="${str:$i:1}"
        code=$(printf '%d' "'$ch")
        inject_key "$hist" "$code"
        # PAL loop processes one key per ~16.7ms iteration (main_loop_
        # chtpm.pal's own `sleep 16667`) — writing faster than that
        # drops characters (confirmed live: no delay -> path_buffer
        # truncated after ~12 chars; 50ms delay -> exact match, zero
        # loss). Real timing constraint, not an arbitrary safety margin.
        sleep 0.05
    done
}

# inject_repeat <history_file> <keycode> <count>
inject_repeat() {
    local hist="$1" code="$2" n="$3" i
    for (( i=0; i<n; i++ )); do inject_key "$hist" "$code"; sleep 0.05; done
}

# --- file-menu's own relay (buttonless layout — see header comment) --

# fm_relay_key <fm_session> <keycode> - bare decimal, gl_mirror.c's
# own exact append_key() format/target for a real live GL keypress.
fm_relay_key() {
    local session="$1" code="$2"
    echo "$code" >> "$session/pieces/apps/player_app/interact_relay.txt"
}

# fm_relay_string <fm_session> <string> - one line per character.
# See inject_string's own comment above — same 16.7ms-per-key PAL loop
# constraint applies identically here (confirmed live for this exact
# function too, same failure/fix shape).
fm_relay_string() {
    local session="$1" str="$2" i ch code
    for (( i=0; i<${#str}; i++ )); do
        ch="${str:$i:1}"
        code=$(printf '%d' "'$ch")
        fm_relay_key "$session" "$code"
        sleep 0.05
    done
}

# fm_relay_repeat <fm_session> <keycode> <count>
fm_relay_repeat() {
    local session="$1" code="$2" n="$3" i
    for (( i=0; i<n; i++ )); do fm_relay_key "$session" "$code"; sleep 0.05; done
}

# --- PASTE mode (2026-07-30) ------------------------------------------
#
# A real keypress is one int, matching a physical keyboard's one-
# scancode-per-keystroke model — the FILE field's own typing path
# (fm_menu_input.c) and INTERACT's own typing path (editor_menu_
# input.c) both only accept single-byte ASCII for exactly that reason.
# This house's own directory tree has emoji path segments
# (🤖️🪤️🏠️, 🥡️🪜️, ❤️‍🔥️, ...) that can never be typed character-by-
# character through either — confirmed live: iterating a UTF-8 string
# one keycode at a time drops every multi-byte sequence. PASTE is a
# real, separate mechanism (not a relaxation of the keystroke-by-
# keystroke path, which stays exactly as strict as a real keyboard) —
# both ops now accept `<op> PASTE "<string>"` as a direct, whole-string
# insert. This is the SAME op binary real production code will use
# once wired to an actual paste trigger (Ctrl+V/clipboard) — calling
# it directly here exercises the real capability, not a bypass of it.

# fm_paste <fm_session> <string> - appends to whichever field (SEARCH/
# FILE) is currently selected, via the real fm_menu_input.+x op.
fm_paste() {
    local session="$1" text="$2"
    PRISC_PROJECT_ROOT="$session" "$FM_DIR/ops/+x/fm_menu_input.+x" PASTE "$text" >/dev/null 2>&1
}

# ed_paste <editor_session> <string> - inserts at cursor, via the real
# editor_menu_input.+x op. Requires INTERACT already engaged (same
# real gate insert_char() has).
ed_paste() {
    local session="$1" text="$2"
    PRISC_PROJECT_ROOT="$session" "$EDITOR_DIR/ops/+x/editor_menu_input.+x" PASTE "$text" >/dev/null 2>&1
}

# --- polling ---------------------------------------------------------

# wait_for_path <path> <timeout_tenths>
wait_for_path() {
    local path="$1" timeout="${2:-50}" waited=0
    while [ ! -e "$path" ] && [ "$waited" -lt "$timeout" ]; do
        sleep 0.1; waited=$((waited + 1))
    done
    [ -e "$path" ]
}

# wait_for_glob <parent_dir> <pattern> <timeout_tenths> - prints match on stdout
wait_for_glob() {
    local parent="$1" pattern="$2" timeout="${3:-50}" waited=0 hit=""
    while [ "$waited" -lt "$timeout" ]; do
        # -d: list the matched dir ENTRY itself, not its contents
        # (plain `ls -t <dir>` lists what's INSIDE the dir — a real
        # bug this exact helper hit live: printed "debug.txt", the
        # newest file inside the session dir, instead of the session
        # dir's own path).
        hit="$(ls -td "$parent"/$pattern 2>/dev/null | head -1)"
        [ -n "$hit" ] && { echo "$hit"; return 0; }
        sleep 0.1; waited=$((waited + 1))
    done
    return 1
}

# --- session lifecycle ------------------------------------------------

APP_PID=""

start_app() {
    # PITFALL 53: stdin from /dev/null so keyboard_input never touches
    # a real controlling tty at all - sidesteps the SIGTTIN group-stop
    # entirely, rather than relying on "just don't background it" (a
    # harness has to background it to keep injecting after launch).
    bash "$APP_DIR/button.sh" run </dev/null >"$HARNESS/proof/app_stdout.log" 2>&1 &
    APP_PID=$!
}

stop_app() {
    [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null
    sleep 0.3
    sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
}

# find_editor_session - prints the /tmp/.text-editor-xyz-editor-* dir
find_editor_session() {
    wait_for_glob /tmp ".text-editor-xyz-editor-*" 80
}

# find_fm_session - prints the file-menu widget's own session dir
find_fm_session() {
    wait_for_glob "$FM_DIR/pieces/sessions" "*" 80
}

report() {
    local status="$1" msg="$2"
    echo "$status: $msg"
}
