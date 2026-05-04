#!/bin/bash
# run.sh - Start PyOS-TPM Web Development Server

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PORT=8000

echo "========================================"
echo "  PyOS-TPM Web Edition"
echo "  Starting PHP development server..."
echo "========================================"
echo ""
echo "  Open: http://localhost:$PORT"
echo "  Directory: $SCRIPT_DIR"
echo ""
echo "  Press Ctrl+C to stop the server"
echo "========================================"
echo ""

php -S localhost:$PORT
