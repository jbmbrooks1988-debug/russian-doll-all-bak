#!/bin/bash
# dispatch.sh - real Show Choices orchestration for book-stack's own
# page_1. prisc+x's own real exec opcode only supports ONE literal arg
# (confirmed this session, !.HOUSE_STDS.md §H.5.1) and has no way to
# capture a command's own stdout into a PAL register - so the real
# "call khtpm_show_choices, branch on its result" logic lives here, in
# a real shell wrapper, not in event.pal itself. event.pal's own exec
# line just calls this ONE script with zero extra args.
PACKAGE_DIR="/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/#.desktop/entities/book-stack"
CHOICES_FILE="/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/@.apps/book-stack/pieces/reader/event_pkg/pages/page_1/choices.objects.pdl"
PICKED=$("/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/&.widgits/tile-picker/ops/+x/khtpm_show_choices.+x" "$PACKAGE_DIR" "$CHOICES_FILE")
case "$PICKED" in
    bible_text) "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/@.apps/book-stack/pieces/reader/event_pkg/pages/page_1/branches/bible_text/run.sh" ;;
    bible_tts)  "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/@.apps/book-stack/pieces/reader/event_pkg/pages/page_1/branches/bible_tts/run.sh" ;;
    tao)        "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.15/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.10/@.apps/book-stack/pieces/reader/event_pkg/pages/page_1/branches/tao/run.sh" ;;
esac
