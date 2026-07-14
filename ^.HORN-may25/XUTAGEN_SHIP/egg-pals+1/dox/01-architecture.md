# egg-pals — what's built, what's next

Read `egg-pals.txt` (one directory up, sibling to this project) first — it's
the original handoff and explains *why* each piece below exists and which
real reference code it's ported from. Note: `egg-pals.txt` describes an
earlier design where the faucet directly generated eggs — that changed by
direct instruction (see below); everything else in that document still
holds.

## The economy (as actually built, not as first drafted)

- **Faucet** = tokens only. `Claim Tokens` grants +10 tokens (no cooldown
  yet). `Coin Flip` stakes a fixed 10 tokens on a 50/50 double-or-nothing.
  The faucet never touches eggs directly.
- **Store** = spend tokens. `Buy Egg` costs 20 tokens and mints one
  (deducts tokens, then calls the existing `generate_egg` op to do the
  actual minting — reuses it rather than duplicating its logic).
- **Pets** = read-only catalog for now (lists owned eggs/pets with
  species/hatched status). Selecting an individual pet to open its GL
  window is not built yet — see step 2 below.

## What's built and verified working right now

Everything compiles clean (`./button.sh compile`, zero warnings). The whole
menu loop has been driven end-to-end via a pty test (navigate every screen,
claim tokens, lose a coin flip, buy an egg, see it appear in the catalog,
quit) and every number/message came out exactly as expected — this isn't
just "it builds," it's been actually exercised.

- **The menu loop itself**, same three-process shape as mutaclsym (no
  ncurses): `system/keyboard_input` (raw terminal, vendored unmodified —
  it's fully game-agnostic) + `system/prisc+x pal/main_loop.pal` (reads
  keys, dispatches to `menu_input` then `compose_menu` each turn) +
  `system/renderer` (vendored unmodified, polls and prints
  `current_frame.txt`, logs every frame to `frame_history.txt`). Run
  `./button.sh run` — `wasd`/arrows to move, `enter` to select, `b` to go
  back, `q` to quit.
- **`ops/menu_input.c`** — the router. Reads/writes
  `pieces/system/menu_state.txt` (`screen`, `cursor`, `last_message`).
  Handles cursor movement, screen transitions, and — on the actual action
  options — shells out to `claim_tokens`/`coin_flip`/`buy_egg` and stores
  their one-line result as `last_message`. Note the constraint that shaped
  this: prisc+x's generic custom-op dispatch only ever passes **one**
  argument to a handler (a register value OR a literal, never both — see
  `exec_custom_op()` in `prisc+x.c`), so the pal script can only hand this
  op the keycode; the owner piece id defaults to `"user_01"` inside the op
  itself. A real multi-user version needs a "current user" pointer read
  here instead of that hardcoded default.
- **`ops/compose_menu.c`** — the renderer op. Boxed-ASCII-panel look
  (`+===+` borders, letter-spaced centered titles, `>`-marked cursor rows)
  matching real TPMOS's `.chtpm` layout files
  (`pieces/chtpm/layouts/*.chtpm` in the 1.TPMOS reference tree) without
  adopting their markup engine — this is hand-printed text, same pattern as
  mutaclsym's `compose_frame.c`.
- **`ops/claim_tokens.c` / `ops/coin_flip.c` / `ops/buy_egg.c`** — each a
  self-contained single-verb op, each printing one result line to stdout
  for `menu_input` to relay. `buy_egg` is the one that calls another op
  (`generate_egg.+x`) via `popen` rather than reimplementing minting.
- **`ops/generate_egg.c`** — the underlying mint primitive (unchanged from
  before): weighted-random species pick from
  `pieces/registry/emoji_pool/common_emoji.txt` (common ~10x more likely
  than rare), a real incrementing serial number
  (`pieces/system/serial_counter.txt`), piece creation, inventory update.
  Still directly runnable standalone (`./button.sh demo`), but now also
  reachable through the Store in the real menu.
- **`system/emoji_gen_atlas.c` + `system/emoji_xtract.c`** — the emoji
  glyph → PNG → plain-text pixel CSV pipeline, vendored from 1.TPMOS and
  verified working (`./system/emoji_gen_atlas "🐸" /tmp/frog.png` → real
  64x64 RGBA PNG → `./system/emoji_xtract` → correct downsampled CSV). Not
  wired into hatching yet — see step 1.
