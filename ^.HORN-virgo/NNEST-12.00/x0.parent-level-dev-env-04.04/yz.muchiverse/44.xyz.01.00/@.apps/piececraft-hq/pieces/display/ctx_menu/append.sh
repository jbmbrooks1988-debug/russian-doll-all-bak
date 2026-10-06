#!/bin/sh
# CTX_<VERB> x y z id kind glyph template   ('_' = empty field)
V="$1"; X="$2"; Y="$3"; Z="$4"; ID="$5"; KIND="$6"; GLYPH="$7"; TMPL="$8"
[ "$V" != EXIT ] && printf 'CTX_%s %s %s %s %s %s %s %s\n'     "$V" "$X" "$Y" "$Z" "${ID:-_}" "${KIND:-_}" "${GLYPH:-_}" "${TMPL:-_}" >> "/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/piececraft-hq/pieces/system/widget_cmds/inbox.txt"
echo "$(date '+%H:%M:%S') click $V $X,$Y,$Z ${ID} ${KIND}" >> "/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/piececraft-hq/pieces/display/ctx_menu.log"
[ -n "/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/piececraft-hq" ] && printf 'ctx_visible=0\n' > "/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/piececraft-hq/state/ctx.txt"
for p in $(pgrep -f "khtpm_core_render.+x .*ctx-menu\.xhtpm" 2>/dev/null); do
    [ "$(cat /proc/$p/comm 2>/dev/null)" = khtpm_core_rend ] && kill "$p" 2>/dev/null
done
