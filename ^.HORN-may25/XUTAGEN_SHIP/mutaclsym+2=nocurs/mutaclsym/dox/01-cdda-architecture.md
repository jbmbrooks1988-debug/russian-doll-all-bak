# mutaclsym — CDDA feature architecture & roadmap

Handoff document. If you are picking this project up cold: read this file
first, then the three source docs listed in §1, then read the actual code
under `system/`, `ops/`, `pal/` — this document explains the *pattern* for
extending the game, it does not restate what the code already shows.

Primary goal right now, per direct instruction: **implement actual CDDA
(Cataclysm: Dark Days Ahead) game mechanics and content**, using the
established TPMOS / "russian doll" architecture. Multiplayer, mods, farming,
crafting-beyond-CDDA, and civilization features are real future goals but are
explicitly out of scope until CDDA-parity mechanics exist — see §4.

## 1. Current state (2026-07-14)

Phase 0 vertical slice is built and verified working end-to-end, and the I/O
layer has since been rebuilt to match real TPMOS: **there is no ncurses
anywhere in this project.** ncurses was tried first, called "a regressive
mistake" by direct instruction, and fully removed. The pipeline is now three
independent processes, no central manager binary:

- `system/keyboard_input` — owns the real terminal in raw termios mode
  directly (`tcgetattr`/`tcsetattr`, no curses), decodes `ESC [ A/B/C/D`
  itself into sentinel ints (`ARROW_LEFT=1000` etc.), and appends bare
  decimal keycodes to `pieces/apps/player_app/history.txt` — one int per
  line. Exits (restoring the terminal) on `q`, after writing it, and drops a
  byte in `pieces/system/quit_flag.txt` so the renderer knows to stop.
- `system/prisc+x pal/main_loop.pal` — reads `history.txt` via
  `read_history`, dispatches to `ops/move_player.+x` / `ops/end_turn.+x` /
  `ops/compose_frame.+x`, and halts itself (native `halt` opcode) when it
  sees the same `q` keycode — no external kill needed for the VM.
- `system/renderer` — a plain cooked-terminal process, no raw mode needed
  for output. Polls `pieces/display/frame_changed.txt`'s *size* (never
  mtime), and on growth, ANSI-clears the screen and prints
  `pieces/display/current_frame.txt`. Every frame it draws is also appended,
  timestamped, to `pieces/display/frame_history.txt` — a plain-text,
  append-only log of every frame the game has ever shown. **This is the big
  win of dropping ncurses**: rendering can be verified by reading a text
  file, no pty/pyte terminal emulation needed at all. Exits when
  `quit_flag.txt` becomes non-empty.

`button.sh run` is the orchestrator: it launches `renderer` and
`prisc+x`/pal in the background (tracked, killed on exit per
`cdda-tpm-std-fast.txt`'s "always track and be able to kill subprocess PIDs"
rule), then runs `keyboard_input` in the foreground since it's the one
process that needs the controlling tty.

Hero position, wall/door collision (via the terrain registry), and turn
counting all persist correctly across the process boundary through plain
state files, verified via a pty-driven test that sent `wasd` + arrow-key
input and then read `frame_history.txt`/state files directly (no terminal
emulator needed to check the result).

Mandatory source docs (read, do not duplicate their contents into new docs):

- `cdda-tpm-std-fast.txt` — no-shared-headers, one-process-manager +
  many-single-verb-ops doctrine, marker-file-size redraw triggering.
- `!.world_architecture+1=rusindol.txt` — world/map/piece nested directory
  structure and its renderer/engine resolution order.
- Real CDDA source, `catacylsm.DDA-0.F-dev🧟‍♀️️]ON]/0001/data/json/` — reference
  for what content categories and fields CDDA actually models. CC-BY-SA-3.0
  licensed; read only the specific category file you're porting, don't bulk
  read this tree.

## 2. The extension pattern (how every new feature gets added)

Everything new is either a **registry piece** or a **world/map instance
piece**. There is no third kind.

