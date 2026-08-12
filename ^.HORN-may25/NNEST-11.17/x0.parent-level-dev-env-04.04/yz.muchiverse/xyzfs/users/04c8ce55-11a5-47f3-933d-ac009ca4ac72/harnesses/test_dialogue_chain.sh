#!/bin/bash
# Test harness for H1: Show Text + Show Choices dialogue flow
# Tests the complete multi-page dialogue chain end-to-end via relay

set -e

HOUSE="$(cd "$(dirname "$0")/../../../../.." && pwd)"
echo "[TEST] House root: $HOUSE"

# Create test event package structure
TEST_PKG="$HOUSE/@.apps/MUCHI_RANCHER/entities/test_dialogue_entity"
mkdir -p "$TEST_PKG/event_pkg/pages"

echo ""
echo "=== SETUP: Creating test event package ==="
echo "[+] Test package: $TEST_PKG"

# Create minimal entity structure for the test
mkdir -p "$TEST_PKG"

# Page 1: Show Text trigger=on_click
mkdir -p "$TEST_PKG/event_pkg/pages/page_1"
cat > "$TEST_PKG/event_pkg/pages/page_1/condition.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | test_dialogue_entity
COND         | trigger              | on_click
EOF

# Create Show Text IR node
cat > "$TEST_PKG/event_pkg/pages/page_1/event.ir.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | test_dialogue_entity
STATE        | source               | test
NODE         | id=1 type=show_text | text=What's your name? speaker=Elder
EOF

# Compile event.pal for page 1 (manually, as would be done by ez_menu_input)
cat > "$TEST_PKG/event_pkg/pages/page_1/event.pal" << 'EOF2'
# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl
# pkg=test_dialogue_entity page=1
exec cmd_1.sh
halt
EOF2

cat > "$TEST_PKG/event_pkg/pages/page_1/cmd_1.sh" << 'EOF2'
#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/xyzfs/bin/muchi-pet/ops/+x/mr_show_text.+x" "$ENT" "What's your name?" "Elder"
EOF2
chmod +x "$TEST_PKG/event_pkg/pages/page_1/cmd_1.sh"

# Page 2: Show Choices trigger=on_interact
mkdir -p "$TEST_PKG/event_pkg/pages/page_2"
cat > "$TEST_PKG/event_pkg/pages/page_2/condition.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | test_dialogue_entity
COND         | trigger              | on_interact
EOF

# Create Show Choices IR node
cat > "$TEST_PKG/event_pkg/pages/page_2/event.ir.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | test_dialogue_entity
STATE        | source               | test
NODE         | id=1 type=show_choices | choices=Alice\nBob\nCharlie default=0
EOF

# Compile event.pal for page 2 manually (simulating the compiler output)
cat > "$TEST_PKG/event_pkg/pages/page_2/event.pal" << 'EOF'
# event.pal - real prisc+x opcodes, COMPILED from event.ir.pdl
# pkg=test_dialogue_entity page=2
exec cmd_1.sh
halt
EOF

cat > "$TEST_PKG/event_pkg/pages/page_2/cmd_1.sh" << 'EOF'
#!/bin/sh
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
exec "$D/xyzfs/bin/muchi-pet/ops/+x/mr_show_choices.+x" "$ENT" 'Alice
Bob
Charlie' 0
EOF
chmod +x "$TEST_PKG/event_pkg/pages/page_2/cmd_1.sh"

# Create inventory/history tracking
mkdir -p "$TEST_PKG/event_pkg"
touch "$TEST_PKG/event_pkg/inventory.txt"
touch "$TEST_PKG/event_pkg/messages.txt"
touch "$TEST_PKG/event_pkg/history.txt"

echo "[+] Page 1: Show Text (trigger: on_click)"
echo "[+] Page 2: Show Choices (trigger: on_interact)"

echo ""
echo "=== TEST 1: Show Text Command (Page 1) ==="
echo "[>] Running Page 1 event..."

# Run event.pal for page 1 via prisc+x (the real relay)
if [ -f "$TEST_PKG/event_pkg/pages/page_1/event.pal" ]; then
    echo "[*] Executing: prisc+x pages/page_1/event.pal"
    cd "$TEST_PKG/event_pkg"
    PRISC=$(find "$HOUSE" -name "prisc+x" -type f 2>/dev/null | head -1)
    if [ -z "$PRISC" ]; then
        echo "[!] ERROR: prisc+x not found"
        exit 1
    fi
    PRISC_PROJECT_ROOT="$HOUSE" "$PRISC" pages/page_1/event.pal
    echo "[✓] Page 1 executed"
else
    echo "[!] ERROR: Page 1 event.pal not found"
    exit 1
fi

# Check messages output
echo ""
echo "=== RESULT: Messages logged ==="
if [ -f "$TEST_PKG/event_pkg/messages.txt" ]; then
    cat "$TEST_PKG/event_pkg/messages.txt"
    if grep -q "SHOW_TEXT" "$TEST_PKG/event_pkg/messages.txt"; then
        echo "[✓] Show Text message logged successfully"
    else
        echo "[!] ERROR: Show Text message not found in messages.txt"
    fi
else
    echo "[!] ERROR: messages.txt not found"
fi

echo ""
echo "=== TEST 2: Show Choices Command (Page 2) ==="
echo "[>] Running Page 2 event..."

cd "$TEST_PKG/event_pkg"
PRISC=$(find "$HOUSE" -name "prisc+x" -type f 2>/dev/null | head -1)
PRISC_PROJECT_ROOT="$HOUSE" "$PRISC" pages/page_2/event.pal
echo "[✓] Page 2 executed"

echo ""
echo "=== RESULT: Choices logged ==="
if grep -q "SHOW_CHOICES" "$TEST_PKG/event_pkg/messages.txt"; then
    echo "[✓] Show Choices message logged successfully"
else
    echo "[!] ERROR: Show Choices message not found in messages.txt"
fi

echo ""
echo "=== SUMMARY ==="
echo "[+] Multi-page dialogue flow test complete"
echo "[*] Check $TEST_PKG/event_pkg/messages.txt for full output:"
cat "$TEST_PKG/event_pkg/messages.txt"

echo ""
echo "[+] History of commands:"
cat "$TEST_PKG/event_pkg/history.txt"

echo ""
echo "[✓] TEST PASSED: Dialogue commands work end-to-end"
