# py3d-inspo: What plugy3d-engine Gets Right, What Wraith 3D Gets Wrong, How To Fix It

Date: 2026-07-03
Status: Analysis + remedy plan. Not yet implemented — this is the "what
and why" doc to work from, per owner request after reviewing a live
screenshot of piececraft-wraith's current 3D mode next to plugy3d-engine's
"Fresh Engine 2026" output.

Related docs: `ARCHITECTURE-RGB-RENDERING.md` (locks in no-OpenGL-ever,
documents the current POC's known gaps), `wra-mana-checklist.txt` (manager
migration, unrelated to rendering).

## The two references

- **Wraith 3D (ours):** `projects/wraith-alpha/plugins/wraith_rgb_daemon.c`
  — `draw_tile_zmap_preview_3d()` + `draw_box()` + `project_world_point()`.
  Pure software RGB rasterizer, no GL, file-backed (reads
  `assets/tiles/*.tile.txt`, `maps/*.txt`, `session/state.txt`).
- **plugy3d-engine-2026_v19 ("Fresh Engine 2026"):**
  `#.plugy3d-engine-2026_v19/plugins/core/voxel_grid/voxel_grid.py`,
  `plugins/core/camera_controller/camera_controller.py`,
  `plugins/rendering/entity_renderer.py`, `plugins/terrain/terrain_generator.py`.
  Real PyOpenGL (VBOs, `gluPerspective`, `gluLookAt`) — this is the project
  the owner pointed at as the visual/behavioral target ("gl-os 3d").

**We are not adopting OpenGL, Qt, or VBOs.** The "no OpenGL, ever — RGB is
the pure signal" decision in `ARCHITECTURE-RGB-RENDERING.md` stands. What
we're borrowing from plugy3d is purely the *data model and camera
semantics* — the shape of the math and the shape of the file-backed
"receipts," not the renderer.

## TL;DR verdict on the screenshot

The "corruption" in the piececraft-wraith screenshot (warped-looking
floor, brown/blue pillars that look disconnected and floating) is not
random breakage — it traces to two specific, fixable causes:

1. **Floor tiles punch a pit instead of staying flat.** `grass.tile.txt`
   has `extrude=1.0` and no `extrude_dir`, which defaults to `down`
   (`wraith_project_input.c` / tile-meta loader). That means every single
   walkable floor tile is rendered as a full 1.0-unit-deep box with its
   own visible front face (the trench wall), not a flat reference plane.
   Stack that across a whole grid, painted tile-by-tile with no depth
   buffer, and you get exactly the "warped plaid" floor in the screenshot.
2. **Boxes only ever draw 2 of 6 faces.** `draw_box()` draws a top face
   plus one hardcoded "camera-facing" front face — this was flagged as a
   known, deliberate POC gap in `ARCHITECTURE-RGB-RENDERING.md` already.
   Trees/walls/stone (all `extrude_dir=up`) are visually thin/paper-like
   pillars because their left/right/back faces never get drawn — from any
   angle other than dead-on, you're looking at a two-sided cutout, not a
   solid block.

plugy3d avoids both problems structurally, not by accident — see below.

## Side-by-side: how plugy3d actually gets its look

### 1. The floor is a flat wireframe, not a box

`voxel_grid.py::draw_grid()` draws the entire ground plane as `GL_LINES`
at a constant `y=0` — literally just a grid of lines, no solid geometry,
no extrusion, no per-tile "box":

```python
def draw_grid(self):
    glColor3f(0.3, 0.3, 0.3)
    glBegin(GL_LINES)
    for i in range(self.grid_size + 1):
        glVertex3f(i * self.cell_size, 0, 0)
        glVertex3f(i * self.cell_size, 0, self.grid_size * self.cell_size)
        ...
```

Entities/terrain get an optional flat "foundation" quad
(`draw_flat_cube_immediate`) that is only `size * 0.1` tall — a thin
marker disc, not a pit or a box. Terrain blocks (the actual
Minecraft-style ground blocks) don't even get a foundation; they're pure
voxel geometry sitting on top of the `y=0` datum.

