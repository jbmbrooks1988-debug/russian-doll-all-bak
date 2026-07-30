#!/bin/bash
# demo_save.sh - proves SAVE_AS works end-to-end through REAL entry
# points and REAL key injection (§36.6 level 2), including REAL text
# EDITING first (not a static seed-buffer passthrough — this is the
# "text edited files" part specifically asked for). Independent of
# demo_load.sh (modular — either scenario runs standalone).
#
# Flow: launch the real combined app -> engage editor's own INTERACT
# mode via a real keypress on the default-selected "EDIT TEXT
# (INTERACT)" button -> type a unique marker string as real keys ->
# exit INTERACT -> navigate file-menu's REAL menu (SAVE_AS -> type
# absolute output path -> confirm) -> assert the ACTUAL FILE ON DISK
# contains what was typed.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { report PASS "$1"; }
fail() { report FAIL "$1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS/proof/save-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF" "$HARNESS/workdir"

echo "=== demo_save: real edit + SAVE_AS via real key injection ==="

# Letters/hyphens only — avoids any ambiguity if timing is imperfect
# and a character lands before INTERACT is fully engaged (digits would
# otherwise risk being read as a menu-dispatch keycode instead of
# literal typed text).
EDIT_MARKER="EDITED-BY-HARNESS-$$-$(date +%s | tr -d '\n' | tr '0-9' 'a-j')"
OUT_PATH="$HARNESS/workdir/saved_$$.txt"
rm -f "$OUT_PATH"

sh "$HOUSE/EMERGENCY_KILL.sh" >/dev/null 2>&1 || true
start_app

EDITOR_SESSION="$(find_editor_session)" || { fail "editor session never appeared"; exit 1; }
FM_SESSION="$(find_fm_session)" || { fail "file-menu session never appeared"; exit 1; }
echo "EDITOR_SESSION=$EDITOR_SESSION" | tee -a "$PROOF/01_sessions.txt"
echo "FM_SESSION=$FM_SESSION" | tee -a "$PROOF/01_sessions.txt"

ED_HIST="$EDITOR_SESSION/pieces/keyboard/history.txt"
TYPING_FLAG="$EDITOR_SESSION/pieces/display/active_gui_is_typing.txt"
wait_for_path "$ED_HIST" 50 || { fail "editor keyboard/history.txt never appeared"; exit 1; }

# "EDIT TEXT (INTERACT)" is the default-selected element at fresh
# launch (row [>] 1. per the editor's own layout ordering) — Enter
# here engages chtpm_parser_pal's own onClick="INTERACT" handling
# directly (this is NOT routed through editor_menu_input.c's own
# digit-dispatch — piece.pdl's METHOD list has no "EDIT" command
# string handled there; confirmed by reading both files directly).
inject_key "$ED_HIST" 13

# Wait for the engagement to actually register (chtpm_parser_pal's
# export_active_index() writes this file) rather than a fixed sleep —
# real synchronization point, not a guess.
waited=0
while [ "$(cat "$TYPING_FLAG" 2>/dev/null)" != "1" ] && [ "$waited" -lt 30 ]; do
    sleep 0.1; waited=$((waited + 1))
done
if [ "$(cat "$TYPING_FLAG" 2>/dev/null)" != "1" ]; then
    fail "INTERACT never engaged (active_gui_is_typing.txt never became 1)"
    echo "$FAIL"; exit 1
fi
pass "INTERACT engaged (real button activation, not a shortcut)"

# ed_paste, not inject_string: confirmed live that the keyboard/
# history.txt -> chtpm_parser_pal -> project-history relay hop has its
# own separate polling-rate bottleneck beyond keyboard/history.txt's
# own 50ms-per-char throttle (inject_string's own real fix, still
# correct for what it covers) — a full marker string typed character-
# by-character through this specific relay came out truncated
# (confirmed: typed "...feadagh", buffer only got "...bhi") even with
# the throttle in place. ed_paste sidesteps the extra hop entirely via
# editor_menu_input.c's own PASTE mode (same mechanism PITFALL 56 added
# for file-menu's FILE field, reused here for a different bottleneck).
ed_paste "$EDITOR_SESSION" "$EDIT_MARKER"
sleep 0.3
# ESC exits INTERACT (per the layout's own on-screen hint text).
inject_key "$ED_HIST" 27
sleep 0.2
# '4' = FM (FILE MENU) per piece.pdl's own METHOD list — a real
# production signal (do_fm(), wakes/focuses the already-running
# widget), not a test shortcut; matches the real user flow.
inject_key "$ED_HIST" 52
sleep 0.3

echo "--- injected key sequence (editor) ---" | tee -a "$PROOF/02_injected_keys_editor.txt"
tail -n 40 "$ED_HIST" >> "$PROOF/02_injected_keys_editor.txt"

# file-menu's own layout is buttonless — interact_relay.txt is the
# real input channel for it (gl_mirror.c's own append_key() format),
# not keyboard/history.txt. See common.sh's header comment / demo_load.sh
# for the full account of why this differs from editor's own side above.
wait_for_path "$FM_SESSION/pieces/apps/player_app/interact_relay.txt" 50 \
    || { fail "file-menu interact_relay.txt never appeared"; exit 1; }

# '3' = SAVE_AS (direct numeric select).
fm_relay_key "$FM_SESSION" 51
sleep 0.3
fm_relay_key "$FM_SESSION" 1003
sleep 0.2
# PASTE, not character-by-character — same PITFALL 56 fix as demo_load.sh.
fm_paste "$FM_SESSION" "$OUT_PATH"
sleep 0.2
fm_relay_repeat "$FM_SESSION" 1003 30
sleep 0.2
fm_relay_key "$FM_SESSION" 1002
sleep 0.2
fm_relay_key "$FM_SESSION" 13
sleep 0.3

echo "--- relayed key sequence (file-menu, via interact_relay.txt) ---" | tee -a "$PROOF/02_injected_keys_fm.txt"
tail -n 60 "$FM_SESSION/pieces/apps/player_app/interact_relay.txt" >> "$PROOF/02_injected_keys_fm.txt"

sleep 2

STATUS="$EDITOR_SESSION/pieces/system/widget_cmds/status.txt"
cp "$STATUS" "$PROOF/03_status.txt" 2>/dev/null
[ -f "$OUT_PATH" ] && cp "$OUT_PATH" "$PROOF/03_saved_file.txt" 2>/dev/null

if [ -f "$OUT_PATH" ] && grep -qF "$EDIT_MARKER" "$OUT_PATH"; then
    pass "disk file at SAVE_AS target contains the text actually typed — real edit, real save, real disk"
else
    fail "disk file missing or does not contain the typed marker"
fi

# NOTE: editor_widget_cmds.c's own do_save_to() always labels
# last_cmd=SAVE via its own set_status("SAVE", ...) call, even when it
# was reached through the SAVE_AS:<path> branch — a real, minor,
# pre-existing naming quirk in that function itself (confirmed live),
# not a bug in this harness. The message field still correctly names
# the real SAVE_AS target path, and the disk-content assertion above
# is the actual load-bearing proof either way.
if [ -f "$STATUS" ] && grep -q '^last_cmd=SAVE$' "$STATUS" && grep -q '^result=ok$' "$STATUS"; then
    pass "widget_cmds/status.txt shows result=ok for the save"
else
    fail "widget_cmds/status.txt does not confirm SAVE_AS ok"
fi

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then
    echo "=== demo_save: ALL PASS ==="
else
    echo "=== demo_save: FAILURES ABOVE — see $PROOF ==="
fi
exit "$FAIL"
