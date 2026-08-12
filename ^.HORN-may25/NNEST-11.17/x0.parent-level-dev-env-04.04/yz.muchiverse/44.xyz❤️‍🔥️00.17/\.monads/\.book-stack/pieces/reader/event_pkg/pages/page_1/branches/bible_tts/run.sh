#!/bin/bash
# Bible verse + TTS — calls external TTS handler on hard drive partition
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../../../../../.." && pwd)"
TTS_SCRIPT="/media/no/b7ced73c-5231-4462-b98d-64e38fe2df9e/home/jbez/Desktop/^.📶️.SHARE]/^.🦾️]fullsharezip/💪🏾️].no-desk.sharezip/!.🫁️.BIBLE.📔️]z3+/bible-ench.twins+ai]b2/bible.ch2en.ran/7.tts.sh"

# Show text first (same as bible_text branch)
bash "$SCRIPT_DIR/../bible_text/run.sh"

# Generate and play audio via external TTS script
LOGFILE="$HOUSE/.tts_debug.log"
echo "DEBUG: TTS_SCRIPT=$TTS_SCRIPT" >> "$LOGFILE"
if [ -f "$TTS_SCRIPT" ]; then
    echo "DEBUG: TTS script found, running..." >> "$LOGFILE"
    bash "$TTS_SCRIPT" >> "$LOGFILE" 2>&1
    EC=$?
    echo "DEBUG: TTS script exit code: $EC" >> "$LOGFILE"
else
    echo "ERROR: TTS script not found at $TTS_SCRIPT" >> "$LOGFILE"
fi
