#!/bin/bash
# demo_load.sh - proves LOAD works end-to-end through REAL entry
# points and REAL key injection (§36.6 level 2), independent of
# demo_save.sh (modular — either scenario runs standalone).
#
# Flow: launch the real combined app -> navigate file-menu's REAL menu
# via injected keys (LOAD -> type absolute fixture path -> confirm) ->
# assert editor's buffer AND its rendered frame both show the loaded
# content (state-file assertion alone is exactly what the old
# level-1-only harness could do; the frame assertion is the part it
# could never catch — see !.xyzos-standards+1.txt §36.6).
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { report PASS "$1"; }
fail() { report FAIL "$1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS/proof/load-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"

echo "=== demo_load: LOAD via real key injection ==="

# Unique fixture content so a false-positive (stale buffer, wrong
# file) is impossible to mistake for a real pass.
#
# Fixture lives under $HARNESS itself (this house's own directory tree
# has emoji path segments — 🤖️🪤️🏠️, 🥡️🪜️, ❤️‍🔥️ — that used to be
# untypeable through the FILE field one keycode at a time; fixed via
# fm_menu_input.c's own new PASTE mode, see common.sh's header and
# PITFALL 56, so the real project path is used here unmodified).
MARKER="HARNESS-LOAD-MARKER-$$-$(date +%s)"
FIXTURE="$HARNESS/fixtures/load_target_$$.txt"
mkdir -p "$HARNESS/fixtures"
printf '%s\nsecond line of the fixture\n' "$MARKER" > "$FIXTURE"
cp "$FIXTURE" "$PROOF/00_fixture.txt"

sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
start_app

EDITOR_SESSION="$(find_editor_session)" || { fail "editor session never appeared"; exit 1; }
FM_SESSION="$(find_fm_session)" || { fail "file-menu session never appeared"; exit 1; }
echo "EDITOR_SESSION=$EDITOR_SESSION" | tee -a "$PROOF/01_sessions.txt"
echo "FM_SESSION=$FM_SESSION" | tee -a "$PROOF/01_sessions.txt"

# file-menu's own layout is buttonless (a "dumb ASCII text menu", the
# same PAL-native architecture as mutaclsym/muchi-pals/pal-chain) —
# chtpm_parser_pal's own is_interactive() gate (button/canvas/cli_io/
# scroller only) means it has NOTHING to navigate for this layout;
# confirmed live that keyboard/history.txt injection produces zero
# movement here. interact_relay.txt IS the real input channel for
# this architecture — gl_mirror.c's own append_key() writes here,
# bare decimal, for a real live GL keypress; fm_relay_key reproduces
# that exact mechanism, not a shortcut around it. See common.sh's own
# header comment and !.xyzos-standards+1.txt §36.6 for the full account.
wait_for_path "$FM_SESSION/pieces/apps/player_app/interact_relay.txt" 50 \
    || { fail "file-menu interact_relay.txt never appeared"; exit 1; }

# Real file-menu menu nav: '4' = LOAD (direct numeric select, matches
# handle_main_menu's own key>=49&&key<=54 branch in fm_menu_input.c).
fm_relay_key "$FM_SESSION" 52
sleep 0.3
# ARROW_DOWN once: cursor_pos 0 (SEARCH) -> 1 (FILE field).
fm_relay_key "$FM_SESSION" 1003
sleep 0.2
# PASTE the fixture's absolute path into the FILE field — real UTF-8,
# whole string, via fm_menu_input.c's own new PASTE mode (PITFALL 56).
fm_paste "$FM_SESSION" "$FIXTURE"
sleep 0.2
# ARROW_DOWN bounded-many times clamps at cancel_idx (max_idx) —
# entry-count-agnostic, no need to know how many dir entries exist.
fm_relay_repeat "$FM_SESSION" 1003 30
sleep 0.2
# One ARROW_UP steps back from cancel_idx to confirm_idx.
fm_relay_key "$FM_SESSION" 1002
sleep 0.2
# Enter at confirm_idx -> enqueue_cmd_with_path("LOAD", path_buffer).
fm_relay_key "$FM_SESSION" 13
sleep 0.3

echo "--- relayed key sequence (file-menu, via interact_relay.txt) ---" | tee -a "$PROOF/02_injected_keys.txt"
tail -n 60 "$FM_SESSION/pieces/apps/player_app/interact_relay.txt" >> "$PROOF/02_injected_keys.txt"

# editor_widget_cmds's own background drain loop (started by this
# app's button.sh, step 6/97 of @.apps/text-editor-xyz/button.sh)
# polls the inbox every 0.2s — give it real margin.
sleep 2

BUFFER="$EDITOR_SESSION/pieces/system/editor_buffer.txt"
FRAME="$EDITOR_SESSION/pieces/display/current_frame.txt"
STATUS="$EDITOR_SESSION/pieces/system/widget_cmds/status.txt"
cp "$BUFFER" "$PROOF/03_editor_buffer.txt" 2>/dev/null
cp "$FRAME" "$PROOF/03_current_frame.txt" 2>/dev/null
cp "$STATUS" "$PROOF/03_status.txt" 2>/dev/null

if [ -f "$BUFFER" ] && diff -q "$FIXTURE" "$BUFFER" >/dev/null 2>&1; then
    pass "editor_buffer.txt byte-for-byte matches fixture"
else
    fail "editor_buffer.txt does NOT match fixture (state-file check)"
fi

if [ -f "$FRAME" ] && grep -qF "$MARKER" "$FRAME"; then
    pass "current_frame.txt (RENDERED frame) contains the marker — level-2 proof, not just state"
else
    fail "current_frame.txt does NOT show the marker — rendered frame check FAILED"
fi

if [ -f "$STATUS" ] && grep -q '^last_cmd=LOAD$' "$STATUS" && grep -q '^result=ok$' "$STATUS"; then
    pass "widget_cmds/status.txt shows last_cmd=LOAD result=ok"
else
    fail "widget_cmds/status.txt does not confirm LOAD ok"
fi

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then
    echo "=== demo_load: ALL PASS ==="
else
    echo "=== demo_load: FAILURES ABOVE — see $PROOF ==="
fi
exit "$FAIL"
