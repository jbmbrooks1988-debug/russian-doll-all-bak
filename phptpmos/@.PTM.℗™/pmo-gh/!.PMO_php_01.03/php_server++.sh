#!/bin/bash
# php_server.sh - Run the CHTPM PHP Web Server
# TPM-Compliant: Uses dynamic path resolution (mirrors run_chtpm.sh)

# Use a more portable way to get the script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Set up location_kvp for dynamic path resolution
mkdir -p pieces/locations
cat > pieces/locations/location_kvp << EOF
project_root=${SCRIPT_DIR}
pieces_dir=${SCRIPT_DIR}/pieces
fuzzpet_app_dir=${SCRIPT_DIR}/pieces/apps/fuzzpet_app
fuzzpet_dir=${SCRIPT_DIR}/pieces/apps/fuzzpet_app/fuzzpet
system_dir=${SCRIPT_DIR}/pieces/system
EOF

# Ensure all needed directories exist (using macro folders) - POSIX compatible
mkdir -p pieces/locations
mkdir -p pieces/keyboard
mkdir -p pieces/display
mkdir -p pieces/chtpm/layouts
mkdir -p pieces/chtpm/plugins
mkdir -p pieces/chtpm/renderers
mkdir -p pieces/chtpm/frame_buffer
mkdir -p pieces/apps/fuzzpet_app/fuzzpet
mkdir -p pieces/apps/fuzzpet_app/keyboard
mkdir -p pieces/apps/fuzzpet_app/master_ledger
mkdir -p pieces/apps/fuzzpet_app/display
mkdir -p pieces/apps/fuzzpet_app/world
mkdir -p pieces/apps/fuzzpet_app/clock
mkdir -p pieces/system
mkdir -p pieces/master_ledger
mkdir -p pieces/ui_components
mkdir -p pieces/ui_instances

# Copy layout files from C version (mirrors C structure)
cat > pieces/chtpm/layouts/os.chtpm << 'LAYOUT_EOF'
<panel>
    <text label="╔═══════════════════════════════════════════════════════════╗" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║           C H T P M + O S   C L I   v2.0                 ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  CHTPM+OS Main Menu                                      ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  " /><button label="Help" onClick="LAUNCH:HELP" /><text label="                                            ║" /><br/>
    <text label="║  " /><button label="Launch FuzzPet" href="pieces/chtpm/layouts/fuzzpet_launcher.chtpm" /><text label="                                  ║" /><br/>
    <text label="║  " /><button label="Status" onClick="LAUNCH:STATUS" /><text label="                                          ║" /><br/>
    <text label="║  " /><button label="List Processes" onClick="LAUNCH:PS" /><text label="                                  ║" /><br/>
    <text label="║  " /><menu label="Settings">
        <button label="Toggle Frame History (Current: ${display_frame_history})" onClick="LAUNCH:TOGGLE_HISTORY" />
    </menu><text label="                                         ║" /><br/>
    <text label="║      Applications                                        ║" /><br/>
    <text label="╠═══════════════════════════════════════════════════════════╣" /><br/>
    <text label="║  Running Applications: None                               ║" /><br/>
    <text label="║  System Time: ${clock_time} | Turn: ${clock_turn}                          ║" /><br/>
    <text label="╠═══════════════════════════════════════════════════════════╣" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  $ " /><cli_io label="_" /><text label="                                                 ║" /><br/>
    <text label="║                                                           ║" /><br/>
</panel>
LAYOUT_EOF

cat > pieces/chtpm/layouts/fuzzpet_launcher.chtpm << 'LAYOUT_EOF'
<panel>
    <text label="╔═══════════════════════════════════════════════════════════╗" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║             F U Z Z P E T   L A U N C H E R               ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  Select an option to begin:                               ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  " /><button label="New Game (Reset Pet)" onClick="LAUNCH:FUZZPET_NEW" href="pieces/chtpm/layouts/main.chtpm" /><text label="                       ║" /><br/>
    <text label="║  " /><button label="Load Game (Continue)" onClick="LAUNCH:FUZZPET" href="pieces/chtpm/layouts/main.chtpm" /><text label="                           ║" /><br/>
    <text label="║  " /><button label="Back to Main Menu" href="pieces/chtpm/layouts/os.chtpm" /><text label="                                  ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="╚═══════════════════════════════════════════════════════════╝" /><br/>
</panel>
LAYOUT_EOF

