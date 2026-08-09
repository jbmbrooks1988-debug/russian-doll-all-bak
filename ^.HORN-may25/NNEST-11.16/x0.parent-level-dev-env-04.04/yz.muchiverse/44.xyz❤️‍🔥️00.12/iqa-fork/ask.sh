#!/usr/bin/env bash
# iqa-fork/ask.sh - Ask a Mac ollama model, print clean text.
# Usage: ask.sh <model> <prompt> [timeout_seconds]
set -u
MODEL="$1"
PROMPT="$2"
TMO="${3:-120}"
JP="/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/045.muchi-pal-agent🤖️+1/ops/+x/json_parser.+x"
TMP=$(mktemp)
ESCAPED=$(python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$PROMPT")
if ! timeout "$TMO" curl -s http://10.0.0.144:11434/api/chat \
    -d "{\"model\":\"$MODEL\",\"messages\":[{\"role\":\"user\",\"content\":$ESCAPED}],\"stream\":false}" > "$TMP" 2>/dev/null; then
    echo "ASK|ERROR|timeout_or_net"
    rm -f "$TMP"
    exit 1
fi
OUT=$("$JP" "$TMP" message.content 2>/dev/null)
rm -f "$TMP"
if [ -z "$OUT" ]; then echo "ASK|ERROR|empty_reply"; exit 1; fi
echo "$OUT"