**Registry pieces** — global, non-located, static definitions. Live under
`pieces/registry/<category>/`, outside any `world_*/map_*/` container, per
the rusindol doc's rule for global pieces. Established example:
`pieces/registry/terrain/terrain_types.txt`. These are the mutaclsym
equivalent of a CDDA `data/json/<category>.json` file full of `"type"`
objects — same idea (a stable `id`, a set of fields, string
cross-references to other ids), different serialization (plain
pipe-delimited text, not JSON — see below for why).

**World/map instance pieces** — live, mutable state placed on a specific
map. Live under `pieces/world_<id>/map_<id>/<piece_id>/`, each with its own
`state.txt` (mutable fields) and `piece.pdl` (verb -> op-binary dispatch
table). Established example: `pieces/world_01/map_start/hero/`. Monsters,
NPCs, dropped items, vehicles, etc. all become sibling directories here (or
nested *inside* another piece's directory when they're logically contained
by it — e.g. an item inside a container inside the hero's inventory —
per the rusindol doc's "any piece can itself be a container" rule).

**Why plain text, not JSON:** every op is a small, self-contained C binary
with no shared headers and, by extension, no shared JSON parser dependency.
An op parses only the fields it needs off the pipe-delimited line, exactly
like `move_player.c`'s `terrain_walkable()` does today. Adding a JSON library
would mean either vendoring a parser into every single op or reintroducing a
shared header — both against the standing architecture decision. If a
specific piece of ported CDDA logic strongly wants JSON's nesting, convert it
to flat pipe-delimited fields at data-authoring time, the same way the
terrain registry already flattens `glyph|id|name|walkable`.

**Every new verb = one new op.** Harvesting, crafting, attacking, feeding,
wearing, examining — each becomes its own `ops/<verb>.c`, self-contained,
with the same ~10 lines of `resolve_root()` boilerplate every existing op
already has. It gets wired in two places: a `METHOD` line in the relevant
piece's `piece.pdl`, and/or a call from a `.pal` script (see
`pal/main_loop.pal` for the existing pattern of dispatching on read input).
Never factor the boilerplate into a shared `.h` — copy it, per doctrine.

## 3. Build order — CDDA mechanics first

Phases are meant to be done roughly in order, since each depends on state the
previous one introduces. Each phase should end the same way Phase 0 did:
flood-fill/consistency-check any new map or data before committing it, then
prove the feature with a pty capture, not just a claim.

**Phase 1 — World substance**
- Expand the terrain registry past wall/floor/door (grass, dirt, rubble,
  water, trees, etc.).
- Add a separate furniture registry (`pieces/registry/furniture/`), mirroring
  CDDA's terrain/furniture split (a tile has both a terrain *and* optionally
  a furniture layer — CDDA's `furniture_and_terrain`).
