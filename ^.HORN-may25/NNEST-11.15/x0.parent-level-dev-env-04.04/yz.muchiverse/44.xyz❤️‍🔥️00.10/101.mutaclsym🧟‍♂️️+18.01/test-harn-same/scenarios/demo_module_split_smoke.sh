#!/bin/bash
# demo_module_split_smoke.sh - real K3-style smoke test for mutaclsym's
# own Phase 2 per-screen module split (2026-07-31, #.haiku+/!.xyzos-
# standards+1.txt §41), the first real harness this project has ever
# had. Proves the two things that split could plausibly break: real
# INTERACT-mode hero movement (the real xlector-cursor-then-possess
# flow this game actually uses, NOT a WASD button-focus model - see
# below) still works under game.chtpm's own new dedicated module, and
# real href navigation to/from info_test.chtpm's own new dedicated
# module still works, landing cleanly both ways.
#
# REAL CONTROL MODEL (ground-truthed live 2026-07-31, direct user
# correction + direct read of ops/move_player.c + ops/choice.c after
# an earlier version of this scenario used a wrong, pal-chain-style
# "digit-focus a button, Enter engages, WASD moves" model that never
# worked here):
#   1. "Control Hero" is a real <button onClick="INTERACT"> in
#      game.chtpm - focusing it and pressing Enter sets hero/state.txt's
#      interact_mode=1 directly. This does NOT reset the xlector
#      cursor to the hero's position (unlike the OLD, now-removed 'i'
#      key toggle choice.c's own header comment describes - that
#      behavior does not apply to this button-driven entry point,
#      confirmed live: cursor starts at (0,0) regardless of hero's own
#      position).
#   2. While interact_mode=1 and possessed_id="none" (the default),
#      ARROW keys move a free xlector CURSOR (uncollided) - see
#      ops/move_player.c's own header comment. WASD does NOT move
#      anything here - move_player.c's own comment states w/a/s/d/q/e/
#      r/t/c/v/f are camera_control.c's own keys, entirely separate.
#      Real arrow codes (from system/keyboard_input.c): ARROW_LEFT=
#      1000, ARROW_RIGHT=1001, ARROW_UP=1002, ARROW_DOWN=1003 - inject
#      these decimal codes directly (tk_inject_key.+x takes any
#      decimal code, not just a byte).
#   3. Enter, while the cursor sits exactly on the hero's own tile,
#      triggers try_possess_at() in ops/choice.c, setting
#      possessed_id="hero".
#   4. Once possessed, arrow keys move the HERO itself (with real
#      wall/monster-attack logic) instead of the cursor.
#
# STATE RESET: mutaclsym runs IN-PLACE (no pieces/sessions/<id>/
# isolation, confirmed via button.sh's own real "run" action) - game
# state (hero hp/pos, monsters, etc.) PERSISTS ACROSS HARNESS RUNS. A
# stale, previously-killed hero (hp=0 from earlier combat) silently
# blocks all movement and looks exactly like a dispatch bug - ground-
# truthed live the hard way. This scenario resets pieces/world_01/ from
# pieces/world_01_template/ before running, mirroring ops/
# title_input.c's own real "new game" reset_world() exactly (rm -rf +
# cp -r, same real mechanism the actual game uses, not a harness-only
# invention).
#
# Real per-keystroke injection into pieces/keyboard/history.txt via
# tk_inject_key.+x (matches the [TIMESTAMP] KEY_PRESSED: <code>
# format #.haiku+/tpmos-re-dox/_.0.aigent-testing-k3.txt requires),
# real frame + real state-file assertions - no PASTE-mode shortcuts,
# no op-level bypass.
#
# TIMING (ground-truthed live 2026-07-31): raw ARROW keys sent in a
# rapid burst (~0.6s apart) were observed to register as DOUBLE
# movement (4 real ARROW_RIGHT + 4 ARROW_DOWN landed the xlector
# cursor at +8/+8, not +4/+4) - reproduced twice, from a confirmed-
# fresh cursor each time, so not a miscount. The SAME keys sent one at
# a time with a full 1s wait between each landed correctly, every
# time, no exceptions. Root cause not chased down further (likely a
# relay/dispatch-loop consumption race under rapid injection, a real
# but SEPARATE concern from what this scenario is testing) - the
# pragmatic, verified-reliable fix here is simply: never burst raw
# movement keys faster than ~1s apart in this harness.
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OPS="$SCRIPT_DIR/ops/+x"
PROOF_DIR="$SCRIPT_DIR/proof/harness-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$PROOF_DIR"
FAIL=0
pass() { echo "PASS: $1"; }
fail() { echo "FAIL: $1"; FAIL=1; }

