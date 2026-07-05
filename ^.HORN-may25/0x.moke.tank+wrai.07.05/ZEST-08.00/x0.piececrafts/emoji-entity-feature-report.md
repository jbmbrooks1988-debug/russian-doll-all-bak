# Emoji-Entity Feature: Investigation Report & Implementation Plan

Date: 2026-07-03
Status: **Investigation complete, nothing implemented yet.** Written as a
session hand-off — this doc should be everything a future session (or a
fresh Claude instance) needs to start implementing without re-deriving any
of this.

## What was asked

Owner wants emojis usable as **entities that render both 2D and 3D**, the
3D form being a voxel **extrusion generated from an atlas**, matching what
`#.plugy3d-engine-2026_v19` ("Fresh Engine 2026", the reference target for
piececraft-wraith's whole 3D rebuild this session — see `py3d-inspo.md`)
does with its entity system, using
`x0.parent-level-dev-env-02.01/#.emoji-studio-501.02.05t` as the tangential
POC for *how the atlas→voxel extraction itself* can work.

**Headline finding: this is not a new feature to invent — it's the joining
of three things that already exist in this codebase, at three different
levels of completeness.** Nothing here requires starting from a blank
page. The rest of this doc is: what those three things are, exactly how
finished each one is, and the concrete plan to join them.

---

## Part 1: The three existing pieces

### 1A. The POC — a complete, working, standalone reference

`x0.parent-level-dev-env-02.01/#.emoji-studio-501.02.05t/&.emoji-studio-solo.02.01/`

A self-contained C/OpenGL/GLUT/FreeType desktop app. Three binaries, all
already read in full this session:

- **`emoji-gen-atlas.c`** (113 lines) — takes one emoji symbol (a UTF-8
  string) + an output path. Loads `/usr/share/fonts/truetype/noto/
  NotoColorEmoji.ttf` via FreeType (`FT_LOAD_COLOR`, reads the color
  bitmap glyph directly — Noto Color Emoji ships pre-rendered color
  glyphs, not vector outlines), decodes the UTF-8 codepoint by hand
  (own `decode_utf8()`, no library), centers/scales it into a 64x64 RGBA
  buffer, writes it as a PNG via `stb_image_write.h`. This is "atlas
  generation for one glyph" — really a single-emoji PNG, not a real
  multi-emoji atlas strip (see 1A-manager below for the actual
  strip-atlas variant).
  - **Confirmed dependency present on this machine:**
    `/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf` exists
    (11MB, verified via `fc-list`). This pipeline is reproducible here,
    not just theoretical.
- **`emoji-xtract.c`** (123 lines) — takes `<atlas_path> <emoji_index>
  <resolution> <output_csv>`. Loads the PNG via `stb_image.h`, slices out
  a `64x64` region at `x_offset = emoji_index * 64` (i.e. assumes a
  horizontal 1-row strip atlas, one 64x64 emoji per slot), **box-samples
  downscales** that region to `resolution x resolution` (averaging
  R/G/B/A over each source block — this is what makes low resolutions
  still look reasonable instead of just nearest-neighbor), then writes a
  CSV:
  ```
  # resolution=8
  # scale=1.0
  # transform=0,0,0
  r,g,b,a
  <r>,<g>,<b>,<a>       <- one row per pixel, row-major (y*N+x), N*N rows total
  ...
  ```
  This exact CSV shape (3 `#`-comment header lines + `r,g,b,a` column
  header + flat pixel rows) is **already the established convention**
  used by every TPMOS emoji-related asset found this session (see 1B/1C).
- **`emoji-studio_host.c`** (389 lines) — the GLUT window. Left 320px
  strip = a scrollable 2D picker grid: for each visible emoji, renders
  the *actual* FreeType color glyph as a cached GL texture (`PICKER_COLS
  * PICKER_ROWS` texture-slot cache, re-bound on scroll) — this is the
  **2D rendering path**, and it does not touch the voxel CSV at all, it's
  the real font glyph. Right 480px = 3D viewport: on emoji select, forks
  `emoji-gen-atlas` (glyph → 64x64 PNG in `pieces/<name>/mini_atlas.png`)
  then `emoji-xtract` (PNG → `pieces/<name>/voxels_<res>.csv`) if the CSV
  doesn't already exist, loads the CSV, and for every pixel with
  `alpha > 10` draws **one uniform-height cube** (`draw_cube()`, real
  6-face immediate-mode GL_QUADS box) positioned at that pixel's XZ grid
  cell, using the pixel's own RGB as the cube's flat color. All columns
  are the **same height** (`h = total_size`, a fixed 2.0 GL units) —
  **this is a "bar-chart" extrusion, not sculpted depth.** A pixel is
  either a full-height colored column or nothing; there is no
  per-pixel depth/height variation, no relief. Camera is `WASD` zoom +
  `QE` pitch (mouse is not used in the POC despite `atlas-cash-
  blueprint.md`'s framing — the actual `keyboard()` handler is the only
  camera input registered).
- **`manager/emoji-studio_manager.c`** (140 lines) — a *different*
  orchestration variant: assumes a **pre-built master atlas**
  (`../#.emoji.xtract.stb]c4/emoji_atlas.png`, a real horizontal strip of
  many 64x64 emoji, referenced but not present in this POC directory —
  it lived in a sibling dir from an earlier project phase) and just calls
  `emoji-xtract` directly with `emoji_list[idx].index` as the slice
  index — no per-click FreeType generation. This is the "bulk pre-bake"
  path vs. the host's "generate on first click" path. Both converge on
  the same `emoji-xtract` op and the same CSV format.
- `parsed_emojis.txt` — 3624 fully-qualified emoji, `codepoint_hex|
  symbol|name` per line. The registry both host and manager load at
  startup to map a click/index to a symbol/name.
- `atlas-cash-blueprint.md` — a well-written architecture write-up of
  this exact pipeline, confirms everything above (read in full).

**Not adopted, and shouldn't be:** GLUT, immediate-mode GL_QUADS,
FreeType-in-the-render-loop. Per the locked `ARCHITECTURE-RGB-
RENDERING.md` decision, none of that crosses into TPMOS/Wraith. What's
reusable is the **data pipeline** (atlas → box-downsample → CSV) and the
**extrusion concept** (alpha-gated column per pixel), not the renderer.

### 1B. `projects/emoji-studio/` — an abandoned/incomplete port, NOT TPMOS-wired

Top-level (non-Wraith) project. Verified this session:
- Contains near-identical copies of `emoji-studio_host.c`,
  `manager/emoji-studio_manager.c`, `ops/emoji-xtract.c` (all three
  already compiled to `+x` binaries — `ops/+x/emoji-xtract.+x`,
  `manager/+x/emoji-studio_manager.+x`, `emoji-studio_host`). Diffed
  `emoji-studio_host.c` against the POC's version: cosmetic path/include
  tweaks only (`../../libraries/stb_image.h` instead of local `lib/`,
  `emoji_list[100]` instead of `[4000]`) — functionally the same GLUT app.
- **Has `assets/`, `pieces/`, `maps/`, `layouts/` directories, all
  empty.** No `project.pdl` at all.
- `session/test_emoji.csv` exists — an 8x8 CSV in the right format, but
  **every row is `0,0,0,0`** (fully transparent/blank placeholder, not
  real emoji data).
- **Conclusion: this is a reference copy someone dropped in mid-port,
  never actually wired into TPMOS conventions (no manager= field, no
  layout, no launcher entry). Treat as read-only reference material for
  "how the original C files looked," not as a dependency to run or fix.**

### 1C. `emoji-studio-wraith` — the real, deliberate, half-built Wraith attempt

`projects/wraith-alpha/wraith-projects/emoji-studio-wraith/` — this is
where the actual design intent lives, and it's much further along than
1B. Already discovered/re-confirmed this session:

- **`ASSUMPTIONS.md`** (58 lines, read in full) is a genuinely good spec,
  written by an earlier session, that already answers most of "how should
  this work":
  - States plainly: *"Internal Wraith validation project for 2D
    RGBA-to-3D extrusion inside `${game_map}`."*
  - Declares the semantic object contract:
    ```
    IMAGE_EXTRUSION | id=emoji_sample | role=rgba_extrusion |
      source=pieces/sample_emoji/voxels_8.csv | resolution=8 | surface=game_map
    CAMERA | mode=4 | x=0.00 | y=0.00 | z=0.00 | pitch=15.00 | yaw=0.00 | roll=0.00
    ```
  - Explicitly lists what the RGB daemon *should eventually* do: "parse
    the CSV, create one extruded column/cell for each alpha-visible
    pixel, use RGB as material, use alpha as occupancy/height policy,
    project through the Piececraft camera contract, rasterize into
    `current_frame.rgba32`, write receipt data..." — **this is
    word-for-word the plan this report would otherwise have to write from
    scratch.** It even correctly anticipates needing "the Piececraft
    camera contract" (i.e. `project_world_point`/`draw_box`), which
    didn't fully exist yet when this was written but now does (built this
    session).
  - Explicitly disambiguates from the other two Wraith 3D conventions so
    nobody conflates them:
    - `wraith-3d-cube`: `pieces/<id>/artifact.txt` z-slice bitmask (and
      per `wraith-3d-cube/IMPLEMENTATION.md`'s 2026-07-02 correction —
      that project is *actually* supposed to migrate to the tile-map
      standard below, its old z-slice doc was wrong)
    - `piececraft-wraith`: `maps/map_01_z*.txt` + tile registry/extrude
      standard (this is the one we spent this whole session maturing)
    - `emoji-studio-wraith`: RGBA CSV extrusion standard (this feature)
