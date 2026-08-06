#!/bin/bash
# Real "Bible verse (with TTS)" branch - same real verse pick as
# bible_text, PLUS real TTS playback of the Chinese text (same real
# edge-tts + mpg123 pipeline 7.tts.sh already uses, reused not
# reinvented).
cd "/media/no/b7ced73c-5231-4462-b98d-64e38fe2df9e/home/jbez/Desktop/^.📶️.SHARE]/^.🦾️]fullsharezip/💪🏾️].no-desk.sharezip/!.🫁️.BIBLE.📔️]z3+/bible-ench.twins+ai]b2/bible.ch2en.ran" || exit 1
OUT=$(./bible_verses --short bible.en_translation.txt "bible]ch.txt" 2>/dev/null)
TMP=$(mktemp --suffix=.txt)
echo "$OUT" | fold -s -w 70 > "$TMP"
"/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x" "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/#.desktop/entities/book-stack" "$TMP"
CHINESE_TEXT=$(echo "$OUT" | grep "^Chinese: " | sed 's/^Chinese: //')
if [ -n "$CHINESE_TEXT" ]; then
    MP3=$(mktemp --suffix=.mp3)
    edge-tts --voice zh-CN-XiaoxiaoNeural --text "$CHINESE_TEXT" --write-media "$MP3" > /dev/null 2>&1
    if [ -f "$MP3" ]; then mpg123 -q "$MP3"; rm -f "$MP3"; fi
fi