**This is the single biggest structural difference.** In plugy3d, "the
ground" and "a thing standing on the ground" are different render paths
with different heights. In ours right now, they're the same `draw_box()`
call with the extrude direction flipped — which is why a floor tile and a
tree both "read" as boxes of similar visual weight, and why the floor
looks like a trench instead of a surface.

### 2. Every voxel gets all 6 faces

`entity_renderer.py::generate_voxel_vertices()` explicitly emits Front,
Back, Top, Bottom, Right, and Left quads for every single voxel in a
model — full closed geometry, so from any camera angle you're looking at
a solid shape, never a cutout. This is true whether the "voxel" is a
whole terrain block or a tiny piece of an emoji-sprite entity.

Ours (`draw_box()`) emits top + one side, permanently. `ARCHITECTURE-RGB-RENDERING.md`'s Known Issues section already named this
("Voxel boxes aren't rendering all their faces") as a deliberate,
deferred POC gap — the plugy3d comparison confirms it's exactly the right
next fix, not a nice-to-have.

### 3. Shape comes from real per-tile data ("receipts"), not two colors + a height scalar

Our tile files (`assets/tiles/*.tile.txt`) are `rgb_top` + `rgb_side` +
one `extrude` scalar + `extrude_dir`. That's a flat-colored box, full
stop — a tree is the same shape as a wall, just taller.

plugy3d's `terrain_generator.py::TerrainBlock.load_voxel_model()` loads
each block type's actual shape from a CSV of per-voxel `(x,y,z,r,g,b)`
rows (or an 8x8 2D tile expanded to a cube via 6-face duplication) — this
is the same convention already established elsewhere in this codebase
(emoji-studio's `pieces/*/voxels_8.csv`, referenced in
`wra-mana-checklist.txt`'s note on `emoji-studio-wraith`). A tree's canopy
and trunk are genuinely different voxels, not one uniform colored column.

**"Better receipts" = give each tile type an actual voxel/shape file**,
not just two colors and a scalar. This is the literal meaning of "adding
better 3d information to the receipts."

### 4. Camera/POV model is selector-anchored for 1st/3rd person, target-anchored for free-cam

`camera_controller.py::apply_camera_transformations()`:

- **Free Camera** (`pov_mode == 2`): orbits a pannable `target` point
  (WASD pans `target[0]`/`target[2]`, mouse drag adjusts `pitch`/`yaw`,
  scroll adjusts `zoom`). This is genuinely independent of where any
  entity is.
- **First Person** (`pov_mode == 1`): `gluLookAt` eye is placed *at the
  selector's live world position* (`selector_pos * cell_size`, plus an eye
  height), looking along a rotatable `camera_direction` (Q/E turns it).
- **Third Person** (`pov_mode == 0`): `gluLookAt` eye is offset from the
  *same selector position* by a fixed distance/height behind
  `camera_direction`, looking back at the selector.

So 1st/3rd person are both **anchored to and follow the selector**, and
have their own **independent facing/turn control** (Q/E) separate from
movement. Free cam is the only mode with an independent pannable target.