key()   { "$OPS/tk_inject_key.+x" "$PROJECT_DIR" "$1"; sleep 0.3; }
focus() { "$OPS/tk_focus_item.+x" "$PROJECT_DIR" "$PROJECT_DIR/pieces/display/current_frame.txt" "$1" >/dev/null; sleep 0.3; }
check() { "$OPS/tk_assert_contains.+x" "$1" "$2" "$3"; [ $? -ne 0 ] && FAIL=1; }
# wait_for_field <state_file> <field_name> <expected_value> <timeout_s>
# Polls a real state file for a field to reach a real value, instead of
# guessing a fixed sleep - ground-truthed live 2026-07-31: a fixed
# sleep that worked reliably run-by-run in isolated manual testing
# still intermittently failed when run back-to-back inside this script
# (real timing variance under load, not a logic bug) - polling for the
# actual state removes the guesswork entirely.
wait_for_field() {
    local file="$1" field="$2" want="$3" timeout="${4:-5}"
    local waited=0
    while [ "$waited" -lt "$((timeout * 5))" ]; do
        local got
        got="$(grep "^${field}=" "$file" 2>/dev/null | cut -d= -f2)"
        [ "$got" = "$want" ] && return 0
        sleep 0.2
        waited=$((waited + 1))
    done
    return 1
}

cleanup() {
    echo; echo "--- cleanup ---"
    bash "$SCRIPT_DIR/button.sh" kill >/dev/null 2>&1
}
trap cleanup EXIT INT TERM

echo "=== mutaclsym: real K3 smoke test - INTERACT movement + href nav, both new per-screen modules ==="

bash "$SCRIPT_DIR/button.sh" kill >/dev/null 2>&1
sleep 1
cd "$PROJECT_DIR"

# Real reset, mirroring ops/title_input.c's own reset_world() exactly -
# see this file's own header comment for why (no session isolation
# here, so stale hero state from a previous run is otherwise a silent
# false-failure risk indistinguishable from a real dispatch bug).
rm -rf pieces/world_01
cp -r pieces/world_01_template pieces/world_01

: > pieces/keyboard/history.txt
NO_GL=1 PAL_MODE=1 setsid bash button.sh run > "$PROOF_DIR/app_stdout.log" 2>&1 < /dev/null & disown

waited=0
while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 50 ]; do
    sleep 0.2; waited=$((waited + 1))
done
if [ ! -s pieces/display/current_frame.txt ]; then
    fail "app never rendered a real frame - see $PROOF_DIR/app_stdout.log"
    exit 1
fi
sleep 2
cp pieces/display/current_frame.txt "$PROOF_DIR/00_initial_frame.txt"
[ "$(cat pieces/display/current_layout.txt 2>/dev/null)" = "pieces/chtpm/layouts/game.chtpm" ] \
    && pass "launched real frame, landed on game.chtpm (its own new dedicated module)" \
    || fail "did not land on game.chtpm - see current_layout.txt"

# --- Real INTERACT movement: engage Control Hero, xlector-cursor to the
# hero's own tile, possess, then move - the real control model (see
# this file's own header comment), not a WASD/button shortcut. ---
BEFORE_POS="$(cat pieces/world_01/map_start/hero/state.txt 2>/dev/null | grep -E '^pos_x=|^pos_y=' | tr '\n' ' ')"
HERO_X=$(echo "$BEFORE_POS" | grep -oE 'pos_x=[0-9]+' | cut -d= -f2)
HERO_Y=$(echo "$BEFORE_POS" | grep -oE 'pos_y=[0-9]+' | cut -d= -f2)
echo "hero pos before: $BEFORE_POS (x=$HERO_X y=$HERO_Y)"

HERO_STATE="pieces/world_01/map_start/hero/state.txt"