- **`README.md`** confirms the CSV convention (`pieces/<name>/
  voxels_8/16/32/64.csv`, alpha=occupancy, RGB=material) and explicitly
  cites the POC dir as its reference source.
- **`layouts/emoji-studio-wraith.chtpm`** — has a picker/resolution UI
  (`Interact Emoji Surface`, `Resolution 8/16/32` buttons via
  `onClick="EMOJI_RES:N"`) but **these buttons don't do anything yet** —
  no op consumes the `EMOJI_RES:` verb.
- **`session/scene.objects.pdl`** — declares one real, static object:
  ```
  OBJECT tag=model id=emoji_sample role=rgba_extrusion x=35 y=6 w=32 h=14 z=23
    source=.../emoji-studio-wraith/pieces/sample_emoji/voxels_8.csv
    label=IMAGE_EXTRUSION:source=pieces/sample_emoji/voxels_8.csv;resolution=8;
          alpha=occupancy;rgb=material;reference=emoji-studio/session/test_emoji.csv
  ```
  This is hand-authored/static (no manager writes it — there is no
  manager for this project) but it's a real, valid `rgba_extrusion`
  object the RGB daemon already knows how to find.
- **`pieces/sample_emoji/voxels_8.csv`** — a real 8x8 sample emoji (64
  data rows, actual RGBA values, not the blank placeholder from 1B).
  Genuinely renderable today.