Ours (`draw_tile_zmap_preview_3d()`'s `switch (camera_mode)` block) sets
`pitch` / `cam_y` / `cam_z` / `focal` to three fixed constant tuples per
preset, and `cam_x = pan_x` (pure WASD/ZX pan, global). **None of the
three presets reference the xelector's position at all.** "1/2/3" today
are just three different fixed viewing angles on the same static scene —
not first-person, third-person, and free-camera in the plugy3d sense.
That's the literal cause of "POV switching doesn't work the way it's
supposed to" — there's no selector-following behavior to switch into.

## Remedy plan (ordered, incremental — each step independently testable)

**Status (2026-07-03): steps 1 and 2 implemented.** See
`wraith_rgb_daemon.c` (`draw_box()`, `draw_tile_zmap_preview_3d()`,
`load_tile_meta()`) and `wraith-projects/piececraft-wraith/assets/tiles/*.tile.txt`.

1. **Flatten the floor — DONE, but not the way first proposed.** The
   `extrude_dir` field is gone entirely (removed from `TileMeta` and every
   `.tile.txt`); every solid tile now just has `extrude` (a height) and
   always grows up from `wy=0`.
   The actual floor fix ended up different from the original plan above:
   rather than a per-tile "wireframe" render-mode field on individual
   tiles (tried, then corrected — a ground plane isn't a "piece" and
   shouldn't be registered per-glyph in `assets/tiles/registry.txt`), the
   ground is now **one shared wireframe grid, sized to the whole map**
   (`col_count` x `row_count`), drawn once per frame directly in
   `draw_tile_zmap_preview_3d()` before the per-tile loop — a literal
   match for plugy3d's `draw_grid()`, which is also one shape sized to
   `grid_size`, not a per-cell object. Walkable tiles (`walkable=1` in
   their `.tile.txt` — the field already existed, reused as-is, no new
   field invented) draw nothing per-tile at all now; only non-walkable
   tiles (`wall`/`tree`/`stone`) still draw a solid `draw_box()` + their
   own top-outline. This directly kills the "warped trench floor" look
   (root cause #1) without inventing a floor-specific tile/piece concept.
2. **Draw the remaining visible box faces — DONE, then generalized
   further (2026-07-03, second pass).** Originally: `draw_box()` drew
   top + near(z) face + one x-facing side face, chosen by comparing the
   box's center-x to `cam_x` — reasonable when the camera model had pitch
   but no yaw. Once mouse-orbit (below) added yaw, that heuristic broke:
   at large orbit angles it kept filling the same 2 faces regardless,
   leaving real gaps where the actually-visible faces should have been
   (live-reported at `cam_yaw=-89.6`). Replaced with true per-face
   visibility — all 6 faces tested independently via `unrotate_by_yaw()`
   (undoes the orbit rotation to find the camera's position in the box's
   own local frame, then a plain per-axis coordinate comparison, exact
   for an axis-aligned box) — verified correct across a full 360° yaw
   sweep. A shared `project_tile_top()` helper computes the 4 top corners
   for both `draw_box()` and the (boxless) walkable-tile grid-line path.
3. **Give tiles real shape data ("better receipts") — DONE (2026-07-03).**
   Tiles can now declare `voxel_source=<path>` in their `.tile.txt`
   pointing at a `voxels_N.csv` (same `# resolution=N` convention already
   used by emoji-studio/emoji-studio-wraith) — see
   `emoji-entity-feature-report.md` for the full pipeline and
   `draw_voxel_grid_extrusion()`/`draw_voxel_grid_2d_thumbnail()` in
   `wraith_rgb_daemon.c`. Falls back to the plain flat-box behavior when
   absent, so this is additive, not a rewrite. Three real emoji-derived
   tiles (`tree_emoji`/`rock_emoji`/`dog_emoji`, glyphs `e`/`k`/`d`) prove
   it end-to-end on `map_01_z0.txt`. In 3D, voxels stand as a thin
   vertical relief facing the camera (image-x→world-x, image-y→world-
   height) rather than lying flat as a top-down footprint mosaic — a
   standing sign/statue is what actually reads as recognizable from
   piececraft-wraith's oblique POV camera; a flat top-down mosaic (which
   is what the emoji-studio POC itself does) only reads correctly from
   directly overhead. In 2D (already an inherently top-down view), the
   same CSV renders as a flat face-up pixel-art thumbnail instead —
   different orientation per view, same underlying data, no duplicate
   asset needed.
**Mouse camera orbit — DONE (2026-07-03), added on top of the plan.** Both
plugy3d and gl-os let the mouse move the camera; that now works here too,
gated to map-control (interact) mode only. Pipeline: the `MOUSE_MOVE 0 x y`
drag stream Wraith already receives now, while `read_project_map_control()`
is true, gets diffed per-event in `wraith-alpha_manager.c`'s `handle_mouse()`
and forwarded to the active project's op as a `MOUSE_DRAG: dx dy` history
entry (the exact same forward path as `KEY_PRESSED`, ~2px deadzone). The op
(`wraith_project_input.c`) turns dx into `cam_yaw` (turntable spin about the
map center) and dy into `cam_pitch` (delta on top of the POV preset's tilt,
clamped ±40°). The renderer applies yaw as a turntable rotation about the
scene-center ground pivot inside `project_world_point()` (yaw=0 is a pure
no-op, so all existing WASD/POV behavior is unchanged until you actually
drag). The `camera=` field in `scene.objects.pdl` grew from
`mode,pan_x,pan_y,pan_z,15.00,0.00,0.00` (last 3 dead) to
`mode,pan_x,pan_y,pan_z,yaw,pitch_delta,0.00` — slots 5/6 now live. Outside
interact mode the mouse just moves the desktop cursor as before. Not yet
done: mouse-wheel zoom (plugy3d has it), and orbit is turntable-about-center
rather than about the xelector.

**Default-view reset on open — DONE (2026-07-03).** Camera state
(mode/pan/yaw/pitch) persists in `session/state.txt`, so without a reset a
window reopens at whatever angle it was last orbited/panned to. Fix is
data-driven and generic: a project can ship `assets/camera_default.txt`
(plain `key=value` lines), and wraith-alpha's `reset_project_view_from_default()`
— called from `launch_window_instance()` right after the existing
map-control reset — overwrites exactly those keys in `session/state.txt`
on **every** window open. (The manager itself only runs once per session,
lock-gated, so it's the wrong hook for "each time"; `launch_window_instance`
fires on every open/reopen.) Only the listed keys are touched, so
piececraft's file resets camera/view but intentionally leaves `xel_*`
(player position) alone. Projects without the file are unaffected.
Verified standalone: a dirtied state (mode=2, pans set, yaw=137.5,
pitch=40) resets to the file's defaults while `is_map_control`, `xel_*`,
`display_mode`, `pet_*`, and the long `game_map=` line survive byte-intact.
If a reset on *entering interact mode* (rather than on open) is ever
wanted too, it's a one-line add in route_command's INTERACT branch.

4. **Make 1st/3rd person actually follow the xelector — not done yet.** Change the
   `camera_mode` cases in `draw_tile_zmap_preview_3d()` (and the
   corresponding state in `wraith_project_input.c`) so that instead of
   three fixed constant tuples, `cam_x`/`cam_z` are derived from
   `sel_x`/`sel_y` (the xelector's live grid position) the way
   `camera_controller.py` derives `target_x`/`target_z` from
   `selector_pos`. Keep "free camera" (today's default WASD/ZX pan,
   independent of the xelector) as its own distinct mode — don't collapse
   all three into selector-following. Whether to add a turn/yaw control
   (plugy3d's Q/E `camera_direction`) on top of this is a follow-up
   decision, not required to fix the immediate "doesn't work like
   plugy3d" complaint — anchoring to the xelector is the core fix.
5. Re-test visually against a live Wraith window after each step,
   comparing to a plugy3d screenshot at a roughly matching camera angle,
   before moving to the next step — same "verify with a standalone
   harness / real reference before touching the daemon" discipline used
   for the original perspective-math fixes.

## Explicit non-goals

- Not adopting OpenGL, Qt/PySide6, or VBOs. The RGB software rasterizer
  stays the renderer.
- Not copying plugy3d's mouse-orbit free camera 1:1 (no mouse input path
  exists in Wraith today) — WASD/ZX pan stays as our free-cam input
  scheme unless a future ask changes that.
- Not solving the Z-level-stacking visual-continuity gap noted in
  `ARCHITECTURE-RGB-RENDERING.md`'s Known Issues #1 — that's a separate,
  still-deferred issue, orthogonal to the floor/faces/camera fixes above.
