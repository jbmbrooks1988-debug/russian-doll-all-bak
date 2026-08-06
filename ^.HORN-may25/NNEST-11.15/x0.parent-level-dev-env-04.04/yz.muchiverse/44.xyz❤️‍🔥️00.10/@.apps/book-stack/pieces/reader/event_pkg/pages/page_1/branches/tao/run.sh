#!/bin/bash
# Real "Tao Te Ching (random chapter)" branch - real chapter-boundary
# scan (awk, a real standard tool) against the real source text, picks
# one real chapter at random, real Show Text displays it.
TAO_FILE="/media/no/b7ced73c-5231-4462-b98d-64e38fe2df9e/home/jbez/Desktop/^.📶️.SHARE]/^.🦾️]fullsharezip/💪🏾️].no-desk.sharezip/!.🫁️.BIBLE.📔️]z3+/tao-te-ching-tty/tao-te-ching]a1.txt"
N_CHAPTERS=$(grep -c "^Chapter " "$TAO_FILE")
PICK=$(( (RANDOM % N_CHAPTERS) + 1 ))
TMP=$(mktemp --suffix=.txt)
awk -v n="$PICK" '
  /^Chapter / { c++; if (c==n) { p=1; next } else if (p) { exit } }
  p { print }
' "$TAO_FILE" | fold -s -w 70 > "$TMP"
"/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/&.widgits/tile-picker/ops/+x/khtpm_show_text.+x" "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/#.desktop/entities/book-stack" "$TMP"
