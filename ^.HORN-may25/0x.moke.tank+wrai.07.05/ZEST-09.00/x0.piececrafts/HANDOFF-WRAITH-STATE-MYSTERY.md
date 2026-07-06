# PIECECRAFT-WRAITH: GL XELECTOR/COORDS/NAV — REAL ROOT CAUSE FOUND & FIXED
**Date:** July 2, 2026 | **Status:** ✅ GL marker CONFIRMED WORKING in live Wraith. Coords HUD + clickable nav buttons added, pending confirmation.

## UPDATE (same session, after live confirmation)

User confirmed: **the GL xelector marker is now visible in Wraith.** The amber border+crosshair fix in `draw_tile_zmap_preview()` works end-to-end.

Two follow-ups implemented immediately after:

1. **Clickable nav buttons** (not just hotkeys) — user's standing preference from earlier in this session was "we don't have to just rely on hotkeys, remember?" The layout only had raw `${game_map}`/`${xel_*}` text with no buttons after the debugging churn stripped them out. Restored full button set in `layouts/piececraft-wraith.chtpm`:
   ```xml
   <button label="[↑] Up" onClick="KEY:1002" /><br/>
   <button label="[↓] Down" onClick="KEY:1003" /><br/>
   <button label="[←] Left" onClick="KEY:1000" /><br/>
   <button label="[→] Right" onClick="KEY:1001" /><br/>
   <button label="[X] Ascend Z" onClick="KEY:120" /><br/>
   <button label="[Z] Descend Z" onClick="KEY:122" /><br/>
   <button label="[8] Toggle 2D/3D" onClick="KEY:56" /><br/>
   <button label="[D] Debug" onClick="KEY:68" /><br/>
   <button label="[ESC] Menu" onClick="ESC" /><br/>
   ```
   No compile needed (layout is plain text, read live by Wraith's parser). Cleared `.manager.lock` so the next window open picks it up fresh.

2. **2D/3D mode is still text-only, by design** — pressing `8` updates `Mode: 2D`/`Mode: 3D` in the coords HUD text row, but the GL tile grid itself does not change appearance between modes. This was already documented as expected (no 3D voxel renderer exists yet) — restating here so it isn't mistaken for a new bug when testing the nav buttons.

**Pending verification:** click the new nav buttons in Wraith (not just keyboard) and confirm both movement and the `8` mode toggle behave identically to hotkey input — button `onClick="KEY:NN"` should inject the same key code into `history.txt` that the keyboard path does, but this specific project hadn't had buttons tested since the manager rewire, so worth a quick click-through.

## UPDATE 2 (same session) — REAL FIX for missing nav buttons + frame-lag root cause

The `<button>` markup added above turned out to be **dead code**. Traced why:

### Why buttons in `.chtpm` never rendered

`wraith-alpha_manager.c` reads `entry_layout` from `project.pdl` (grep confirms exactly 2 occurrences in the whole file — both just storing the path string) but **never parses that `.chtpm` file's `<button>`/`<text>` tags for `is_map_control=1` projects**. The window body you see is synthesized entirely from a hardcoded template driven by `state.txt`'s `game_map=` field. Proof: the `[^] 5. [Control_Map]` toolbar item visible since the very first screenshot was never from our `.chtpm` — it's from `scene.objects.pdl`'s `OBJECT tag=control ... nav=5 action=INTERACT label=Control_Map` line, the only mechanism Wraith actually renders as clickable nav for hosted projects.

Removed the dead `<button>` markup from `piececraft-wraith.chtpm`, replaced with a comment pointing at the real mechanism.

### The real fix: scene.objects.pdl `tag=control` + a new `KEY:` action verb

Clicking/activating a nav item routes through `wraith-alpha_manager.c`'s `action_for_nav_index()` → `route_command()`. That function already handled `INTERACT`, `ESC`, `PROJECT_ACTION:`, `DESKTOP_ACTION:*`, etc. — but nothing that injects a raw key press. Added:

```c
if (strncmp(cmd, "KEY:", 4) == 0) {
    int key_code = atoi(cmd + 4);
    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "%d", key_code);
    append_project_history("KEY_PRESSED", key_buf);
    run_active_project_input_op();
    update_state(key_code);
    trigger_render();
    return;
}
```

Then added 8 new `OBJECT tag=control ... action=KEY:N` lines to piececraft-wraith's `update_scene_objects()` (Up/Down/Left/Right/Ascend/Descend/ToggleMode/Debug), positioned in a column right of the map (`x=84`).

### Nav-slot collision risk (handled carefully)

Nav indices 1–4 are hardcoded to Wraith's window chrome (focus/minimize/close); index 5 was implicitly shared between a project's single `Control_Map`-style control and the first entry of the nested-project launcher list — a **pre-existing overlap** in the original code (only avoided because `action_for_nav_index()` always wins for real actions). Adding 8 more project controls needed 8 more non-colliding nav slots, or they'd stomp on launcher/taskbar indices.

Fixed by adding `count_project_nav_controls()` (counts `tag=control` objects with `nav>0` in the active window's `session/scene.objects.pdl`) and threading an `extra_project_slots = project_nav_count > 1 ? project_nav_count - 1 : 0` term through both `recompute_nav_bounds()`'s `g_max_index` formula and `dispatch_menu_index()`'s `launcher_start`/`taskbar_start` math. **When `project_nav_count <= 1` (every existing Wraith project except piececraft-wraith), `extra_project_slots = 0` and the formula is byte-identical to the old hardcoded one** — zero behavior change for any other project. Only piececraft-wraith's 9 total project controls (Control_Map + 8 buttons) push launcher/taskbar indices out by 8, and the very last one still (harmlessly, matching prior behavior) shares its slot with the first launcher entry if any nested projects exist.

Hit one unrelated compile error while adding `count_project_nav_controls()`: `active_window(void)` had never needed a forward declaration before (all its original call sites happened to be textually after its definition). Added `static Window *active_window(void);` to the prototype block — trivial, no behavior change.

**Compiled and verified standalone** (both `wraith-alpha_manager.+x` and `piececraft-wraith_manager.+x`); `scene.objects.pdl` output confirmed to contain all 9 `tag=control` lines with correct unique nav numbers 5–13. **Not yet click-tested live in Wraith.**

### Frame lag ("lags behind 1 keypress") — root cause CONFIRMED, fix NOT yet implemented

User reported the same one-frame-behind symptom seen previously in fuzz-op, and pointed at `!.gem-flashlite--yolo/#.docs/^.pmo.ld-faq+8/PITFALLS_ACTIVE_2026-03-18.txt` for prior art. Pitfall **#36 "ONE VISIBLE FRAME WRITER RULE"** describes the general symptom class (`state files advance, visible frame lags, debugging becomes misleading`) but this project's actual mechanism is more specific and was traced exactly:

- `wraith-alpha_manager.c`'s raw-key handler, when a project is in `is_map_control` mode, does this **synchronously, on every keypress**:
  ```c
  append_project_history("KEY_PRESSED", key_buf);
  run_active_project_input_op();   // <-- looks for ops/+x/wraith_project_input.+x
  update_state(key);
  trigger_render();                // <-- composes the visible frame RIGHT NOW
  ```
- `run_active_project_input_op()` execs `<project>/ops/+x/wraith_project_input.+x` **if it exists** and waits for it (`waitpid`) before continuing. This is the exact op-invocation convention `wraith-3d-cube` and `wraith-ed` use (both are ops-only, no manager) — it guarantees the project's state is fully updated *before* `trigger_render()` composes the frame.
- **piececraft-wraith has no `ops/+x/wraith_project_input.+x`** (confirmed: `ls ops/+x/` shows only `render_map_wraith.+x`). So `run_active_project_input_op()` silently no-ops (`access(op_path, X_OK) != 0` → return).
- The *only* thing that updates piececraft-wraith's `state.txt`/`scene.objects.pdl` for that keypress is our **manager's separate pthread**, which polls `history.txt` on a `usleep(16667)` (~16.6ms) cycle — running fully asynchronously from wraith-alpha's synchronous key-handler/render cycle.
- Net effect: `trigger_render()` composes the frame using state from *before* this keypress (the manager hasn't woken up yet), and the just-processed keypress's effect only becomes visible on the **next** `trigger_render()` call (triggered by the *following* keypress). This is a textbook one-frame-behind bug, and it's structural, not timing-flaky — it will reproduce every time regardless of machine speed, because the race is between a synchronous call and an async poll interval, not between two synchronous paths.

**This is the same class of bug fuzz-op hit before** (per user), and the underlying reason is the same: a manager built as an async background poller doesn't fit the frame-per-synchronous-call model that Wraith's hosted-project dispatch actually uses on the hot input path.

### UPDATE 3 (same session) — Fix IMPLEMENTED and verified standalone

Before implementing, checked whether this was actually grounded in real working code or invented. It was a mix — the `scene.objects.pdl` sync and GL marker fix (Update 1/2 above) were grounded in real code (`wraith-3d-cube`, `wraith_rgb_daemon.c`), but the frame-lag fix plan hadn't yet been checked against a real implementation. Read both real, currently-compiled examples before writing anything:

- `wraith-3d-cube/ops/src/wraith_project_input.c`
- `wraith-ed/ops/src/wraith_project_input.c`

Also checked whether fuzz-op's manager does something structurally different that avoids this race (user's hypothesis, since fuzz-op reportedly had this same lag bug historically). It does **not** — `fuzz-op_manager.c` uses the exact same `usleep(16667)` pthread-polling-`history.txt` pattern our manager had. Copying it would have changed nothing. The actual reason fuzz-op's version of this race is invisible: its renderer (`chtpm_parser.c`, the top-level mother-terminal parser) polls `frame_changed.txt` continuously on its own independent loop, so the race resolves within a few ms rather than being stuck for a full keypress cycle like Wraith's one-shot-per-keypress `trigger_render()`. Wraith's dispatcher already has the correct fix built in (`run_active_project_input_op()`, the same synchronous op-invocation wraith-3d-cube/wraith-ed use) — it just wasn't wired up for piececraft-wraith.

**Implementation:** created `projects/wraith-alpha/wraith-projects/piececraft-wraith/ops/src/wraith_project_input.c`, following the exact structure of the two real examples:
- `main(argc, argv)`, `argv[1]` = project root (matches what `run_active_project_input_op()` passes: `execl(op_path, op_path, project_dir, NULL)`, confirmed by reading `active_project_dir()` → `project_dir_for_window()`)
- `load_state()` reads current `xel_x/y/z`/`display_mode`/`debug_mode_on` from `session/state.txt` fresh on every invocation (this is a one-shot process now, not a persistent daemon — no in-memory state carries over between calls)
- `session/history.cursor` tracks the last-read byte offset into `history.txt`, so each invocation only processes *new* `KEY_PRESSED:` lines since the last run (exact same cursor pattern as the two reference files)
- `apply_key()` — ported directly from the old manager's `route_input()` switch statement
- `render_body()` / `save_state()` / `write_scene()` — ported directly from the old manager's `render_map()` / `save_state_txt()` / `update_scene_objects()`, unchanged in content, just restructured to take state as a parameter instead of globals
- Only writes anything if `changed` (matching the reference pattern's efficiency guard)

Compiled with the project's actual convention (`compile_op` from `compile_all.sh`: `gcc -pthread -lm -Wno-format-extra-args`), output to both `ops/src/+x/` and copied to `ops/+x/` (the path `run_active_project_input_op()` actually execs) — matching exactly how `compile_all.sh`'s nested-wraith-projects loop handles `ops/src/*.c` files.

**Manager simplified:** removed `input_thread()` and `route_input()` entirely from `piececraft-wraith_manager.c` — that logic now lives in the op. `main()` now only does one-time init (initial `state.txt`/`scene.objects.pdl`/`wraith_body.txt` so the window isn't blank before the first keypress) and returns immediately — no more persistent process, no more polling thread, no more race. Recompiled (30KB, down from prior size).

**Verified standalone**, three separate test passes:
1. Confirmed the op only replays genuinely new `history.txt` lines (cursor-based), not the whole file, across repeated invocations.
2. Full clean-state test: manager init → 4 sequential keypresses (RIGHT, RIGHT, DOWN, X) via separate synchronous op invocations, exactly mirroring wraith-alpha's real call pattern → final state `xel_x=6, xel_y=3, xel_z=1` — matches expected math exactly, and `scene.objects.pdl`'s `selected=6,3,1` stays in lockstep.
3. Session files reset to a clean slate (`history.txt` truncated, `history.cursor`/`​.manager.lock`/`debug_log.txt` cleared) for the actual live-Wraith test.

**Not yet done:** live click/keypress test inside the actual Wraith UI (both for the nav buttons from Update 2 and the delag fix from this update). Everything up to that point has been verified outside Wraith with the exact same call sequence Wraith uses, but the live UI hasn't been re-tested since these changes landed.

This also fully resolves the standing architectural question from earlier in this session ("even ops-based projects should be orchestrated by managers, right?") — see `wraith-man-fix-j2.md`'s update for the complete, evidence-based answer: managers own one-time init, `ops/+x/wraith_project_input.+x` owns the synchronous hot input path, and the two are not interchangeable — using a manager's async thread for the hot path is what caused this exact bug.

---

## TL;DR

Every previous attempt in this session (writing `wraith_body.txt`, `gui_state.txt`, `state.txt`'s `game_map=`, even syncing `scene.objects.pdl`'s `selected=X,Y,Z`) was **data-side plumbing that was already correct or became correct** — but none of it mattered, because **the GL renderer function that paints the tile grid never reads the `label` field at all.**

`draw_tile_zmap_preview()` in `projects/wraith-alpha/plugins/wraith_rgb_daemon.c` opens the map file named in `source_ref` and paints grass/wall/tree/rock colors character-by-character. That's it. It has zero code to parse `MAP_SOURCE:...selected=X,Y,Z...` out of the object's `label`, and it `return`s before reaching the generic text-label rendering path that every other object type gets. So:

- The GL xelector marker was **never implemented** — not broken, just never built.
- Coords and nav commands were **never implemented** either — the `tile_zmap` role short-circuits past the text/HUD rendering code entirely, and nothing else in `piececraft-wraith`'s `scene.objects.pdl` carried that text as a separate `tag=text` object.

This is a genuine engine feature gap in shared Wraith code (also used by `wraith-ed`, which writes the identical unused `MAP_SOURCE:...selected=...` label format — meaning wraith-ed's GL marker has silently never worked either).

---

## HOW THIS WAS FOUND

1. User pointed at `chtmgl-wraith` and the webcam-ascii project — both work with **no manager at all**, yet ASCII and GL mirror each other. That ruled out "the manager isn't running/isn't syncing" and forced the question: **which file does each renderer actually read, and does anything actually parse it?**
2. Grepped `wraith-alpha_manager.c` for `scene.objects.pdl` — confirmed it IS read and forwarded via `append_project_scene_objects()` into the composed `current_frame.objects.pdl`, label included, verbatim. So the data pipe itself is fine.
3. Grepped the whole tree for `MAP_SOURCE` (the label format both piececraft-wraith and wraith-ed write) — it appears in exactly **2 places, both writers, 0 readers**.
4. Found the actual GL tile renderer: `draw_tile_zmap_preview()` in `wraith_rgb_daemon.c`. Read it top to bottom — confirmed it uses only `obj->source_ref` (map file path) and `obj->x/y/w/h` for layout. `obj->label` is never referenced.
5. Confirmed via the dispatch table (`if (strcmp(obj->role, "tile_zmap") == 0) { draw_tile_zmap_preview(...); return; }`) that this role `return`s immediately — never falls through to the generic `build_display_label()` + `blit_text()` path that would otherwise render a label as HUD text.

---

## THE FIX (Two Parts)

### Part 1 — GL Xelector Marker (Wraith core engine change)

**File:** `projects/wraith-alpha/plugins/wraith_rgb_daemon.c`
**Function:** `draw_tile_zmap_preview()`

Added:
- Parse `obj->label` for `selected=X,Y,Z` via `strstr` + `sscanf` at function entry.
- Inside the existing per-tile loop, after the terrain color is chosen and drawn, check `is_selected = (x == sel_x && y == sel_y)`.
- If selected: draw an amber (`#FFD166`) border around the tile plus a small crosshair (two short lines) through its center, using the existing `draw_border_px()` / `draw_line_px()` helpers already in this file.

This is additive only — no existing behavior changed for objects without a `selected=` field in their label (the `sscanf` simply leaves `sel_x/sel_y` at `-1,-1`, which never matches a real tile coordinate).

**Recompiled:** `projects/wraith-alpha/plugins/+x/wraith_rgb_daemon.+x` (200KB) using the exact same flags as the project's `compile_all.sh` (`gcc -pthread -lm`). One pre-existing unrelated truncation warning, no errors.

**Blast radius:** This is shared engine code. `wraith-ed` uses the same `tile_zmap` role and the same `MAP_SOURCE:...selected=...` label convention (confirmed via grep), so this fix should make wraith-ed's GL marker start working too, for free, with the same data it was already writing.

### Part 2 — Coords/Nav HUD (project-level, piececraft-wraith only)

**File:** `projects/wraith-alpha/wraith-projects/piececraft-wraith/manager/piececraft-wraith_manager.c`
**Function:** `update_scene_objects()`

The `tile_zmap` role's early `return` means nothing placed *inside* that object will ever get text rendering. So instead of fighting that, added two **separate** `tag=text` objects positioned directly below the map (which occupies `y=6` through `y=19` at `x=35, w=48, h=14`):

```
OBJECT tag=text id=coords_hud role=game_hud x=35 y=20 w=48 h=1 z=24 nav=0 source=- fg=#FFD166 bg=#0B1118 border=#0B1118 action=- label=Position: (4,2,0) | Mode: 2D | Debug: OFF
OBJECT tag=text id=nav_hud   role=game_hud x=35 y=21 w=48 h=1 z=24 nav=0 source=- fg=#7EDFF2 bg=#0B1118 border=#0B1118 action=- label=Arrows move | X/Z z-level | 8 toggle mode | D debug | ESC menu
```

`tag=text` objects DO fall through to the generic `build_display_label()` + `blit_text()` path (confirmed by reading the dispatch code — `tile_zmap`/`zslice_piece`/etc. are the only roles that early-`return`), so these render as plain text rows in the GL window, updated live every time `update_scene_objects()` runs (i.e., every keypress, since it's called from inside `save_state_txt()`).

**Recompiled:** `manager/+x/piececraft-wraith_manager.+x`.

**Verified standalone** (ran the binary directly, inspected `session/scene.objects.pdl`):
```
OBJECT tag=model id=piececraft_map_01 role=tile_zmap ... label=MAP_SOURCE:...;selected=4,2,0;...
OBJECT tag=text id=coords_hud role=game_hud x=35 y=20 w=48 h=1 z=24 nav=0 source=- fg=#FFD166 bg=#0B1118 border=#0B1118 action=- label=Position: (4,2,0) | Mode: 2D | Debug: OFF
OBJECT tag=text id=nav_hud role=game_hud x=35 y=21 w=48 h=1 z=24 nav=0 source=- fg=#7EDFF2 bg=#0B1118 border=#0B1118 action=- label=Arrows move | X/Z z-level | 8 toggle mode | D debug | ESC menu
```

---

## WHY EARLIER FIXES IN THIS SESSION DIDN'T HELP (AND WEREN'T WRONG)

| Earlier fix | Was it needed? | Why it didn't fix the GL panel |
|---|---|---|
| Manager auto-launch in `wraith-alpha_manager.c` | Yes, still needed | Unrelated — this makes the manager run at all |
| `wraith_body.txt` written by manager | Yes, still needed | Feeds the ASCII text panel via `state.txt`'s `game_map=`, not the GL panel |
| `state.txt`'s `game_map=` (ascii-escaped) | Yes, still needed | This is what `${game_map}` in the layout resolves to for the **text** panel |
| `scene.objects.pdl`'s `selected=X,Y,Z` sync | Necessary but insufficient | Correctly written, correctly forwarded by Wraith's compositor into `current_frame.objects.pdl` — but the GL pixel renderer never read that field until this fix |

Nothing was wrong with the data pipeline changes made earlier — they were prerequisites. The missing piece was always downstream, in the pixel-drawing function itself.

---

## VERIFICATION STEPS (for you or the next agent)

1. Close piececraft-wraith window in Wraith if open; kill any stray manager process; clear `.manager.lock` (already done as of this handoff).
2. Reopen PIECECRAFT WRAITH in Wraith.
3. **ASCII panel** (already known-good): map + `>` xelector marker, moves with arrow keys.
4. **GL panel ("Control_Map")**: should now show:
   - An amber bordered+crosshaired tile at the xelector's position, moving with arrow keys
   - Two text rows below the tile grid: `Position: (x,y,z) | Mode: ... | Debug: ...` and the controls legend
5. Press `X`/`Z` to change z-level — confirm the GL tile grid swaps to the correct map file (`map_01_z1.txt` etc.) and the marker still tracks correctly.
6. Press `8` to toggle mode — confirm the coords HUD text updates to show `Mode: 3D`. (The tile grid itself won't change appearance — 3D voxel rendering was never implemented; only the mode label updates. This is expected, not a bug.)

If the marker or HUD text still doesn't appear after this:
- Check that Wraith actually reloaded the recompiled `wraith_rgb_daemon.+x` — this daemon may be a long-running background process started once at Wraith launch, not per-project. If so, **the whole Wraith session needs a full restart**, not just closing/reopening the piececraft-wraith window.
- Check `projects/wraith-alpha/session/rgb/current_frame.receipt.pdl` (the audit/receipt file) for a `PRIMITIVE | ... | role=tile_zmap ...` line — if `source_exists=0`, the map path resolution is failing at the compositor level, not the pixel-render level.

---

## PATTERN FOR FUTURE WRAITH-HOSTED PROJECTS WITH GL TILE VIEWS

1. `scene.objects.pdl`'s `tile_zmap` model object only controls **terrain tile colors** (from the map file) plus, as of this fix, **one highlighted marker tile** via `selected=X,Y,Z` in its `label`. It does not support arbitrary HUD text — that role `return`s early in the renderer.
2. For any text/coords/status HUD in the GL view, add **separate `tag=text` objects** positioned outside the `tile_zmap` object's `x/y/w/h` bounds (e.g., a row below it). These get full label rendering via `blit_text()`.
3. If you need more than a single-tile marker (e.g. multiple entities, path highlighting, distinct colors per entity type), `draw_tile_zmap_preview()` will need further extension — right now it supports exactly one `selected=X,Y` highlight, hardcoded amber.
4. This same fix benefits `wraith-ed` for free (it uses the identical `tile_zmap` + `MAP_SOURCE:...selected=...` convention) — worth a quick sanity check there too since it was presumably also silently broken.

---

## FILES CHANGED (this fix)

- `projects/wraith-alpha/plugins/wraith_rgb_daemon.c`
  - `draw_tile_zmap_preview()`: parse `selected=X,Y` from `obj->label`, draw amber border+crosshair marker on matching tile
  - Recompiled: `projects/wraith-alpha/plugins/+x/wraith_rgb_daemon.+x` (shared engine binary — affects all projects using `tile_zmap`)
- `projects/wraith-alpha/wraith-projects/piececraft-wraith/manager/piececraft-wraith_manager.c`
  - `update_scene_objects()`: added `coords_hud` and `nav_hud` as separate `tag=text` OBJECT lines below the map
  - Recompiled: `manager/+x/piececraft-wraith_manager.+x`
- Cleared `.manager.lock` and killed stray manager/daemon processes to force fresh state on next Wraith load

---

*End of handoff. If GL still doesn't update after a full Wraith restart, the next lead is whether `wraith_rgb_daemon` is a persistent background process that needs its own explicit restart separate from the Wraith window/project reload cycle — check its process lifecycle/launch point before debugging further into the render code, which has now been read and verified correct end-to-end for this feature.*
