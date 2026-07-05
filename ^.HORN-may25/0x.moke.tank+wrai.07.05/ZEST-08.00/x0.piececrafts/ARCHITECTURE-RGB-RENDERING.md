# RGB Rendering Architecture — Locked Decision
**Date:** July 2, 2026 | **Status:** LOCKED — governs all future 3D rendering work in this codebase

---

## The Principle (verbatim decision)

**No OpenGL. Ever.** Not because it's inferior — because it isn't in the target
stack at all. The long-term goal is sending raw RGB signal directly to
screens (framebuffer/DRM/custom display driver level), with no windowing
system or graphics API dependency in between. "Worse than OpenGL" is not a
valid objection to any approach here, because OpenGL was never going to be
part of the destination architecture.

**OpenCL is fine, if/when needed.** It's a compute API, not a display API —
it doesn't want a GL context, a swapchain, or a windowing system. It can be
used purely to parallelize math (vertex transforms, rasterization) on GPU
hardware, with results read back into the same plain RGB buffer the rest of
the pipeline already uses. It never touches how pixels reach the screen.

## The Signal Chain

The composed pixel buffer (currently `projects/wraith-alpha/session/rgb/current_frame.rgba32`
and equivalents) **is** the pure signal. Every renderer's only job is to
write correct RGB pixel values into a `unsigned char *buffer`. Today that
buffer gets blitted to an X11 window ("Wraith Alpha RGB Mirror"); tomorrow
it could be written straight to a framebuffer/DRM device or dedicated
display hardware. Renderer code must never know or care which — that
separation already exists in `wraith_rgb_daemon.c` (it fills `buffer` and
never touches windowing) and must be preserved.

## The `.rgba32` File Format (why it's binary, not text)

`current_frame.rgba32` is a raw, headerless pixel dump — no magic bytes, no
length prefix, nothing but `WIDTH * HEIGHT * 4` bytes back to back, one byte
each for R, G, B, A per pixel, row-major. `WIDTH`/`HEIGHT` come from
`COLS(128) * GLYPH_W(8)` and `ROWS(40) * GLYPH_H(16)` in
`wraith_rgb_daemon.c` — `1024×640`, so **2.56 MB per frame**. It's written
with a single `fwrite(buffer, 1, WIDTH*HEIGHT*4, f)` (`wraith_rgb_daemon.c`)
on a `usleep(16667)` loop — i.e. up to **60 times a second** — and read back
with one `fread()` straight into the buffer handed to
`glTexImage2D(..., GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
frame_buffer)` (`wraith_gl.c`). The bytes on disk are byte-for-byte the same
bytes the display path consumes — there is no encode/decode step at all.

This is deliberate, not an oversight, for the same reason nothing else in
this signal chain reaches for a heavier format:

- **Size.** Hex/text encoding a byte is a minimum 2x blowup (2 ASCII chars
  per byte), realistically 3-4x once separators are added. 2.56 MB/frame
  becomes 10+ MB/frame, and at 60 fps that's several hundred MB/sec of disk
  I/O instead of ~154 MB/sec.
- **Parse cost inside a 16ms frame budget.** Raw is a single memcpy-speed
  `fread()` directly into the GPU upload call. A text format means parsing
  ~2.6 million hex pairs back into bytes every frame — real CPU work that
  doesn't exist today, competing with the same 16ms deadline the daemon is
  already built around.
- **It was never meant to be read by a person.** The project's
  human-readable convention (plain-text `state.txt`/config/history files)
  exists for things someone might actually open and debug by eye. A
  60fps-refreshed GPU frame buffer isn't that kind of file — it's the same
  category as a PNG or any other framebuffer, binary by nature everywhere
  this pattern exists. The human-inspectable need is already served
  separately by `session/current_frame.png` (a preview snapshot, not the
  hot path) — that split is intentional, not a gap.

## Why GL-OS (the "ideal" reference) doesn't change this