- **What's missing**, confirmed by direct inspection (not assumption):
  - **No manager at all.** Per the standing rule enforced across the
    other 9 Wraith projects this session (`wra-mana-checklist.txt`),
    this needs the identical init-only manager pattern
    (`manager/+x/emoji-studio-wraith_manager.+x`, `<module>` tag in the
    layout, `manager=` field in `project.pdl`). `project.pdl` currently
    has no `manager=` line either.
  - **No `ops/wraith_project_input.c`.** So nothing is interactive:
    `INTERACT`, `EMOJI_RES:8/16/32` all currently no-op. This is the
    single biggest gap — everything else (CSV data, semantic object
    contract, even a first-pass renderer) already exists in some form,
    but there's no hot-path input handler to change resolution, swap
    which emoji is shown, or drive a camera.
  - **`session/history.txt` and `session/.manager.lock`** exist (someone
    opened this window in Wraith at some point) but there's no
    `session/state.txt` — confirms it's never actually been driven by a
    manager/op, only ever displayed statically.

### The RGB daemon already has a partial renderer for this — a stub, by its own admission

`projects/wraith-alpha/plugins/wraith_rgb_daemon.c` already contains (read
in full this session, lines ~1106–1416):

- **`draw_rgba_extrusion_preview()`** — parses `# resolution=N` +
  `r,g,b,a` rows (the exact format above), lays out an `N x N` grid
  inside the object's screen rect, and for each `alpha > 0` pixel draws a
  small rect **offset diagonally by a height-derived pixel amount**
  (`px+height,py-height` → `px+cell+height,py+cell-height`, where
  `height = 3 + alpha/64`). This is a cheap "drop-shadow" fake-3D trick —
  a flat top-down grid with a diagonal offset standing in for depth. It
  does **not** use `project_world_point()`/`draw_box()`/camera state at
  all.
