pal-scripts/ - meta-orchestration layer for the muchi-verse pal family
================================================================================
Status: LIVE, first script built and live-verified 2026-07-20.

This is CHAT-INTEGRATION-ARCHITECTURE.txt sec. 6's own "llm scripts/
folder" - an OUTER layer, separate from any one project, that launches
and coordinates real pal-family instances from the outside. Plain .pal
scripts run here via this directory's own system/prisc+x (built from
shared-ops/prisc+x.c - see scripts/build.sh), with the heavy lifting
delegated to plain bash, matching the whole family's own op-calls-op
convention (a .pal script is a thin sequencer, not where real logic
lives - main_loop_chtpm.pal itself delegates to send_message.c/check_
response.c the same way).

========================================================================
WHAT'S HERE
========================================================================
  system/prisc+x       - synced from shared-ops/prisc+x.c (re-sync via
                          scripts/build.sh whenever the shared VM
                          changes - this copy is NOT auto-updated).
  default_op.txt        - empty on purpose (pal-chat-agent.pal only
                          uses built-in opcodes, no custom ops) - kept
                          as a real file only to silence prisc+x's own
                          "could not open ops file" stderr line.
  pal-chat-agent.pal     - the entry point (see below).
  launch_irc_agent.sh     - the real orchestration logic pal-chat-
                          agent.pal execs into.

========================================================================
pal-chat-agent.pal - launch a muchi-pal-agent instance running as an
                     auto-responding bot inside a pal-chat-irc room
========================================================================
Direct user ask, 2026-07-20: "we could have a pal script 'irc-agent-
0000' inside the chat that hooks up to chat, says hi and waits for new
messages and responds to everything from random... a new dir 'pal-
scripts/' and running pal-chat-agent.pal will navigate to and start
the muchi-pal-agent, and start the irc-agent-0000 script within the
agent."