- More than one map piece, with a way to move the hero between
  `map_<id>` containers (CDDA's overmap, minimal version).

**Phase 2 — Items & inventory**
- Item registry under `pieces/registry/items/<category>/`, split the way CDDA
  splits `item_category` (generic, comestible, tool, gun, ammo, armor, book,
  container, ...).
- Item *instances* as pieces, nested under whatever currently holds them
  (ground tile, container, hero) — see the open decision in §5.
- Ops: pickup, drop, wear/wield, examine.

**Phase 3 — Player needs & stats**
- Extend `hero/state.txt` with hunger/thirst/fatigue/pain/stamina fields,
  ticked by `end_turn` (which today only increments the turn counter).
- Per-body-part HP (CDDA `body_parts.json`) instead of the current flat `hp`.

**Phase 4 — Monsters & NPCs**
- Monster registry (`pieces/registry/monsters/`), mirroring CDDA's
  `monsters.json` / `monster_special_attacks`.
- Monster instances as world/map pieces alongside the hero, each ticked once
  per turn from a pal script (own AI-tick op, same "one verb, one op"
  pattern).
- Melee combat op first; ranged later.

**Phase 5 — Crafting (CDDA parity first)**
- Recipe registry (`pieces/registry/recipes/`) with requirement/material
  references into the Phase 2 item registry.
- A `craft` op: checks inventory against a recipe's requirements, consumes
  inputs, produces the output item.

**Phase 6 — Professions/scenarios, starting presets, basic day/night and
weather, basic vehicles** — later CDDA-parity milestones, in roughly that
order. Not detailed further here; revisit this document when Phase 5 is
done.

## 4. Deferred — explicitly not now

Named here so scope doesn't creep into current phases, and so a future agent
knows these are planned, not forgotten:

- **Mods/ops packaging** — a way to load extra ops/registries as optional
  add-ons.
- **Multiplayer / online play.**
- **Save/load via named save-game file bundles** — distinct from the
  always-live `state.txt` files that exist now; a save is a snapshot you can
  name, store, and reload, not just "the current live directory tree."
- **"xelector"** — an entity-selection/targeting mechanism (name references
  the `xlector` piece pattern already seen in the TPMOS reference project
  `projects/fuzz-op/pieces/xlector/`), needed later for combat targeting,
  examine-at-a-distance, and pet commands.
- **Pet/companion management.**
- **Farming** — plant/harvest crops, raise and kill animals, recipes from
  farmed goods, selling goods to other players or NPC businesses.
- **Crafting broadened past CDDA parity.**
- **Civilization features** — founding settlements/countries, trade between
  players or factions. This is explicitly "way down the road" per direct
  instruction.

Do not start scaffolding any of these until the CDDA-parity phases in §3 are
substantially done.

## 5. Open decisions (resolve before/during Phase 1-2)

- **Registry granularity.** Terrain today is one flat pipe-delimited file
  (`terrain_types.txt`) holding every terrain type as a line. That's fine at
  small scale. Items may outgrow a single flat file fast (CDDA has hundreds
  of item defs per category) — may need one piece-directory-per-item instead
  of one-line-per-item in a shared file. Not decided; pick whichever keeps
  individual ops simple to parse, and revisit if a flat file gets unwieldy.
- **Item location model.** When an item sits on the ground, is it a piece
  nested under the map piece, nested under whatever contains it (pocket,
  hero, container-on-ground), or tracked in a flat registry with a
  `location` field pointing elsewhere? Recommendation: nest under whatever
  currently holds it, since arbitrary piece-in-piece nesting for
  "inventories/interiors" is already the sanctioned mechanism in the
  rusindol doc — don't invent a second location-tracking system alongside
  it.

## 6. Handoff notes

- Build/run: `./button.sh {compile|run|kill|check}` from the `mutaclsym/`
  root.
- **No ncurses anywhere, by design** — see §1. Do not reintroduce it (not
  even for a single op or a debug view); if a rendering need seems to
  require it, extend `compose_frame`/`renderer` instead.
- Test harness: no `pty`/`pyte` terminal emulation is needed to verify
  rendering anymore — `pieces/display/frame_history.txt` is a plain-text,
  timestamped, append-only log of every frame the renderer has drawn, and
  `pieces/display/current_frame.txt` is always the latest one. Read them
  directly. A pty is still needed only to *drive input*, since
  `keyboard_input` requires a real tty for raw termios mode (`tcgetattr`
  fails on a plain pipe) — write raw bytes to the pty's master fd, then
  verify by reading the frame/state files, not by parsing terminal escape
  codes.
- Quit path: `keyboard_input` and the pal script both independently
  recognize keycode `113` (`'q'`) as the stop signal — `keyboard_input`
  exits and writes `pieces/system/quit_flag.txt`; the pal script hits a
  native `halt` opcode. `renderer` polls `quit_flag.txt` and exits when it's
  non-empty. `button.sh run` resets `quit_flag.txt` to empty at the start of
  every session — a leftover non-empty flag from a prior run would make the
  renderer exit instantly.
- Arrow-key sentinel values (`ARROW_LEFT=1000`, `ARROW_RIGHT=1001`,
  `ARROW_UP=1002`, `ARROW_DOWN=1003`, matching real TPMOS's own convention)
  are defined independently in both `system/keyboard_input.c` and
  `ops/move_player.c` — no shared header, per doctrine, so if a value ever
  changes it must be changed in both places by hand.