cat > pieces/chtpm/layouts/main.chtpm << 'LAYOUT_EOF'
<panel>
    <text label="╔═══════════════════════════════════════════════════════════╗" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║             F U Z Z P E T   D A S H B O A R D             ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  Pet Name: ${fuzzpet_name}                                     ║" /><br/>
    <text label="║  Status: ${fuzzpet_status}                                         ║" /><br/>
    <text label="║  Turn: ${clock_turn} | Time: ${clock_time}                          ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  Hunger: ${fuzzpet_hunger}/100 | Happiness: ${fuzzpet_happiness}/100           ║" /><br/>
    <text label="║  Energy: ${fuzzpet_energy}/100 | Level: ${fuzzpet_level}                       ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  GAME MAP:                                                ║" /><br/>
    <text label="║  " /><button label="Control Map" onClick="INTERACT" /><text label="                                     ║" /><br/>
    <text label="${game_map}" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  " /><button label="Feed Pet" onClick="FEED" /><button label="Play" onClick="PLAY" /><button label="Sleep" onClick="SLEEP" /><text label="             ║" /><br/>
    <text label="║  " /><button label="Evolve" onClick="EVOLVE" /><button label="End Turn" onClick="END_TURN" /><text label="                      ║" /><br/>
    <text label="║  " /><link label="Exit to Main Menu" href="pieces/chtpm/layouts/os.chtpm" /><text label="                         ║" /><br/>
    <text label="║                                                           ║" /><br/>
    <text label="║  DEBUG [Last Key]: ${fuzzpet_last_key}                                    ║" /><br/>
</panel>
LAYOUT_EOF

# Clear history and ledger files for new session
> pieces/keyboard/history.txt
> pieces/apps/fuzzpet_app/fuzzpet/history.txt
> pieces/master_ledger/master_ledger.txt
> pieces/master_ledger/ledger.txt
> pieces/display/frame_changed.txt
> pieces/display/current_frame.txt

# Clear piece ledgers for new session
> pieces/keyboard/ledger.txt
> pieces/apps/fuzzpet_app/fuzzpet/ledger.txt
> pieces/apps/fuzzpet_app/world/ledger.txt
> pieces/apps/fuzzpet_app/clock/ledger.txt
> pieces/display/ledger.txt
> pieces/chtpm/ledger.txt

rm -f pieces/chtpm/state.txt

# Reset fuzzpet.pdl to ensure clean start
cat > pieces/apps/fuzzpet_app/fuzzpet/fuzzpet.pdl << EOF
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | fuzzpet
META         | version            | 1.0
META         | determinism        | strict

STATE        | name               | Fuzzball
STATE        | hunger             | 50
STATE        | happiness          | 75
STATE        | energy             | 100
STATE        | level              | 1
STATE        | pos_x              | 5
STATE        | pos_y              | 5
STATE        | status             | active
STATE        | alive              | true

METHOD       | feed               | void
METHOD       | play               | void
METHOD       | sleep              | void
METHOD       | status             | void
METHOD       | evolve             | void
METHOD       | move_up            | void
METHOD       | move_down          | void
METHOD       | move_left          | void
METHOD       | move_right         | void

EVENT_IN     | fed                | void
EVENT_IN     | played             | void
EVENT_IN     | slept              | void
EVENT_IN     | evolved            | void
EVENT_IN     | moved              | void
EVENT_IN     | command_received   | void

RESPONSE     | fed                | Yum yum~ Feeling so happy! *tail wags wildly*
RESPONSE     | played             | Wheee~ Best playtime ever!
RESPONSE     | slept              | Zzz... feeling recharged!
RESPONSE     | evolved            | Wow! I evolved and got stronger!
RESPONSE     | moved              | *moves around excitedly*
RESPONSE     | command_received   | *looks at you curiously*
RESPONSE     | default            | *confused tilt* ...huh?
EOF

# Reset clock/state.txt
cat > pieces/apps/fuzzpet_app/clock/state.txt << EOF
turn 0
time 08:00:00
EOF

# Reset world/map.txt
cat > pieces/apps/fuzzpet_app/world/map.txt << EOF
####################
#  ...............T#
#  ...............T#
#  ....R...@......T#
#  ....R..........T#
#  ....R..........T#
#  ....R..........T#
#  ................#
#                  #
####################
EOF

# Reset initial state.txt for fuzzpet
cat > pieces/apps/fuzzpet_app/fuzzpet/state.txt << EOF
name=Fuzzball
hunger=50
happiness=75
energy=100
level=1
pos_x=5
pos_y=5
status=active
last_key=None
clock_turn=0
clock_time=08:00:00
EOF

# Reset view.txt
cat > pieces/apps/fuzzpet_app/fuzzpet/view.txt << EOF
╠═══════════════════════════════════════════════════════════╣
║  [FUZZPET]: *moves around excitedly*                       ║
║  DEBUG [Last Key]: CODE None                               ║

  ####################
  #  ...............T#
  #  ...............T#
  #  ....R...@......T#
  #  ....R..........T#
  #  ....R..........T#
  #  ....R..........T#
  #  ................#
  #                  #
  ####################
EOF

# Find available port (start at 8001, increment if taken)
PORT=8001
while true; do
    if ! lsof -i :$PORT > /dev/null 2>&1; then
        break
    fi
    PORT=$((PORT + 1))
done

echo "Starting CHTPM PHP Web Server at http://localhost:$PORT"
cd "$SCRIPT_DIR"
php -S localhost:$PORT