- **`audit_rgba_extrusion()`** — writes a receipt line per object with
  fields that **self-document the stub status**:
  `projection_mode=flat_extrusion_preview`, `final_projection=0`,
  `camera_consumed=0`. Whoever wrote this deliberately left a marker
  saying "this isn't the real thing yet" — which matches `ASSUMPTIONS.md`
  saying the same in prose ("RGB presenter draws a flat extruded preview,
  not depth-sorted 3D columns through camera state").

**This is good news for scope**: the CSV-parsing half of the renderer
already exists and works; only the *projection* half needs replacing —
with machinery (`project_world_point`, `draw_box` w/ its new
multi-face + wireframe-floor-datum logic, the `camera=` field parsing)
that **didn't exist when this stub was written but does now**, having
been built and hardened this session for piececraft-wraith.

---

## Part 2: How this maps onto piececraft-wraith's now-mature 3D pipeline

`py3d-inspo.md`'s remedy plan (written earlier this session) has an item
that is, in hindsight, **this exact feature**:

> **3. Give tiles real shape data ("better receipts") — not done yet.**
> Add an optional per-tile voxel/shape file (reuse the existing
> `voxels_8.csv` / `#resolution=8` convention already used by
> emoji-studio and moke-pet, not a new format)... so a tree can actually
> look like a trunk + canopy instead of a uniform brown-then-green
> column.

So there are two converging use cases for the same underlying mechanism,
not two different features:

1. **Emoji Studio's own use case**: a standalone-ish browsing/preview
   surface (2D picker + 3D voxel view) — closest to what the POC and
   `emoji-studio-wraith` already sketch out.
2. **Piececraft-wraith's use case**: an emoji-derived voxel CSV as the
   *shape* for a tile or entity standing in the tile-map world (a tree
   that's actually shaped like 🌲, a pet that's actually shaped like 🐶),
   rendered through the exact camera/box/wireframe-floor pipeline already
   built for the map.

**Recommendation: build the renderer once, generically, and let both
consume it.** Concretely: a function like
`draw_voxel_grid_extrusion(buffer, csv_path, world_x, world_z, cam_x,
cam_y, cam_z, pitch, focal, screen_cx, screen_cy, scale)` that, for each
alpha-visible CSV pixel, calls the *existing* `draw_box()` (which already
does top+near+one-side-face multi-face rendering, already respects the
shared `wy=0` ground datum, already respects yaw/pitch camera orbit)
instead of a bespoke 2D-offset trick. Then:
- `role=rgba_extrusion` objects (emoji-studio-wraith's own surface) call
  it directly, replacing `draw_rgba_extrusion_preview()`'s stub body —
  same object contract, same receipt fields, just `final_projection=1`
  and `camera_consumed=1` instead of `0`.
- Piececraft-wraith tiles/entities call it too, once a tile/entity
  optionally declares a `voxel_source=pieces/<name>/voxels_8.csv` field
  (additive — falls back to today's flat colored box when absent, per
  `py3d-inspo.md`'s own non-regression note).

This also directly answers the "2D and 3D" half of the ask: **piececraft-
wraith already has this exact duality solved** for its own map (fixed
this session) — `display_mode` (`2d_topdown` / `3d_voxel`), ASCII always
renders the 2D top-down text grid regardless of GL mode, GL renders
either the flat 2D preview or the real 3D scene depending on
`display_mode`. An emoji entity should **reuse that same switch**, not
invent its own: 2D mode = draw the flat top-down pixel grid (a genuine
sprite view of the CSV, not the current diagonal-offset fake-3D hack);
3D mode = the real per-pixel `draw_box()` extrusion described above.

---

## Part 3: Open questions for the owner (don't guess these — ask next session)

1. **Live FreeType generation vs. pre-baked assets.** The POC's
   `emoji-gen-atlas` op requires linking FreeType (`-lfreetype`) and
   reading a system font path — this would be the **first external
   library dependency** in any Wraith/piececraft C code (everything so
   far is libc + vendored `stb_image.h`, zero link-time deps beyond
   `-lm`). Two paths:
   - **(a) Live-generate on demand**, matching the POC exactly: user
     picks any of the 3624 emoji, an op forks `emoji-gen-atlas` +
     `emoji-xtract` the first time, caches the CSV under `pieces/<name>/`
     forever after (this is literally what the POC host already does —
     `generate_voxel_csv()`'s "already exists? skip" check). Needs
     FreeType linked into one small standalone op binary (not the RGB
     daemon itself — keep the daemon dependency-free).
   - **(b) Pre-bake a fixed small set** (e.g. the emoji piececraft-wraith
     actually wants to use — 🌲 for tree, 🪨 for stone, 🐶 for a pet) as
     static `.tile.txt`-sibling assets shipped in the repo, same as
     `assets/tiles/*.tile.txt` today. Zero new runtime dependencies, zero
     new op, but not "pick any emoji" — a fixed art set.
   - Given TPMOS's existing convention (tiles are static data files
     checked into the project), **(b) leans more consistent**, but (a) is
     what "emoji studio" as a picker/browser tool actually needs to be
     useful as its own feature. These likely aren't mutually exclusive —
     (a) for the interactive Emoji Studio browsing surface, (b) for
     piececraft-wraith tiles/entities that just want a fixed emoji shape
     baked in at map-author time. **Needs an explicit decision before
     writing the generation op.**
2. **Uniform-height bar-chart extrusion vs. real depth.** The POC (and
   the RGB daemon's stub) both treat "alpha > threshold" as a binary
   occupied/empty per pixel, with every occupied pixel getting the same
   column height. That's cheap and already proven, and matches the ask
   ("extrusion generated from an atlas") literally. A fancier version
   could map alpha or luminance to variable height for a relief effect —
   not asked for, not recommended as a first pass, but worth naming so
   nobody assumes it's already relief-mapped.
3. **Does emoji-studio-wraith stay a standalone validation surface, or
   does it become piececraft-wraith's asset browser?** i.e. is the
   long-term shape "one Wraith project (`emoji-studio-wraith`) where you
   browse/pick emoji and it writes voxel CSVs that other projects then
   reference," or does each project (piececraft-wraith included) just
   grow its own tiny picker? Given the "entities... within piececraft"
   framing of the ask, probably the former (emoji-studio-wraith becomes
   the generator/library, piececraft-wraith's tiles/entities just point
   `voxel_source=` at whatever CSV it produced) — but this is a real
   architectural fork worth confirming, not assuming.

---

## Part 4: Recommended concrete implementation order (next session)

Mirrors the same discipline used all session (standalone C test harness
verification before touching the daemon; manager stays init-only; atomic
writes for any new state file):

1. **Decide Part 3's open questions first** — they change what op(s) get
   written.
2. **Give `emoji-studio-wraith` its manager** — identical mechanical
   pattern as the other 9 (`wra-mana-checklist.txt`'s template). This was
   already flagged as pending, lower-priority work in that checklist;
   doing it now unblocks everything else since nothing in this project
   currently runs without one.
3. **Write `ops/wraith_project_input.c`** for emoji-studio-wraith — at
   minimum: handle `EMOJI_RES:N` (switch which `voxels_N.csv` is
   referenced), `INTERACT` (map-control gate, same pattern as
   piececraft-wraith), and re-emit `scene.objects.pdl`'s `rgba_extrusion`
   object with the new `resolution=` value. If Part 3.1 picks live
   generation, this is also where the fork/exec of the generation op(s)
   belongs (mirroring `render_body()`'s existing fork/exec pattern in
   piececraft-wraith's op).
4. **Extract a shared `draw_voxel_grid_extrusion()` helper** in
   `wraith_rgb_daemon.c` built on the existing `draw_box()` /
   `project_world_point()` / camera-field parsing already proven for
   piececraft-wraith — verify it standalone (test harness with a
   synthetic small CSV + fixed camera, dump projected coordinates,
   compare to hand-computed expectations) **before** wiring it into
   `draw_rgba_extrusion_preview()`'s call site, same discipline used for
   every perspective-math change this session.
5. **Swap `draw_rgba_extrusion_preview()`'s body** to call the new
   helper; update `audit_rgba_extrusion()`'s receipt fields to
   `projection_mode=camera_projected`, `final_projection=1`,
   `camera_consumed=1` once it's real.
6. **Only then**, if Part 3.3 confirms the "entities within
   piececraft-wraith" direction: add an optional `voxel_source=` field to
   `assets/tiles/<id>.tile.txt` (additive, falls back to today's flat box
   when absent — per `py3d-inspo.md`'s non-regression note), and have
   `draw_tile_zmap_preview_3d()`'s per-tile branch call the same shared
   helper instead of `draw_box()` when a tile declares one.
7. Live-test each step in actual Wraith before moving to the next, same
   as every other change this session — do not chain multiple untested
   steps.

---

## Cross-references

- `py3d-inspo.md` — item 3 of its remedy plan is this feature; item 2
  (multi-face `draw_box()`) is the rendering primitive this feature reuses.
- `ARCHITECTURE-RGB-RENDERING.md` — the no-OpenGL-ever / RGB-is-the-
  signal decision this feature must keep honoring (the POC's GL host is
  reference-only, never a dependency).
- `wra-mana-checklist.txt` — the manager template emoji-studio-wraith
  still needs (step 2 above).
- `projects/wraith-alpha/wraith-projects/emoji-studio-wraith/
  ASSUMPTIONS.md` and `README.md` — read these first in the next
  session; they already contain most of the "why," this report mainly
  adds "confirmed current state" + "how it now plugs into piececraft-
  wraith's finished camera pipeline."