GL-OS's actual 3D engine uses real OpenGL/GLUT (confirmed: pitfalls doc
entries #37-40 describe GLUT-specific input handling; window title in
screenshots is literally "GL-OS Desktop", a separate subsystem from
Wraith's RGB mirror). It also uses a different data contract entirely —
per-piece directories (`pieces/<id>/state.txt`), not Wraith's flat
`scene.objects.pdl`. GL-OS was useful tonight only as a **visual quality
bar** (what the perspective grid should look like), not as a dependency or
integration target. We are not routing through it.

## Build Plan (staged, CPU-first)

1. **CPU-only correctness pass.** Extend the rotation + perspective-divide
   math already proven in `draw_zslice_piece_preview()` (`wraith_rgb_daemon.c`,
   used by wraith-3d-cube: rotate by pitch/yaw/roll, then
   `focal / (focal + z + cam_z + offset)` perspective divide) from "8
   vertices of one cube" to "4 corners of every tile in a grid." Pure
   portable C, zero new dependencies.
2. **Polygon fill.** The renderer only had `fill_rect_px` (axis-aligned)
   before this work. Projected/rotated tile corners are not axis-aligned,
   so a scanline (or triangle) fill is required. This was the one
   genuinely missing primitive.
3. **Depth handling.** POC ships with far-to-near painting order (rows
   drawn back to front) as a first approximation. A real per-pixel depth
   buffer is deferred until camera angles that actually cause overlap
   are needed — flagged as a known simplification, not forgotten.
4. **Benchmark before reaching for OpenCL.** Grids in play are small
   (20×10 tiles = a few hundred quads). Plain C on CPU should hit
   interactive framerates without help. OpenCL is a later optimization
   pass, applied only once we know the actual bottleneck — likely the
   existing full-recompose-per-keypress pattern, not raw fill-rate.

## Status

POC (steps 1+2+3) implemented and compiled clean. Code:
`project_world_point()`, `fill_quad_px()`, `draw_tile_zmap_preview_3d()` in
`projects/wraith-alpha/plugins/wraith_rgb_daemon.c`.

**Important correction made during implementation:** the first draft reused
`draw_zslice_piece_preview()`'s exact transform (rotate-in-place, camera as
a screen-space nudge), which works for a single small object near the
origin but produces a near-clip singularity and inverted near/far ordering
for a full tile grid. Replaced with a proper camera-relative transform
(translate world point into camera space, rotate by pitch, perspective
divide by depth) — verified standalone against the real tile_zmap object's
canvas bounds (see test iterations in session history) before integrating:
confirmed monotonic width convergence (near tiles wide, far tiles narrow)
and correct vertical ordering (near tiles at the bottom of the object,
far tiles near the top/horizon) before touching the daemon source.

Painting order: far-to-near rows (`row_count-1` down to `0`), so nearer
tiles correctly draw over farther ones without a depth buffer — this only
works because rows never overlap in an unexpected order for this camera
angle; a real depth buffer is still deferred per the staged plan (needed
if/when camera angles allow more complex overlap).

