#!/bin/bash
# button.sh - $.crypts real house-wide autostart control.
# Direct instruction: "i want the ability thru button.sh to run this
# script manually also, and the .pdl with the app paths 2 run, should
# also have an 'on/off' option incase i want it to stop auto running."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"
PDL="$SCRIPT_DIR/autostart.pdl"
BIN="$SCRIPT_DIR/ops/+x/crypt_autostart.+x"
RESTORE="$SCRIPT_DIR/restore-list.txt"

read_restore_mode() {
    awk -F'|' '
        $1 ~ /^STATE[[:space:]]*$/ {
            gsub(/[[:space:]]+/, "", $2);
            gsub(/[[:space:]]+/, "", $3);
            if ($2 == "restore-last-open") { print $3; found=1; exit }
        }
        END { if (!found) print "0" }
    ' "$PDL"
}

case "$ACTION" in
    run|r|start|restart)
        # restart == run: use the restore feature only when explicitly enabled in autostart.pdl
        if [ "$(read_restore_mode)" = "1" ] && [ -f "$RESTORE" ] && [ -x "$SCRIPT_DIR/scrypts/openall/run.sh" ]; then
            "$SCRIPT_DIR/scrypts/openall/run.sh"
        else
            mkdir -p "$SCRIPT_DIR/ops/+x"
            [ -x "$BIN" ] || gcc -Wall -O2 -o "$BIN" "$SCRIPT_DIR/ops/crypt_autostart.c"
            "$BIN" "$PDL"
        fi
        ;;
    on)
        sed -i 's/^STATE        | enabled              | 0/STATE        | enabled              | 1/' "$PDL"
        echo "autostart: ON"
        ;;
    off)
        sed -i 's/^STATE        | enabled              | 1/STATE        | enabled              | 0/' "$PDL"
        echo "autostart: OFF"
        ;;
    status)
        grep "enabled" "$PDL"
        ;;
    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        gcc -Wall -O2 -o "$BIN" "$SCRIPT_DIR/ops/crypt_autostart.c" && echo "OK crypt_autostart" || echo "FAIL crypt_autostart"
        ;;
    check)
        [ -x "$BIN" ] && echo "OK $BIN" || echo "MISSING $BIN"
        [ -f "$PDL" ] && echo "OK $PDL" || echo "MISSING $PDL"
        ;;
    install-xdg)
        mkdir -p "$HOME/.config/autostart"
        cat > "$HOME/.config/autostart/muchiverse-autostart.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Muchiverse Autostart
Exec=$SCRIPT_DIR/button.sh run
X-GNOME-Autostart-enabled=true
EOF
        echo "installed: $HOME/.config/autostart/muchiverse-autostart.desktop"
        ;;
    help|h|-h|--help|*)
        cat <<EOF
\$.crypts — house-wide autostart control

  sh button.sh run            # quit current livedesk, then mount+launch (autostart.pdl)
  sh button.sh restart        # same as run (clean restart for $ shortcut / focus tests)
  sh button.sh on | off       # toggle STATE|enabled in autostart.pdl
  sh button.sh status         # show current enabled state
  sh button.sh compile        # rebuild ops/+x/crypt_autostart.+x
  sh button.sh check          # verify binary + pdl exist
  sh button.sh install-xdg    # install the real XDG autostart .desktop file
                               # (real login-time autostart - one-time setup)
EOF
        ;;
esac