- **`system/egg_window.c`** — vendored from `mutaclsym/!.shape=on.0.0Ⓜ️/
  shape-a0.c`. Compiles and opens a real, working, draggable, borderless,
  circularly-shaped GL window today (`./system/egg_window`) — confirmed it
  actually opens against this machine's live X display, not just that it
  compiles. Still a circle with a flat color fill, and not wired to the
  menu's Pets screen yet — see steps 2-3.

Run `./button.sh check` to confirm every binary is built,
`./button.sh run` to play the real menu, `./button.sh demo` for the
lower-level standalone mint.

## What's NOT built yet (in the order to build it)

1. **Hatching.** `generate_egg`/`buy_egg` mint a piece with `type=egg`,
   `hatched=0`, and already has the species/emoji fields it needs. Build
   `ops/hatch_egg.c` (same self-contained pattern as the other ops): flips
   `type=egg` → `type=pet`, `hatched=0` → `hatched=1`, and — this is where
   the emoji pipeline (already proven working above) actually gets used
   for real — runs the piece's `species_emoji` through `emoji_gen_atlas`
   then `emoji_xtract`, writing the resulting CSV into that pet's own
   piece directory (e.g. `pieces/world_01/map_lobby/<pet_id>/sprite.csv`).
   Wire a "Hatch" option into the Pets screen once each item's own
   `hatched` state can toggle (compose_menu already reads and displays
   that field per-pet, it just isn't selectable yet).

2. **Per-pet selection on the Pets screen**, spawning `egg_window` for
   that specific piece id. Right now Pets is list-only (just a Back
   option) — extending `menu_input.c`'s option count/dispatch for the pets
   screen to be per-row instead of fixed-at-1 is the concrete next change,
   same pattern already used for the other screens.

3. **Egg shape + sprite texture, not a circle + solid fill.**
   `system/egg_window.c` has a header comment marking exactly where to
   change the `XFillArc` mask call for a real egg silhouette (unequal
   width/height arcs, or a hand-built point list via `XFillPolygon`), and
   `egg-pals.txt` §3 explains the technique in more depth including a
   pointer to `wraith_gl.c` in 1.TPMOS for the more complete
   texture-loading/mouse-hit-testing pattern (load a pixel buffer via
   `glTexImage2D`, map window coordinates back to texture coordinates).
   The window needs to read the selected pet's `sprite.csv` (from step 1)
   and draw it as the GL texture instead of the current flat color fill.

4. **The FSM/behavior layer** (per `egg-pals.txt` §5 and mutaclsym's own
   `dox/01-cdda-architecture.md` §4) — only worth building once there's a
   hatched pet piece to attach it to (step 1). Don't build this before
   step 1 exists; there's nothing to drive it yet.

## Conventions already established here — keep following them

- No shared headers. Every new `ops/*.c` copies the ~15-line
  `resolve_root()`/`PATH_BUF` boilerplate from `generate_egg.c` rather
  than factoring it out.
- Plain pipe-delimited or `key=value` text everywhere, including pixel
  data (the CSV format, not a binary blob) — matches "if it's not in a
  file, it's a lie."
- Registry data (`pieces/registry/*/`) is pure data files, never requires
  a code change to extend — `common_emoji.txt` is the template.
- Every op that needs a bounded string copy: use `snprintf(dest,
  sizeof(dest), "%s", src)`, not `strncpy` (the latter trips
  `-Wstringop-truncation` under `-std=c11 -Wall -Wextra` even when
  followed by manual null-termination — this was hit and fixed in
  mutaclsym's `system/prisc+x.c`, don't rediscover it here).
- A pal script needs an *unconditional* `compose_menu` + `hit_frame` call
  before its main loop starts, not only inside the per-keypress branch —
  otherwise a truly fresh boot (no leftover `current_frame.txt` from a
  prior run) shows nothing until the first keypress. Hit this bug and
  fixed it here (and retroactively in mutaclsym) — `pal/main_loop.pal`
  already has the fix, keep it there if the script gets rewritten.
- prisc+x's generic custom-op dispatch passes exactly one argument to a
  handler (see `menu_input.c`'s own header comment) — don't design a new
  op assuming it can receive two pal-supplied values at once; route
  through a hardcoded default or a pointer file instead.
- Keep `./button.sh compile` at **zero warnings**. It is right now —
  don't let that slip as new ops get added.