Camera constants (`pitch=22°, cam_y=7, cam_z=-16, focal=1.0, scale=210`,
horizon at 8% down from the object's top edge) are tuned against the
current 20×10 map and the tile_zmap object's actual pixel bounds
(x:280–664, y:96–320 in the 1024×640 canvas). If map dimensions or the
object's on-screen size change later, these will need retuning — they are
not derived from map/object dimensions automatically yet.

**Live-tested, works.** POC confirmed rendering a real perspective grid
with correct near/far convergence in Wraith. Screenshot reviewed
2026-07-02 evening.

---

## Voxel Cube Convention (locked, added after live test)

**Default: every tile is a true cube, not a flat card.** If a tile's
footprint is `N×N` in the 2D map, its 3D extrusion is `N×N×N` by default —
width, depth, and height all equal. This was the one correction needed
after the first live test (POC rendered flat-topped quads only).

**Ground-plane reference and sign convention:** `wy=0` is the walkable
top surface — the plane the xelector's feet touch. Two things extrude
from that shared reference in opposite directions:
- **Tiles extrude negatively** — a tile's solid body occupies
  `wy ∈ [-height, 0]`, i.e. downward, into "the earth" below the walkable
  surface.
- **The xelector (and other above-ground objects/pieces) extrude
  positively** — occupying `wy ∈ [0, +height]`, standing on top of the
  shared `wy=0` surface, one layer above the tile it's on.

This resolves cleanly why "xelector sits one layer above the tile": it's
not a rendering hack, it's the natural consequence of ground and
above-ground objects sharing one reference plane and extruding away from
it in opposite directions.

**Future per-voxel fields (not yet implemented):** height need not always
equal width/depth — e.g. a shallow puddle tile might be `10×10×0.5`. When
this is needed, add explicit `opacity`, `scale`, and `offset` fields per
tile/voxel (opacity for partial transparency, scale for non-cubic
height, offset for shifting the extrusion reference off `wy=0`). Default
values (`opacity=1.0, scale=1.0, offset=0.0`) reproduce the current
uniform-cube behavior exactly — this is a strict extension, not a
breaking change, when it lands.

**Reference search (done, documented so it isn't repeated):** searched
for an existing real GL-OS/piececraft-3D voxel-cube renderer to copy
from. Found:
- `pieces/apps/gl_os/plugins/gl_os.c` — desktop/window manager only
  (virtual terminal windows), not a map/voxel renderer at all.
- `pieces/apps/gl_os/plugins/gl_os_renderer.c` — confirmed flat 2D only
  (`glOrtho`, `glVertex2f` throughout, no Z-extrusion anywhere in the file).
- `projects/fuzz-op-gl/assets/voxels/*.voxel` — a genuine "NxNxN Voxel
  DNA" bitmask data format (e.g. `xelector.voxel` is 8×8×8, one bit per
  unit voxel row per layer). This is real prior art for the *data model*
  we want (cubic by default, generalizable from 1-bit fill to float
  opacity later) — but no renderer source consuming this format was found
  in this codebase. It's a promising future data format, not a working
  reference implementation.
- **Conclusion:** no working "map tiles as extruded cubes" renderer
  exists anywhere in this codebase yet to port from. The reference
  screenshot the user showed is either a design target or runs through
  code not present in this checkout. Tile-cube extrusion in
  `wraith_rgb_daemon.c` is original engineering from here, informed by
  the `.voxel` DNA format's conceptual model but not a port of working
  code.

**Implementation (POC v2):** each tile now renders as a 2-face cube (top
face + camera-facing front face, using the far-to-near painting order
already in place) instead of a flat quad. Left/right side faces and full
6-face rendering are deferred — not needed for a `yaw=0` camera where
those faces are rarely visible, revisit if camera yaw/rotation controls
are added later.

---

## Correction (same evening, after live review): height is data-driven, not hardcoded

First POC v2 draft hardcoded per-glyph height in C (`switch` on `'#'`/`'T'`/`'R'`
with fixed extrude constants). **Wrong** — corrected direction: **every
tile defaults to a full cube (width = depth = height) unless a metadata
file says otherwise.** No per-glyph logic belongs in the renderer at all.

**The data already existed, unused, from earlier project scaffolding:**
`assets/tiles/registry.txt` (glyph → tile_id) plus one `assets/tiles/<tile_id>.tile.txt`
per tile type with `rgb_top`, `rgb_side`, `extrude` fields — e.g.
`grass.tile.txt` has `extrude=1.0` (full cube), `wall.tile.txt` has
`extrude=2.0`, `tree.tile.txt` has `extrude=3.0`. This mirrors the
`# resolution=8 / # scale=1.0 / # transform=0,0,0` header convention
already used by emoji-studio's voxel CSV assets (`pieces/test_voxels.csv`),
applied to tiles instead of emoji voxel models — same idea, different
asset type. **Not invented for this fix — found already in place and
wired up.**

`draw_tile_zmap_preview_3d()` now calls `load_tile_meta()` per glyph
(cached per-frame, since a 20×10 grid only has a handful of unique
glyphs) instead of hardcoding colors/heights. A glyph with no registry
entry, or a tile file missing an `extrude=` line, defaults to `1.0` (full
cube) and a neutral gray — "everything is a cube unless specified"
applies even to the fallback path, not just the happy path.

**Real bug caught during standalone verification (not live-debugged
blind):** `'#'` is both the wall glyph AND the registry file's
comment-marker convention (`# Piececraft Wraith Tile Registry` as a
header comment). The naive comment-skip check (`line[0]=='#' → skip`)
was silently treating the actual `#=wall` data line as a comment,
dropping wall's real color/height and falling back to defaults. Caught
by writing a tiny standalone test harness against the real registry file
before touching the daemon again — fixed by only skipping `#`-prefixed
lines when the second character isn't `=` (i.e. `# text` is a comment,
`#=wall` is data).