focus "Control Hero"
key 13     # Enter on "Control Hero" (onClick="INTERACT") - sets interact_mode=1
wait_for_field "$HERO_STATE" "interact_mode" "1" 5
INTERACT_MODE="$(grep '^interact_mode=' "$HERO_STATE" 2>/dev/null | cut -d= -f2)"
[ "$INTERACT_MODE" = "1" ] && pass "Control Hero engaged (interact_mode=1)" \
    || fail "Control Hero did not engage interact_mode (got '$INTERACT_MODE')"

# Walk the free xlector cursor (starts at 0,0) onto the hero's own tile,
# one key at a time, verifying the real resulting position after EACH
# key rather than guessing a fixed sleep - see this file's own header
# comment on TIMING for why a rapid burst is unsafe here.
# ARROW_RIGHT=1001, ARROW_DOWN=1003 (system/keyboard_input.c's own codes).
for ((i=0; i<HERO_X; i++)); do
    key 1001
    wait_for_field "$HERO_STATE" "xlector_pos_x" "$((i + 1))" 3
done
for ((i=0; i<HERO_Y; i++)); do
    key 1003
    wait_for_field "$HERO_STATE" "xlector_pos_y" "$((i + 1))" 3
done

XLECTOR_POS="$(cat "$HERO_STATE" 2>/dev/null | grep -E '^xlector_pos_x=|^xlector_pos_y=' | tr '\n' ' ')"
echo "xlector pos after walking to hero tile: $XLECTOR_POS"

key 13     # Enter on the hero's own tile - try_possess_at() sets possessed_id=hero
wait_for_field "$HERO_STATE" "possessed_id" "hero" 5
POSSESSED="$(grep '^possessed_id=' "$HERO_STATE" 2>/dev/null | cut -d= -f2)"
[ "$POSSESSED" = "hero" ] && pass "real Enter-on-hero-tile possession worked (possessed_id=hero)" \
    || fail "possession did not engage (possessed_id='$POSSESSED') - xlector cursor may not be on the hero's own tile"

key 1001   # ARROW_RIGHT - now that hero is possessed, this should move the hero itself
wait_for_field "$HERO_STATE" "pos_x" "$((HERO_X + 1))" 5
key 27     # Esc - exit INTERACT (possession persists per ops/choice.c's own header comment)
sleep 0.5
cp pieces/display/current_frame.txt "$PROOF_DIR/01_after_move_frame.txt"
AFTER_POS="$(cat pieces/world_01/map_start/hero/state.txt 2>/dev/null | grep -E '^pos_x=|^pos_y=' | tr '\n' ' ')"
echo "hero pos after: $AFTER_POS"
if [ "$BEFORE_POS" != "$AFTER_POS" ]; then
    pass "real possessed-hero movement changed the real hero state (pos: '$BEFORE_POS' -> '$AFTER_POS') - game.chtpm's own new dedicated module dispatches real keys correctly"
else
    fail "hero position did not change after real ARROW_RIGHT while possessed (before='$BEFORE_POS' after='$AFTER_POS')"
fi

# --- Real href navigation: game.chtpm -> info_test.chtpm -> back ---
focus "Info (href test)"
key 13
sleep 1
cp pieces/display/current_frame.txt "$PROOF_DIR/02_info_screen_frame.txt"
[ "$(cat pieces/display/current_layout.txt 2>/dev/null)" = "pieces/chtpm/layouts/info_test.chtpm" ] \
    && pass "real href landed on info_test.chtpm (its own new dedicated module)" \
    || fail "did not land on info_test.chtpm after real href activation"
check "pieces/display/current_frame.txt" "real href test screen" "info_test.chtpm's own real content rendered (not stale/blank from the module transition)"

focus "Back to Game"
key 13
sleep 1
cp pieces/display/current_frame.txt "$PROOF_DIR/03_back_on_game_frame.txt"
[ "$(cat pieces/display/current_layout.txt 2>/dev/null)" = "pieces/chtpm/layouts/game.chtpm" ] \
    && pass "real href navigated back to game.chtpm cleanly" \
    || fail "did not land back on game.chtpm after real Back-to-Game href"

echo
echo "Proof: $PROOF_DIR"
if [ "$FAIL" = "0" ]; then echo "=== OVERALL: PASS ==="; else echo "=== OVERALL: FAIL ==="; fi
exit "$FAIL"
