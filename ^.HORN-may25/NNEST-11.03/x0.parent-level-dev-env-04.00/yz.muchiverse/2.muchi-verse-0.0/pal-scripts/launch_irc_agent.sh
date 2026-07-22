#!/bin/bash
# launch_irc_agent.sh - the real orchestration behind pal-chat-agent.pal
# (exec'd from it - prisc+x's own bytecode has no session-management
# capability of its own; this whole family's convention is op-calls-op,
# heavy lifting in C/bash, .pal scripts stay thin sequencers - matches
# main_loop_chtpm.pal itself delegating to send_message.c/check_
# response.c instead of hand-rolling everything in bytecode).
#
# Launches a real, session-isolated muchi-pal-agent instance the same
# way its own button.sh run action does (renderer + chtpm_parser_pal
# ONLY, never keyboard_input - this is a bot, not a human at a
# keyboard, the exact "ui_drive" backend principle CHAT-INTEGRATION-
# ARCHITECTURE.txt sec. 2 designed: "Launches the target's own
# renderer + chtpm_parser_pal ONLY"). chtpm_parser_pal's own <module>
# tag (pieces/chtpm/layouts/chat.chtpm) launches system/prisc+x pal/
# main_loop_chtpm.pal as ITS OWN child - do not also launch prisc+x
# here directly, that would start a second, competing instance against
# the same session state.
#
# Then injects "/irc-agent <room>" + Enter into that session's own
# gui_state.txt/interact_relay.txt - the exact technique this whole
# project was hand-verified with, all session (see MPC.txt/handoff.txt
# for the trace). The launched session is left running (irc_agent_
# poll.c's own tick loop keeps bridging into pal-chat-irc indefinitely,
# same as a human would leave a chat window open) - this script's own
# job is done once the command is injected.
#
# Room name comes from $IRC_ROOM (inherited automatically - OP_EXEC's
# own system() call passes through prisc+x's full environment, no pal
# bytecode env-plumbing needed), defaulting to "general" if unset.
#
# Usage: launch_irc_agent.sh (no args - set IRC_ROOM before running
# pal-chat-agent.pal if a different room is wanted)
set -e
ROOM="${IRC_ROOM:-general}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AGENT_DIR="$(cd "$SCRIPT_DIR/../1.muchi-pal-agent" && pwd)"

SESSION_ID="irc-agent-$(date +%s)-$$"
SESSION_DIR="$AGENT_DIR/pieces/sessions/$SESSION_ID"
mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
         "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
         "$SESSION_DIR/projects/muchi-pal-agent/manager"
mkdir -p "$AGENT_DIR/pieces/world_01"
ln -s "$AGENT_DIR/system" "$SESSION_DIR/system"
ln -s "$AGENT_DIR/ops" "$SESSION_DIR/ops"
ln -s "$AGENT_DIR/pal" "$SESSION_DIR/pal"
ln -s "$AGENT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
ln -s "$AGENT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
ln -s "$AGENT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry"
ln -s "$AGENT_DIR/pieces/world_01" "$SESSION_DIR/pieces/world_01"
ln -s "$AGENT_DIR/sandbox" "$SESSION_DIR/sandbox"

cd "$SESSION_DIR"
: > pieces/apps/player_app/interact_relay.txt
: > pieces/keyboard/history.txt
: > pieces/system/quit_flag.txt
: > pieces/display/frame_changed.txt
: > projects/muchi-pal-agent/manager/gui_state.txt
: > pieces/apps/player_app/cli_buffers.txt

cat > pieces/apps/player_app/state.txt << EOSTATE
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=muchi-pal-agent
active_target_id=chat
EOSTATE

export PRISC_PROJECT_ROOT="$SESSION_DIR"
export PRISC_PROJECT_ID="muchi-pal-agent"

nohup ./system/renderer > /dev/null 2>&1 &
nohup ./system/chtpm_parser_pal pieces/chtpm/layouts/chat.chtpm > /dev/null 2>&1 &

# Give chtpm_parser_pal a moment to spawn its own prisc+x module child
# before injecting - matches how a human would only start typing once
# the screen is actually up.
sleep 1

echo "message_input=/irc-agent $ROOM" > projects/muchi-pal-agent/manager/gui_state.txt
echo "13" >> pieces/apps/player_app/interact_relay.txt

echo "irc-agent-0000 launched (session $SESSION_ID), joining pal-chat-irc room '$ROOM'."
echo "Session dir: $SESSION_DIR"
echo "To stop it later: kill the renderer/chtpm_parser_pal/prisc+x processes whose cwd is that session dir, then rm -rf it."