### Entities (xelector, pet) — separate from tile extrusion entirely

Confirmed this is genuinely a different concern from tile height, not
solved by the same per-glyph metadata: xelector and pet are drawn as
their own small inset boxes (smaller footprint than a full tile,
extruding *positively* from `wy=0` per the ground-plane convention
above), computed independently of whatever tile they're standing on.
Pet position (`pet_x`, `pet_y`) is new state — currently a fixed test
value (14, 6), no AI/movement yet, added purely to prove the
entity-rendering pipeline handles more than one positioned object
correctly before pets get real behavior.

**Not yet data-driven:** entity dimensions (xelector/pet box size,
height) are still hardcoded constants in `wraith_rgb_daemon.c`, not read
from a `piece.pdl`-style file the way tile dimensions now are. Natural
next step if/when entities need per-type customization, following the
exact same pattern just proven for tiles — not urgent since there are
only two entity types right now.

---

## Correction 2: extrusion direction is data, not inferred from `walkable`

Live-tested after the height fix and walls/trees/rocks still looked
flat — because "extrude down from `wy=0`" (correct for floor) was being
applied to *every* tile including obstacles. A wall's top staying pinned
at the shared `wy=0` ground level means it reads as exactly as tall as
the floor, no matter how big `extrude` is, since only the invisible
underground portion grows.

First instinct was to infer direction from the existing `walkable` field
(`walkable=1` → down, `walkable=0` → up). **Rejected on user direction:**
that couples two independent concerns (can-you-stand-on-it vs.
which-way-does-it-extrude) — the engine should stay a "dumb" consumer of
explicit data, not encode assumptions about what one field implies about
another. Added a dedicated `extrude_dir=up|down` field to each
`.tile.txt` instead, decoupled from `walkable` entirely. Default (field
absent) is `down`, matching the base ground-plane convention — so
`grass.tile.txt` needed no changes at all, only `wall.tile.txt`/
`tree.tile.txt`/`stone.tile.txt` got the new line.

**Verified standalone against the real files before recompiling the
daemon** (same discipline as the earlier `'#'`-is-also-a-comment-marker
bug): confirmed `extrude_up=0` for grass, `extrude_up=1` for wall/tree/
stone, matching the new field exactly, before touching
`wraith_rgb_daemon.c` again.

## Camera controls: WASD pan + 1/2/3 POV presets

Checked piececraft-3d's actual manager before implementing (not
guessing): WASD/Z/X there are claimed by entity movement and Z-level,
same as piececraft-wraith already has — camera adjustment is a *separate*
button-driven mechanism (`CAMERA_MODE:`, `CAMERA_MOVE:`, `CAMERA_SET:`
routed actions). User's actual intent (clarified in conversation): WASD
should be dedicated to camera *only* in piececraft-wraith, never entity
movement — a deliberate divergence from piececraft-3d's scheme, not a
misunderstanding of it.

