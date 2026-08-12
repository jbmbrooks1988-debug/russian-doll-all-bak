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
# REAL FIX 2026-08-10, direct report ("read still not showing the 3
# book options"): PACKAGE_DIR used to be hardcoded to the dev-tree
# entity location - correct only as long as book-stack actually ran
# from there. The pals migration moved the LIVE entity to
# xyzfs/users/<uuid>/home/livedesk/pals/book-stack; prisc+x's own exec
# opcode only supports ONE literal arg (see header comment above), so
# there was no way to pass the real live location through event.pal's
# "exec dispatch.sh" call at all - this script just kept guessing the
# stale dev-tree path, silently pointing khtpm_show_choices.+x at the
# wrong (or non-live) package, so no choices ever appeared. Fix: read
# PACKAGE_DIR from the environment if the caller (meta.pdl's "Read"
# method) exported it - exec inherits environment naturally even though
# it can't pass extra positional args. Falls back to the old dev-tree
# guess only if launched some other way that doesn't set it.
PACKAGE_DIR="${PACKAGE_DIR:-$HOUSE/*.monads/*.book-stack/entities/book-stack}"
CHOICES_FILE="$HOUSE/*.monads/*.book-stack/pieces/reader/event_pkg/pages/page_1/choices.objects.pdl"
PICKED=$("$HOUSE/&.widgits/tile-picker/ops/+x/khtpm_show_choices.+x" "$PACKAGE_DIR" "$CHOICES_FILE")
case "$PICKED" in
    bible_text) bash "$SCRIPT_DIR/branches/bible_text/run.sh" ;;
    bible_tts)  bash "$SCRIPT_DIR/branches/bible_tts/run.sh" ;;
    tao)        bash "$SCRIPT_DIR/branches/tao/run.sh" ;;
esac