USAGE:
  cd pal-scripts
  IRC_ROOM=<room> ./system/prisc+x pal-chat-agent.pal

  IRC_ROOM defaults to "general" if unset. It has to be passed as an
  environment variable, not a script argument - prisc+x's own `exec`
  opcode can only pass INTEGER arguments through registers, never
  arbitrary text, so env-var inheritance (system()'s child
  automatically gets prisc+x's own environment) is the only clean way
  to hand a room name through. See pal-chat-agent.pal's own header
  comment.

WHAT ACTUALLY HAPPENS (all delegated to launch_irc_agent.sh, since
prisc+x bytecode has no session-management opcodes of its own -
mkdir/symlink/backgrounding a process aren't things li/beq/j express):
  1. Launches a real, SESSION-ISOLATED muchi-pal-agent instance
     (pieces/sessions/<id>/, torn-down-on-request, never touching the
     project's own shared/persistent files) - renderer + chtpm_
     parser_pal ONLY, never keyboard_input. This is a bot, not a human
     at a keyboard - the "ui_drive" backend principle CHAT-
     INTEGRATION-ARCHITECTURE.txt sec. 2 designed ("Launches the
     target's own renderer + chtpm_parser_pal ONLY"). chtpm_parser_
     pal's own <module> tag (pieces/chtpm/layouts/chat.chtpm) spawns
     system/prisc+x pal/main_loop_chtpm.pal as ITS OWN child - launch_
     irc_agent_sh does NOT also launch prisc+x separately (that would
     start a second, competing instance against the same session
     state).
  2. Waits 1s for the screen to come up, then injects "/irc-agent
     <room>" + Enter into that session's own gui_state.txt/interact_
     relay.txt - the exact file-injection technique this whole
     project was hand-verified with, all session (writing gui_state's
     message_input field + appending "13" to interact_relay.txt,
     matching real cli_io's own Enter-to-submit mechanism).
  3. The launched session is left RUNNING - it is not torn down when
     this script exits. irc_agent_poll.c's own tick loop (called every
     ~30ms from main_loop_chtpm.pal, see 1.muchi-pal-agent/pal/
     main_loop_chtpm.pal) keeps bridging into pal-chat-irc
     indefinitely, same as a human would leave a chat window open.

WHAT "/irc-agent <room>" ACTUALLY TURNS ON, inside muchi-pal-agent
itself (1.muchi-pal-agent/ops/irc_agent_poll.c has the full design
comment - this is the short version):
  - Uses pal-chat-irc's own DIRECT_OPS backend (CHAT-INTEGRATION-
    ARCHITECTURE.txt sec. 2/3) - shells straight into pal-chat-irc's
    own chat_create_user.+x/chat_post_message.+x and reads its own
    rooms/<room>/messages.txt directly. No pal-chat-irc UI/chtpm
    involvement at all - the right choice for a background bot doing
    bulk polling, not something that needs to look like a human
    pressing keys (sec. 2's own "WHEN TO USE WHICH" guidance).
  - FIRST activation for a room: creates the "irc-agent-0000" user
    (idempotent - refuses silently on every activation after the
    first), records the room's CURRENT message count as a starting
    point (so old history is never replied to), posts one greeting.
  - EVERY tick after: any new message NOT authored by irc-agent-0000
    itself gets a random 5-10 word reply, built from the wordbank
    muchi-pal-agent's own /model script mode is configured with
    (falls back to pieces/registry/wordbanks/sample.txt if a network
    provider is active instead).
  - "/irc-agent off" (typed directly into that muchi-pal-agent
    session, or via a second connector-style injection) stops the
    polling without losing the room's own join-state - turning it
    back on for the SAME room resumes without re-greeting.
  - Target pal-chat-irc root: PRISC_IRC_TARGET_ROOT env override,
    falling back to a hardcoded absolute sibling path - same "override
    a hardcoded default" pattern already used family-wide for this
    exact class of problem (palnet_peer.c's own PRISC_NET_ROOT,
    zoo_0000/button.sh's own PRISC_EXCHANGE_ROOT shim) - session
    isolation means muchi-pal-agent's own PRISC_PROJECT_ROOT is
    usually a pieces/sessions/<id>/ dir, so a naive "../pal-chat-irc"
    relative path would silently resolve to nonsense.

STOPPING A RUNNING BOT SESSION: launch_irc_agent.sh prints the session
dir it created. To stop it: find the renderer/chtpm_parser_pal/
prisc+x processes whose cwd is that session dir (cwd-match, NEVER a
bare `pkill -f` - argv text is identical across every session, same
gotcha documented in !.pal-standards.txt sec. 23), kill -9 them, then
rm -rf the session dir. A real connector_session_kill.+x op (CHAT-
INTEGRATION-ARCHITECTURE.txt sec. 2) would automate this - not built
yet, this whole directory is a first, deliberately small vertical
slice (sec. 7's own "prove one thing end-to-end before generalizing"
discipline).

LIVE-VERIFIED 2026-07-20: `IRC_ROOM=muchi-test ./system/prisc+x pal-
chat-agent.pal` really did launch muchi-pal-agent, join pal-chat-irc,
create the irc-agent-0000 user, and post a greeting. Posting a message
as a second user ("human-tester") got a real auto-reply within ~1s,
with no double-replies and no self-reply loop (irc_agent_state.txt's
own last_line_count settled correctly at 3, matching greeting + human
message + one reply).

========================================================================
TWO VM BUGS FIXED WHILE BUILDING THIS (shared-ops/prisc+x.c)
========================================================================
Both are the SAME root cause: an opcode's assembly-syntax parser left
a register field (rs1/rs2) at its raw zero-initialized default instead
of an explicit "not given" sentinel, and the runtime code treated
"register 0" and "no register given" as the same thing - they aren't,
since register 0 is hardwired-reset-to-0 every single instruction
(RISC-V convention, confirmed by reading this VM's own main loop).

  - OP_READ_HISTORY: the literal-path form (`read_history <path> xD,
    xS`) never actually parsed the position register (xS) - it
    silently defaulted to register 0, so any script using this form to
    incrementally tail a file (main_loop_chtpm.pal's own Enter-key
    read) got permanently stuck re-reading file position 0 forever.
    This WAS this project's original "typing one message sends five
    fragmented sends" bug. The fix already existed in shared-ops/
    prisc+x.c (a 3-argument form parsed BEFORE the broken 1-register
    fallback) - 1.muchi-pal-agent's own local copy was just stale and
    had never been re-synced since that fix landed. Re-copying it in
    and rebuilding was the actual fix, not new code.
  - OP_EXEC: same bug, but genuinely NEW (grep-confirmed: no pal
    script anywhere in the family had used `exec` before pal-chat-
    agent.pal) - any exec call with fewer than 3 total tokens (e.g.
    this script's own `exec ./launch_irc_agent.sh`) got spurious
    trailing "0"/"0 0" arguments silently appended to the shell
    command. Fixed with the same explicit -1 sentinel pattern already
    used elsewhere in the same parsing function.

Both fixes are in shared-ops/prisc+x.c and have been synced into
1.muchi-pal-agent/system/ and pal-scripts/system/ (this directory).
They have NOT yet been re-synced into every other consumer (mutaclsym,
zoo_0000, pal-chain, wsr-pal, muchipal-editor-0.0) - see shared-ops-
manifest.txt for the full consumer list. The fixes are purely additive
(no existing working call shape changes behavior), so syncing them
elsewhere is safe whenever those projects are next touched, just not
done yet as of this writing.