Implementation stayed entirely inside `ops/wraith_project_input.c` and
the renderer — **no changes needed to Wraith's core dispatch this time**,
since piececraft-wraith already routes all raw keys through its own
synchronous op while in map-control mode. Added:
- `cam_pan_x`, `cam_pan_z`, `camera_mode` to `GameState`, persisted to
  `state.txt` (replacing dead placeholder fields — `cam_pitch`/`cam_yaw`/
  `cam_roll` were written but never read by anything).
- `W/S` pan forward/back, `A/D` pan left/right, `1/2/3` switch fixed POV
  presets (different pitch/height/distance combos in the renderer).
- One key conflict resolved: `D`/`d` was debug-toggle before this — moved
  debug to `9` to free the whole WASD cluster for camera, since the user
  was explicit that WASD is 100% camera, not shared with anything else.
- Renderer (`draw_tile_zmap_preview_3d()`) now parses
  `camera=mode,pan_x,pan_y,pan_z,yaw,pitch_delta,0.00` from the label
  instead of using hardcoded `pitch=22/cam_y=7/cam_z=-16` constants — pan
  offsets are added on top of whichever POV preset's base position is
  active. (Field layout updated 2026-07-03: slots 5/6, formerly dead
  placeholders, now carry mouse-drag camera orbit — `yaw` is a turntable
  spin about the map center, `pitch_delta` is added to the preset tilt.
  See `py3d-inspo.md` for the full mouse-orbit pipeline, gated to
  map-control mode.)
- Buttons added for all of this in `scene.objects.pdl` (same `tag=control`
  + `KEY:` pattern as existing movement buttons) — clickable, not
  hotkey-only, per the user's standing preference from earlier in the
  project.

**Verified standalone** end-to-end (manager init → W,W,A,'2' keypresses
via the op → `state.txt` and `scene.objects.pdl`'s `camera=` field both
show `camera_mode=2, cam_pan_x=-1.00, cam_pan_z=-2.00` exactly as
expected) before the live Wraith test.

---

## Known Issues (live-tested, deferred — not forgotten)

Live-tested after all fixes above; 3D mode works but is not visually
complete yet:

1. **Z-level extrusion looks off.** Switching Z-levels (Q/E) changes
   which map file renders, but the vertical relationship between levels
   (how a Z1 wall should read relative to Z0's ground, if/when multiple
   levels are ever shown together) hasn't been tuned or verified — right
   now each Z-level renders as an independent flat scene with no visual
   continuity cue between levels. Needs dedicated investigation into
   whether levels should stack with a consistent world-Y offset per Z, or
   whether the current "one level fully replaces the view" model is fine
   as-is and this is just a cosmetics/depth-cue gap.
2. **Voxel boxes aren't rendering all their faces.** `draw_box()` only
   draws a top face + one camera-facing front face (see "Implementation
   (POC v2)" above — this was a deliberate POC-scope decision, not an
   oversight, since a `yaw=0` camera rarely shows left/right/back faces).
   Now that camera panning (WASD) can shift the view angle, the missing
   side/back faces are more likely to become visible and look like
   holes/see-through geometry. Full 6-face box rendering (or at least
   left/right faces) is the natural next step if panning makes this
   visually obvious in practice.

Both are real, known gaps — not new bugs introduced by tonight's fixes.
Deferred deliberately to keep moving on the manager-migration work;
revisit with dedicated time once there's a clear visual case for exactly
what "correct" should look like (ideally against a live-running
reference, not just static reasoning about camera geometry).

**Update (2026-07-03):** that clear visual case now exists — see
`py3d-inspo.md` for a root-cause diagnosis (grass tiles defaulting to
`extrude_dir=down` punch a visible pit per floor tile; combined with
issue #2 above, this produces the "warped floor + floating pillar" look
seen in a live screenshot) and a concrete, ordered remedy plan compared
against `#.plugy3d-engine-2026_v19`'s actual rendering/camera code. That
doc also covers the POV-preset gap: today's `camera_mode` 1/2/3 presets
are fixed constant tuples that never reference the xelector's position at
all, unlike true first/third-person camera modes.

