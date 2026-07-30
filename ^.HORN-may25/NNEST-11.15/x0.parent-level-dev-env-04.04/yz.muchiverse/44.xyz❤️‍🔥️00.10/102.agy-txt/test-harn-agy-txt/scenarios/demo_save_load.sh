#!/bin/bash
# demo_save_load.sh - proves 102.agy-txt's own real edit -> save -> new
# -> load loop end to end, through the REAL running app, real key
# injection (§36.6 level 2), matching test-harn-ed-app's own real
# precedent shape - not an op-level shortcut.
#
# Flow: launch -> real INTERACT typing (unique marker, not just the
# static seed buffer) -> real menu nav to SAVE AS -> real path typing
# -> real save -> assert the REAL FILE on disk -> real NEW (clears
# buffer) -> real menu nav to LOAD -> real path typing -> real load ->
# assert the buffer is restored from disk, not just "looks unchanged".
#
# NAV METHOD, REAL FINDING FROM BUILDING THIS SCRIPT (not assumed):
# this project's layouts are real CHTPM buttoned layouts, using
# chtpm_parser_pal.c's own GENERIC button nav - NOT file-menu widget's
# own custom "clamps at max_idx" nav (file-menu implements its own nav
# entirely in fm_menu_input.c, a different, buttonless architecture).
# Confirmed by direct read of chtpm_parser_pal.c: ARROW_UP/ARROW_DOWN
# WRAP AROUND cyclically (`if (focus_index < 0) focus_index =
# element_count-1;`), they do NOT clamp - "press UP enough times to
# guarantee item 1" is FALSE for this nav model and produced flaky,
# session-state-dependent failures when first tried. Real fix: use
# DIRECT DIGIT-KEY JUMP instead (chtpm_parser_pal.c's own
# `isdigit(key)` -> `do_jump()` branch - press the ASCII digit for the
# target item number, then Enter to activate it) - deterministic
# regardless of where focus started.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$SCRIPT_DIR/common.sh"

FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

cleanup() { stop_app; }
trap cleanup EXIT INT TERM

PROOF="$HARNESS_DIR/proof/save-load-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF"

echo "=== agy-txt: real edit -> save -> new -> load, via real key injection ==="

MARKER="AGY-HARNESS-MARKER-$$-$(date +%s)"
SAVE_PATH="docs/harness_$$.txt"
rm -f "$AGY_DIR/$SAVE_PATH"

stop_app
start_app

SESS="$(find_session)" || { fail "session launch - current_frame.txt never appeared"; exit 1; }
echo "SESSION=$SESS" | tee "$PROOF/00_session.txt"
cp "$SESS/pieces/display/current_frame.txt" "$PROOF/01_initial_frame.txt"

# jump_to <session> <item_number> - direct digit-key nav (ASCII '1'=49),
# deterministic regardless of persisted focus state (see header note).
jump_to() {
    local session="$1" n="$2"
    inject_key "$session" "$((48 + n))"
}

# --- Real edit: type a unique marker into the editor canvas ---
jump_to "$SESS" 1                # EDIT TEXT (INTERACT)
inject_key "$SESS" 13
wait_for_typing "$SESS" 30 || { fail "INTERACT never engaged on editor.chtpm"; exit 1; }
pass "INTERACT engaged on editor canvas"
ag_paste "$SESS" " $MARKER"
sleep 0.3
inject_key "$SESS" 27            # Esc -> exit INTERACT
sleep 0.3
if grep -qF "$MARKER" "$SESS/pieces/system/editor_buffer.txt"; then
    pass "real typed marker landed in editor_buffer.txt"
else
    fail "marker never appeared in editor_buffer.txt after real typing"
fi

# --- Navigate to FILE MENU -> SAVE AS ---
jump_to "$SESS" 2                 # FILE MENU
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_menu.chtpm" ] \
    && pass "navigated to file_menu.chtpm" || fail "did not land on file_menu.chtpm"

jump_to "$SESS" 3                 # SAVE AS...
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_browser_save.chtpm" ] \
    && pass "navigated to file_browser_save.chtpm" || fail "did not land on file_browser_save.chtpm"

# --- Real path typing + real SAVE ---
jump_to "$SESS" 1                 # TYPE PATH (INTERACT)
inject_key "$SESS" 13
wait_for_typing "$SESS" 30 || { fail "INTERACT never engaged on path field (save)"; exit 1; }
ag_paste "$SESS" "$SAVE_PATH"
sleep 0.3
inject_key "$SESS" 27
sleep 0.3
jump_to "$SESS" 2                 # SAVE
inject_key "$SESS" 13
sleep 2
cp "$SESS/pieces/display/current_frame.txt" "$PROOF/02_after_save_frame.txt"

REAL_SAVE_TARGET="$AGY_DIR/$SAVE_PATH"
if [ -f "$REAL_SAVE_TARGET" ] && grep -qF "$MARKER" "$REAL_SAVE_TARGET"; then
    pass "real file with the real marker landed at the DURABLE path ($SAVE_PATH) — PITFALL 62's own regression covered"
else
    fail "no real file (with marker) found at $REAL_SAVE_TARGET after SAVE"
fi

# --- Navigate back to FILE MENU, real NEW (clears the buffer) ---
jump_to "$SESS" 3                 # CANCEL (file_browser_save's own item 3) -> back to file_menu
inject_key "$SESS" 13
sleep 0.3
jump_to "$SESS" 1                 # NEW FILE
inject_key "$SESS" 13
sleep 2
if ! grep -qF "$MARKER" "$SESS/pieces/system/editor_buffer.txt" 2>/dev/null; then
    pass "real NEW cleared the marker out of editor_buffer.txt"
else
    fail "NEW did not clear the buffer — marker still present"
fi

# --- Navigate to LOAD, real path typing, real LOAD ---
jump_to "$SESS" 4                 # LOAD...
inject_key "$SESS" 13
sleep 0.3
[ "$(current_layout_of "$SESS")" = "pieces/chtpm/layouts/file_browser_load.chtpm" ] \
    && pass "navigated to file_browser_load.chtpm" || fail "did not land on file_browser_load.chtpm"

jump_to "$SESS" 1                 # TYPE PATH (INTERACT)
inject_key "$SESS" 13
wait_for_typing "$SESS" 30 || { fail "INTERACT never engaged on path field (load)"; exit 1; }
ag_paste "$SESS" "$SAVE_PATH"
sleep 0.3
inject_key "$SESS" 27
sleep 0.3
jump_to "$SESS" 2                 # LOAD
inject_key "$SESS" 13
sleep 2
cp "$SESS/pieces/display/current_frame.txt" "$PROOF/03_after_load_frame.txt"
cp "$SESS/pieces/system/editor_buffer.txt" "$PROOF/04_buffer_after_load.txt" 2>/dev/null

if grep -qF "$MARKER" "$SESS/pieces/system/editor_buffer.txt" 2>/dev/null; then
    pass "real LOAD restored the marker from disk into editor_buffer.txt — full save/load loop proven"
else
    fail "marker not present after LOAD — buffer was not really restored from disk"
fi

rm -f "$REAL_SAVE_TARGET"

echo ""
echo "Proof: $PROOF"
if [ "$FAIL" = "0" ]; then echo "=== OVERALL: PASS ==="
else echo "=== OVERALL: FAIL ==="; fi
exit "$FAIL"
