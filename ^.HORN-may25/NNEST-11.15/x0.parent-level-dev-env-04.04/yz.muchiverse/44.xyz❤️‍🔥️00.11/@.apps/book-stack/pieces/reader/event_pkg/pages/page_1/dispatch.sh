#!/bin/bash
# dispatch.sh - real Show Choices orchestration for book-stack's own
# page_1. prisc+x's own real exec opcode only supports ONE literal arg
# (confirmed this session, !.HOUSE_STDS.md §H.5.1) and has no way to
# capture a command's own stdout into a PAL register - so the real
# "call khtpm_show_choices, branch on its result" logic lives here, in
# a real shell wrapper, not in event.pal itself. event.pal's own exec
# line just calls this ONE script with zero extra args.
# 2026-08-07: paths were a byte-identical copy of 00.10's, so they
# pointed at 44.xyz❤️‍🔥️00.10 (correct only for 00.10 itself) - "read
# now" launched the OTHER house's picker/entity and its context menu
# closed with nothing shown. Made portable/house-rooted like the
# branches' own run.sh pattern (SCRIPT_DIR -> HOUSE), so this can never
# drift again no matter where the house lives.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$SCRIPT_DIR/../../../../../../.." && pwd)"
PACKAGE_DIR="$HOUSE/#.desktop/entities/book-stack"
CHOICES_FILE="$HOUSE/@.apps/book-stack/pieces/reader/event_pkg/pages/page_1/choices.objects.pdl"
PICKED=$("$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_choices.+x" "$PACKAGE_DIR" "$CHOICES_FILE")
case "$PICKED" in
    bible_text) bash "$SCRIPT_DIR/branches/bible_text/run.sh" ;;
    bible_tts)  bash "$SCRIPT_DIR/branches/bible_tts/run.sh" ;;
    tao)        bash "$SCRIPT_DIR/branches/tao/run.sh" ;;
esac