**Issue #2 resolved (2026-07-03, second pass).** `draw_box()` no longer
picks a fixed "top + 1 z-face + 1 x-face" by a yaw=0-only heuristic —
mouse-orbit (added earlier the same day) made that assumption wrong the
moment yaw went nonzero (live-reported gaps at `cam_yaw=-89.6`, correctly
predicted by the "missing faces" note above once panning/orbit existed).
Replaced with true per-face visibility: each of the 6 faces is tested
independently (camera un-rotated into the box's own local frame via
`unrotate_by_yaw()`, then a plain coordinate comparison per axis — the
box is axis-aligned, so this is exact, not an approximation), and
whichever faces are actually camera-facing get drawn — verified via a
360° yaw sweep to always yield the correct 3 visible faces with clean
transitions at the 90°/180°/270° boundaries. This is real per-face
culling now, appropriate for a persistent camera-orbitable engine, not a
fixed-viewpoint-shaped heuristic.

**Real Z-buffer added (2026-07-03, third pass) — replaces painter's-sort
for opaque geometry.** Even with the face-selection fix above, a second,
independent bug remained: `draw_tile_zmap_preview_3d()`'s paint order was
still just the original map row index (far-to-near by row), which only
happens to equal true camera-relative depth when yaw=0. Once orbiting,
that assumption breaks — a tile's actual on-screen depth no longer
matches its row number, so far geometry drawn later could paint over near
geometry. Live-reported as scene objects "getting corrupted or going in
the wrong place" while rotating. Root-caused two contributing bugs at
once: (1) no real depth test anywhere, just static row-order painting,
and (2) `project_world_point()`'s near-plane handling *clamped* z2 to 0.5
instead of rejecting geometry that crossed it — the clamp let near-camera
boxes keep rendering at a silently-blown-up scale instead of disappearing
or being properly clipped, which is what the giant distorted shapes in
the live screenshots actually were.

Fixed both, matching how Unreal/Godot actually handle this (per-pixel
hardware Z-buffer for opaque geometry; sorted back-to-front blending only
for translucency, since alpha-blending breaks straightforward depth-test
logic) — scoped to plain CPU C, no OpenCL: the scene is one `game_map`
surface (~800×550px) with a few dozen quads per frame, only re-rendered
on actual state changes, several orders of magnitude below where GPU
compute would earn back its own overhead.

- `project_world_point_ex()` — new, exposes each projected point's
  camera-relative depth (z2) and a near-clipped flag; `project_world_point()`
  is now a thin wrapper for the many callers that don't need either.
- A depth buffer (`depth_buf_begin()`/`depth_buf_end()`), scoped to one
  `game_map` surface's screen rect per render, not full-frame (the
  desktop chrome around it is flat 2D UI with no depth concept).
- `fill_quad_px()` now takes per-vertex depth, bilinearly interpolates it
  across each filled span (same edge-crossing math that already
  interpolated x), and depth-tests every pixel against the scoped buffer.
  `alpha` parameter: 255 = opaque (test+write depth); less = translucent
  (test-only + alpha-blended, depth left unwritten).
- `draw_box()`: any of a box's 8 corners near-clipped -> the whole box is
  skipped for that frame rather than drawn distorted. Gained an `alpha`
  parameter threaded through to `fill_quad_px()`.
- `draw_tile_zmap_preview_3d()` now renders in two passes: opaque tiles +
  entities first (depth-tested+written, paint order no longer matters for
  correctness), then translucent tiles (`alpha<255` in `.tile.txt`)
  back-to-front, depth-tested against the opaque pass but not
  depth-writing.
- New translucent test tile: `glass.tile.txt` (`alpha=110`), glyph `g` in
  piececraft-wraith's registry, placed on `map_01_z0.txt` — proves both
  the blend (visibly not pure glass color) and correct occlusion (hidden
  when something opaque is in front of it) end-to-end.

Verified standalone (not just reasoned about): wrong-paint-order test
(nearer box drawn *first*, farther box drawn *second* — nearer still
wins, proving the depth test resolves it regardless of draw order),
near-clip reject test (a box placed on top of the camera leaves the
frame buffer unchanged instead of drawing a distorted shape),
translucent-blend test (result color is a genuine blend, not pure glass
color), translucent-occluded-by-opaque test (a translucent box fully
behind an opaque one produces zero change to the frame buffer). All four
passed before this was called done.

**Proper near-plane frustum clipping (2026-07-03, fourth pass).** Even
with the Z-buffer, a nearer box could still visually lose to a farther
one at extreme camera angles — traced to `draw_box()`'s near-plane
handling still being "reject the whole box if any corner crosses the
near plane," which is cruder than real clipping and can make a mostly-
fine box vanish outright instead of rendering its visible portion. Fixed
with actual Sutherland-Hodgman clipping: `world_to_camera_space()`
factors the pre-divide transform out of `project_world_point_ex()`;
`clip_poly_near()` clips a face's camera-space quad against the near
plane, inserting real vertices at the boundary (a quad becomes a
pentagon at most, never discarded wholesale unless fully behind);
`draw_clipped_face()` runs this per face before projecting; `fill_quad_px()`
generalized to `fill_poly_px()` (3-8 vertices, was fixed-4) to rasterize
whatever survives. This is what GL actually does for the near plane —
confirmed correct behavior via the 360° yaw sweep and dedicated
in-front/behind-camera/straddling-near-plane tests.

## Pivot: Ray Marching Replaces Rasterization For The Tile/Voxel Grid (2026-07-03)

Even after the Z-buffer and proper near-plane clipping above, a nearer
tile (a tree) could still disappear behind a farther one (a wall) at
certain camera angles — a real, reproduced bug, not a misunderstanding of
turntable-rotation semantics (that part — left/right inverting when
you rotate 180° — is separately confirmed correct and expected for an
orbit camera, see the analysis above it).

**Root framing, not just root cause:** rasterization + a Z-buffer is the
right tool when geometry is arbitrary and irregular — that's what Unreal
and Godot are built for, and why they use it. **Our world isn't
arbitrary geometry. It's a small, regular, axis-aligned grid of boxes at
integer coordinates** — the same shape of problem Minecraft (and every
classic voxel engine before it) solves. We are not attempting anything
more sophisticated than Minecraft's world here — one xelector, one
handful of tile types, one small grid — so there is no reason to reach
for a rendering technique built for polygon soup when a much simpler,
provably-correct-by-construction technique fits our actual shape of data
exactly. That is the standing rationale for this pivot, not "the
rasterizer has bugs, patch it again."

**Why ray marching (voxel DDA traversal) structurally can't have this bug
class:** instead of projecting box faces to screen space and comparing
computed depth values (which is where every bug this session lived —
face selection, near-clip handling, painter's-sort-by-row before that),
ray marching walks a ray **outward from the camera, one grid cell at a
time**, in near-to-far order, and stops at the *first* solid cell it
finds. "Nearer wins" isn't a comparison that can be gotten wrong — it's
the literal order the algorithm visits cells in. There is no face
selection (a hit just is whichever axis-aligned slab boundary the ray's
parametric `t` landed on), no near-plane clipping needed for the world
geometry (a ray either starts past the near plane or doesn't; grid cells
behind the camera are never visited because marching only ever proceeds
outward), and no z-fighting between two different tiles (each pixel
resolves to exactly one winning cell, by construction).

**Hybrid, matching the ask exactly:** ray march the solid tile/voxel grid
(walls, trees, stone, emoji-voxel tiles — the actual "world" content,
and where 100% of this session's rendering bugs lived). Keep the
existing box-rasterization + Z-buffer path for the small number of
overlay entities (xelector/pet markers — at most two on screen, not
worth a rewrite), composited against the ray-marched world by sharing
the same scoped depth buffer (`depth_buf_begin()`/`g_depth_buf`) — the
ray marcher writes a real per-pixel depth for every hit, so entities
drawn afterward via the existing rasterized path still depth-test
correctly against the ray-marched world.

**What ray marching does NOT replace:** the ground wireframe grid (still
plain decorative lines, drawn the same way as always — it was never
implicated in any of this session's bugs), the 2D top-down preview path
(`draw_tile_zmap_preview()`/`draw_voxel_grid_2d_thumbnail()` — an
inherently different, already-correct orthographic view), and the
translucent tile pass (glass/water — kept as a rasterized overlay pass
after the ray march, same back-to-front/depth-tested-not-written model as
before; ray-marching translucency properly is a real can of worms —
refraction, multiple transparent surfaces along one ray — genuinely out
of scope for "basically Minecraft").

**Known simplification, stated up front, not discovered later:** for
`voxel_source` (emoji) tiles, the ray march tests occlusion against the
tile's full outer bounding box (solid, like any other tile), then
samples the CSV at the hit point's local (u,v) for color. A CSV pixel
that's transparent (`alpha<=10`) at that exact (u,v) falls back to the
tile's plain `rgb_top`/`rgb_side` rather than the ray continuing through
the gap to whatever is behind it — i.e. the *silhouette* (the tile's
footprint) is solid for occlusion purposes even where the actual emoji
art has empty pixels. Revisit only if that specific visual gap becomes a
priority; it wasn't part of the bug being fixed here.

**Bug found during rollout, fixed same day: the camera-to-grid gap.** The
camera sits well outside the grid's own footprint (e.g. `cam_z=-16`
while rows start at `z=0`), so the DDA's starting cell is far outside
`[0,row_count)`/`[0,col_count)`. The walk's out-of-range safety check
(`row < -1 || row > row_count`, etc.) was tripping on literally the
first step for every ray, before the walk ever got the chance to bridge
that gap — so nothing solid rendered at all except whatever the
per-tile `max_steps` budget accidentally covered. Fixed by first
intersecting the ray against the grid's overall XZ footprint (a plain
slab test, `y` unconstrained) and starting the DDA at that entry point
instead of at the raw camera position — the same "skip empty leading
space before a bounded volume" trick real ray marchers use. See
`ray_aabb_hit()`'s use inside `raymarch_tile_grid()` for the entry-point
test.

**Bug found during rollout, fixed same day: inconsistent off-widget
overflow.** Before the pivot, `draw_line_px()`/`draw_box()` never
clipped to the game_map widget's screen rect — a projected point could
land anywhere in the full canvas, and the ground grid + entity markers
have always bled past the window edge this way. `raymarch_tile_grid()`,
being a per-pixel scan rather than a projected shape, has no equivalent
of "just happened to land outside" — it only ever visits pixels within
whatever rect it's told to scan, which was always exactly the widget
rect. Net effect: the grid/markers overflowed the window but ray-marched
tiles (walls, trees, stone, emoji) did not, an inconsistent look. Fixed
with a scoped render-clip rect (`render_clip_begin()`/`render_clip_end()`/
`render_clip_visible()`, mirroring the existing `g_depth_buf` scoping
pattern) that `put_px()` and `fill_poly_px()` both consult, plus a new
`pieces/config/wraith_debug.conf` key, `render_beyond_viewport` (default
1/on): when on, `raymarch_tile_grid()` is handed the full canvas to scan
(matching the grid/markers' already-unbounded behavior) and nothing gets
clipped; when off, the clip rect is active and *everything* — grid,
markers, ray-marched tiles alike — is strictly boxed to the widget's own
rect. One switch, one consistent behavior either way.
