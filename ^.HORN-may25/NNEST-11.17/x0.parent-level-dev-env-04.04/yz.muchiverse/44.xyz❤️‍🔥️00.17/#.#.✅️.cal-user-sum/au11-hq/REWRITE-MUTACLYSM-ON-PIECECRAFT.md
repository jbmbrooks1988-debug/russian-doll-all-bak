# REWRITE PLAN: Mutaclysm on Piececraft

**Date**: 2026-08-17
**Status**: PROPOSED
**Author**: opencode

---

## 1. Why Rewrite

Mutaclysm's camera, controls, and input pipeline are fundamentally broken due to legacy architectural debt:
- Multiple orchestrator instances clobber shared state files
- The chtpm_parser_pal → interact_relay → game_dispatch chain has triple-fire bugs
- Camera_control.c, move_player.c, and choice.c fight over hero/state.txt fields
- Direction parity between piececraft's board-viewer and mutaclysm's camera has required 4+ correction passes

Piececraft is the **stable, proven base**: clean CHTPM module loop, session isolation via button.sh, working board-viewer widget, working phymoji pipeline, no legacy debt. The plan is to **port mutaclysm's unique game mechanics onto piececraft's architecture** — not merge the two codebases, but rebuild mutaclysm as a piececraft app.

---

## 2. What Piececraft Already Has (Keep As-Is)

These are the stable foundations we inherit:

| System | Source | Status |
|--------|--------|--------|
| Orchestrator + session isolation | piececraft/system/orchestrator.c | Working |
| CHTPM parser + module loop | piececraft/system/chtpm_parser_pal.c + pal/ | Working |
| keyboard_input + relay pipeline | piececraft/system/keyboard_input.c | Working |
| Board-viewer widget (3D GL) | &.widgits/board-viewer/ | Working |
| Phymoji emoji→voxel pipeline | piececraft/ops/pc_phymoji_gen.c | Working |
| Clock daemon (autotick) | piececraft/ops/pc_clock_daemon.c | Working |
| Chunk-based world gen | piececraft/ops/pc_generate_chunk.c | Working |
| button.sh (run/kill/build) | piececraft/button.sh | Working |
| Session symlinks + real_project_root | piececraft/button.sh run | Working |
| Sun/moon orbital mechanics | piececraft/ops/pc_clock_daemon.c | Working |
| Animal AI (chicken wander) | piececraft/ops/pc_clock_daemon.c tick_animals() | Working |
| Terrain legend (glyph→color) | piececraft/pieces/system/terrain_legend.txt | Working |
| Keybinds system | piececraft/pieces/system/keybinds.txt | Working |
| Master ledger (event log) | piececraft/data/master_ledger.txt | Working |

---

## 3. What Mutaclysm Has That Piececraft Needs (Port These)

### Phase 1: Core Survival Loop (minimum playable)

These are the "mechanics we like" — the survival dungeon crawl that makes mutaclysm a game.

#### 3.1 Inventory System
- **Source**: mutaclysm/ops/pickup.c, drop.c, compose_frame.c (inventory panel)
- **Port to**: new `muta_pickup.c`, `muta_drop.c`
- **What**: Items as directories under `hero/inventory/` (russian-doll). Pickup renames from `map/items/` to `hero/inventory/`. Drop reverses.
- **Map format change**: Piececraft uses chunks (16x16x32). Mutaclysm uses flat 2D maps (40x16). We keep piececraft's chunk system but add an `items/` directory per chunk.
- **State fields**: hero/state.txt needs `inventory_weight`, item pieces need `item_id`, `category`, `power`, `pos_x`, `pos_y`

#### 3.2 Combat System
- **Source**: mutaclysm/ops/move_player.c (bump attack), tick_monsters.c (monster attack)
- **Port to**: extends `muta_move_player.c`
- **What**: Bump into monster = melee attack. Damage = best weapon power from inventory, or fists (2). Monster attacks when its chase step lands on hero tile.
- **State fields**: hero/state.txt needs `hp=100`. Monster pieces need `hp`, `damage`, `decision_mode`, `flee_hp_pct`.

#### 3.3 Monster AI
- **Source**: mutaclysm/ops/tick_monsters.c
- **Port to**: new `muta_tick_monsters.c`
- **What**: Diagonal chase/flee AI, terrain/furniture collision, simultaneous position snapshot to prevent iteration-order bias.
- **State fields**: monster pieces need `decision_mode` (0=always chase, 1=flee below threshold), `flee_hp_pct`, `damage`, `hp`.
- **Spawner system**: `spawners.txt` per map defines respawn zones (top up to 4 monsters per tick).

#### 3.4 Hunger/Thirst/Stamina (Survival Metabolism)
- **Source**: mutaclysm/ops/end_turn.c
- **Port to**: new `muta_end_turn.c`
- **What**: Per-turn: hunger+=1, thirst+=2, stamina+=5 (cap 200). HP-=1 if hunger>=100, HP-=2 if thirst>=100. Stamina used for sprinting (future).
- **State fields**: hero/state.txt needs `hunger=0`, `thirst=0`, `stamina=100`.

#### 3.5 Eating/Drinking
- **Source**: mutaclysm/ops/eat.c
- **Port to**: new `muta_eat.c`
- **What**: Scan inventory for first food/drink item. Restore hunger/thirst by item's `power`. Consume (delete) item.

#### 3.6 Crafting
- **Source**: mutaclysm/ops/craft.c
- **Port to**: new `muta_craft.c`
- **What**: Recipe file (`recipes.txt`), check inventory for materials, consume ingredients, create result.
- **Panel integration**: writes craft_panel_items to gui_state.txt for CHTPM's ACTIVATE submenu.

#### 3.7 Message Log
- **Source**: mutaclysm/ops/compose_frame.c (message_log tail)
- **Port to**: extends `muta_compose_frame.c`
- **What**: Append-only `message_log.txt`, display last 4 lines in HUD. Replaces single-message field.

### Phase 2: Advanced Systems

#### 3.8 Save/Load + Title Screen
- **Source**: mutaclysm/ops/title_input.c, compose_title_frame.c, save_game.c
- **Port to**: new `muta_title_input.c`, `muta_compose_title_frame.c`, `muta_save_game.c`
- **What**: Title screen showing "New Game" + numbered saves. Save = copy `world_01/` to `saves/save_N/`. Load = reverse.
- **Integration with piececraft**: piececraft's CHTPM module loop already supports screen transitions via `current_layout.txt`. Title screen is just another .chtpm layout + module pair.

#### 3.9 Interact Mode / Xlector Cursor
- **Source**: mutaclysm/ops/choice.c (interact_mode), move_player.c (xlector movement)
- **Port to**: extends `muta_choice.c` and `muta_move_player.c`
- **What**: Free-roaming cursor that examines tiles, possesses entities, throws weapons. Enter/Exit via panel key.
- **Key difference from mutaclysm**: In piececraft, arrow keys are handled by the board-viewer widget. The interact mode cursor must coexist with this — when interact_mode=1, arrows move the xlector instead of the board camera.

#### 3.10 Procedural Map Generation
- **Source**: mutaclysm/ops/generate_map.c
- **Port to**: new `muta_generate_map.c`
- **What**: Seeded, deterministic: biome_pass → structure_pass → population_pass → emit. Generates rooms, corridors, monsters, items.
- **Map format**: Must output in piececraft's chunk format (16x16x32 z-layers) OR we adapt piececraft to also support mutaclysm's flat maps. Recommendation: support both, with a `map_format` field in world/state.txt.

#### 3.11 Furniture Layer
- **Source**: mutaclysm/ops/compose_frame.c (furniture overlay), move_player.c (furniture collision)
- **Port to**: extends `muta_compose_frame.c` and `muta_move_player.c`
- **What**: Separate overlay layer on terrain (furniture.txt). 7 types with independent walkability. Gives richer tile composition.

### Phase 3: Polish

#### 3.12 3D Voxel Ray March Renderer
- **Source**: mutaclysm/ops/compose_rgb_frame.c (full DDA ray march)
- **Port to**: new `muta_compose_rgb_frame.c`
- **What**: Per-pixel ray march with AABB collision, perspective projection, yaw rotation, 4 camera POVs, voxel-textured walls (8x8 relief), entity extrusion boxes.
- **Integration**: piececraft's board-viewer already does 3D rendering via a separate widget. We can either: (a) port mutaclysm's inline renderer into piececraft's compose_rgb_frame, or (b) enhance the board-viewer widget with mutaclysm's features. Recommendation: (b) — keep the 3D view in the separate widget, enhance it.

#### 3.13 Emoji-to-RGB Pipeline
- **Source**: mutaclysm/ops/compose_rgb_frame.c (emoji blit pipeline)
- **Port to**: extends board-viewer widget
- **What**: FreeType rasterize → PNG → downsample to CSV → per-frame alpha composite. Auto-generate on first encounter.
- **Note**: piececraft already has `pc_phymoji_gen.c` which does the FreeType→CSV part. The missing piece is the per-frame blit in the renderer.

#### 3.14 Event-Sourcing Ledger
- **Source**: mutaclysm/ops/move_player.c (ledger writes), tick_monsters.c
- **Port to**: extends `muta_move_player.c` and `muta_tick_monsters.c`
- **What**: Every action logged with timestamp/epoch/turn/actor/position/action. Renderer can replay to reconstruct positions.

---

## 4. Architecture Decisions

### 4.1 Map Format: Dual Support

Piececraft uses chunks (16x16x32 z-layers). Mutaclysm uses flat 2D maps (40x16 with furniture overlay).

**Decision**: Support both via a `map_format` field in `world_01/state.txt`:
- `map_format=chunks` — piececraft's native format (3D voxel world)
- `map_format=flat` — mutaclysm's format (2D dungeon with furniture)

The compose_frame and compose_rgb_frame ops check this field and render accordingly. Movement and collision check against the appropriate format.

### 4.2 Game Loop: Piececraft's Module Pattern

Piececraft's PAL modules follow the proven house standard (§A.8 of HOUSE_STDS.md):
```
drain relay → initial render → loop { idle-sync → check_key → dispatch → render }
```

Mutaclysm's `game_dispatch.c` uses a different pattern (read ALL keys, batch dispatch). 

**Decision**: Use piececraft's module pattern. Each action is dispatched per-key via `pc_menu_input` (renamed to `mua_menu_input`). The batched game_dispatch pattern is abandoned — it caused the triple-fire bug.

### 4.3 Input: Board-Viewer Widget + CHTPM Relay

In piececraft, arrow keys go to the board-viewer widget (GL window) for 3D camera/xelector movement. In mutaclysm, arrow keys go through chtpm_parser_pal → interact_relay → game_dispatch.

**Decision**: Keep piececraft's model. Arrow keys are captured by the board-viewer widget's GLUT window (when focused). The widget writes MOVE commands to `widget_cmds/inbox.txt`. The game's menu_input op drains the inbox. Regular keys (digits, letters) go through the CHTPM relay for menu actions.

When interact_mode is active, the board-viewer widget switches from camera-pan to xlector-cursor mode (the widget already supports this via its own state).

### 4.4 3D Rendering: Widget, Not Inline

Mutaclysm's compose_rgb_frame.c contains a full inline 3D renderer (~800 lines). Piececraft delegates 3D to the separate board-viewer widget.

**Decision**: Keep piececraft's separation. Enhance the board-viewer widget with mutaclysm's 3D features (voxel textures, entity extrusion, 4 camera modes) rather than inlining the renderer into compose_rgb_frame. This keeps the ONE WRITER RULE clean and avoids the rgb_frame.raw race (§E.1 of HOUSE_STDS.md).

### 4.5 Title Screen: CHTPM Layout

Mutaclysm's title screen is a custom PAL loop (title_loop → game_start). Piececraft's CHTPM system already supports screen transitions.

**Decision**: Title screen = another .chtpm layout (`title.chtpm`) + module (`title_module.pal`). The module reads saves from `pieces/saves/`, renders the title view, and transitions to `main.chtpm` on commit. No custom PAL loop needed — the CHTPM parser handles it.

---

## 5. File-by-File Port Map

### From piececraft (keep, possibly extend):

| Piececraft File | Mutaclysm Equivalent | Action |
|----------------|---------------------|--------|
| system/orchestrator.c | system/orchestrator.c | Keep piececraft's (better session isolation) |
| system/keyboard_input.c | system/keyboard_input.c | Keep piececraft's |
| system/chtpm_parser_pal.c | system/chtpm_parser_pal.c | Keep piececraft's (shared-lib) |
| system/prisc+x.c | system/prisc+x.c | Keep piececraft's (shared-lib) |
| system/renderer.c | system/renderer.c | Keep piececraft's |
| system/chtpm_rgb_render.c | system/chtpm_rgb_render.c | Keep piececraft's (has emoji gen) |
| pal/main_module.pal | pal/game_module.pal | Keep piececraft's module pattern |
| pal/new_game_module.pal | — | Keep, extend for mutaclysm world gen |
| ops/pc_menu_input.c | ops/choice.c + game_dispatch.c | **Extend** with survival actions |
| ops/pc_compose_frame.c | ops/compose_frame.c | **Replace** with mutaclysm's richer renderer |
| ops/pc_generate_chunk.c | ops/generate_map.c | **Extend** with mutaclysm's room/corridor gen |
| ops/pc_clock_daemon.c | — | Keep piececraft's (better architecture) |
| ops/pc_phymoji_gen.c | — | Keep piececraft's |
| button.sh | button.sh | Keep piececraft's (better session management) |

### From mutaclysm (port these):

| Mutaclysm File | Destination | Complexity |
|---------------|-------------|------------|
| ops/end_turn.c | ops/muta_end_turn.c | Low — standalone, clean |
| ops/tick_monsters.c | ops/muta_tick_monsters.c | Medium — needs monster piece format |
| ops/move_player.c | ops/muta_move_player.c | High — merge with piececraft's xelector |
| ops/pickup.c | ops/muta_pickup.c | Medium — depends on inventory format |
| ops/drop.c | ops/muta_drop.c | Low — reverse of pickup |
| ops/eat.c | ops/muta_eat.c | Low — standalone |
| ops/craft.c | ops/muta_craft.c | Medium — recipe parsing + panel integration |
| ops/save_game.c | ops/muta_save_game.c | Low — directory copy |
| ops/title_input.c | ops/muta_title_input.c | Medium — adapt to CHTPM module pattern |
| ops/compose_title_frame.c | ops/muta_compose_title_frame.c | Low — text renderer |
| ops/compose_frame.c | ops/muta_compose_frame.c | HIGH — most complex port, but the core visual |
| ops/compose_rgb_frame.c | board-viewer enhancement | HIGH — port 3D features to widget |
| ops/camera_control.c | board-viewer (already has camera) | Low — board-viewer already has 4 modes |

### New files to create:

| File | Purpose |
|------|---------|
| ops/muta_end_turn.c | Survival metabolism tick |
| ops/muta_tick_monsters.c | Monster AI + spawner system |
| ops/muta_eat.c | Food/drink consumption |
| ops/muta_craft.c | Recipe-based crafting |
| ops/muta_pickup.c | Item pickup (directory rename) |
| ops/muta_drop.c | Item drop |
| ops/muta_save_game.c | Save/load via directory copy |
| ops/muta_title_input.c | Title screen input |
| ops/muta_compose_title_frame.c | Title screen renderer |
| pieces/chtpm/layouts/title.chtpm | Title screen layout |
| pal/title_module.pal | Title screen module loop |
| pieces/registry/recipes/recipes.txt | Crafting recipes |
| pieces/registry/monsters/ | Monster type definitions |
| pieces/registry/items/ | Item type definitions |
| pieces/registry/terrains/ | Terrain properties (walkable, damage, etc.) |
| pieces/registry/furniture/ | Furniture properties |

---

## 6. State Schema Changes

### hero/state.txt (additions to piececraft's existing):

```
# Existing piececraft fields (keep):
entity_type=hero
hp=100          # piececraft has 20, mutaclysm has 100 — use 100
pos_x=8
pos_y=8
pos_z=15
owner_id=player
chunk_x=0
chunk_y=0
interact_mode=0
xlector_pos_x=8
xlector_pos_y=8

# New fields (from mutaclysm):
hunger=0
thirst=0
stamina=100
facing=0         # last arrow direction (for 1st-person view)
map_id=map_start # current map ID (for flat-map mode)
```

### world_01/state.txt (additions):

```
# Existing piececraft fields (keep):
name=World 01
seed=<random>
tick=0
game_time_epoch_ms=...
autotick_enabled=0
autotick_speed=min

# New fields:
map_format=flat         # or "chunks" for 3D voxel mode
current_turn=0          # global turn counter (mutaclysm pattern)
```

### New state files:

- `pieces/registry/monsters/<type>/state.txt` — monster type template (hp, damage, glyph, emoji, decision_mode, flee_hp_pct)
- `pieces/registry/items/<type>/state.txt` — item template (item_id, category, power, glyph, emoji)
- `pieces/registry/terrains/<glyph>/state.txt` — terrain properties (walkable, damage, glyph, emoji, rgb_top)
- `pieces/registry/furniture/<type>/state.txt` — furniture properties (walkable, glyph, emoji)
- `pieces/world_01/<map>/spawners.txt` — spawner zones per map
- `pieces/world_01/<map>/transitions.txt` — map transitions
- `pieces/world_01/<map>/furniture.txt` — furniture overlay
- `pieces/world_01/<map>/items/<item_id>/state.txt` — ground items
- `pieces/world_01/<map>/monsters/<monster_id>/state.txt` — live monsters
- `pieces/registry/recipes/recipes.txt` — crafting recipes
- `pieces/system/save_serial_counter.txt` — save numbering

---

## 7. Build Order

### Sprint 1: Skeleton (1-2 hours)
1. Copy piececraft-xyz → mutaclysm-xyz
2. Rename all `pc_` prefixed ops to `mua_` prefixed
3. Update default_op.txt, button.sh, PAL scripts, CHTPM layouts
4. Verify: `./button.sh run` launches, shows a title screen, enters game

### Sprint 2: Core Survival (2-3 hours)
5. Port end_turn.c → muta_end_turn.c (hunger/thirst/stamina)
6. Port eat.c → muta_eat.c
7. Add hunger/thirst/stamina to hero/state.txt schema
8. Extend pc_compose_frame.c to show HP/hunger/thirst/stamina in HUD
9. Wire end_turn into the PAL module loop (call after every action)
10. Verify: HP drops when starving, eating restores hunger

### Sprint 3: Inventory + Combat (2-3 hours)
11. Port pickup.c → muta_pickup.c
12. Port drop.c → muta_drop.c
13. Add items/ directory structure
14. Port move_player.c bump-attack logic
15. Create monster piece templates (zombie, zombie_child)
16. Place initial monsters on the map
17. Verify: can pick up items, bump-attack kills monsters

### Sprint 4: Monster AI (2-3 hours)
18. Port tick_monsters.c → muta_tick_monsters.c
19. Port spawner system
20. Wire tick_monsters into PAL loop (after end_turn)
21. Verify: monsters chase, spawn from gravestones, deal damage

### Sprint 5: Crafting (1-2 hours)
22. Port craft.c → muta_craft.c
23. Create recipes.txt
24. Integrate with CHTPM ACTIVATE submenu
25. Verify: can craft items from materials

### Sprint 6: Title Screen + Save/Load (1-2 hours)
26. Port compose_title_frame.c → muta_compose_title_frame.c
27. Port title_input.c → muta_title_input.c (adapt to CHTPM module)
28. Port save_game.c → muta_save_game.c
29. Create title.chtpm layout
30. Create title_module.pal
31. Verify: can start new game, save, load from title

### Sprint 7: Map Generation (1-2 hours)
32. Port generate_map.c → muta_generate_map.c
33. Adapt output to piececraft's chunk format OR add flat-map support
34. Verify: can generate a dungeon with rooms, corridors, monsters, items

### Sprint 8: 3D Widget Enhancement (2-3 hours)
35. Port compose_rgb_frame.c's 3D features to board-viewer widget
36. Add voxel texturing (8x8 relief maps)
37. Add entity extrusion
38. Add 4 camera modes (already in board-viewer, verify parity)
39. Verify: 3D view shows textured walls, entities, matches 2D map

---

## 8. Testing Strategy

Per HOUSE_STDS.md §C:
- Kill all stale sessions before every test
- Use `NO_GL=1 bash button.sh run` for headless testing
- Inject keys via `interact_relay.txt` for automated testing
- Sample `rgb_frame.raw` pixels for 3D verification
- Wrap all tests in `timeout`

### Test Matrix:

| Test | Method | Pass Criteria |
|------|--------|---------------|
| Title screen renders | Headless, read view.txt | Contains "PIECECRAFT" or "MUTACLSYM" |
| New Game generates world | Headless, check chunks exist | 32 z-layer files present |
| Hero starts at valid position | Headless, read hero/state.txt | pos_x, pos_y on walkable tile |
| Arrow movement works | Inject 1002 into relay | hero pos_y changes |
| Bump attack works | Move into monster tile | monster hp decreases |
| Monster AI chases | Wait 2 ticks | monster pos closer to hero |
| Hunger increases per turn | Run 10 end_turns | hunger=10 |
| Starvation damages HP | Set hunger=100, run end_turn | hp decreases |
| Eat restores hunger | Have food item, inject eat key | hunger decreases |
| Pickup works | Stand on item, inject pickup key | item in inventory |
| Crafting works | Have materials, inject craft key | result in inventory |
| Save works | Inject save key | saves/save_1/ exists |
| Load works | From title, inject load key | world state restored |
| 3D render matches 2D | Compare rgb_frame.raw to map | Wall positions match |

---

## 9. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Map format mismatch (chunks vs flat) | High — breaks movement, rendering | Support both via map_format field. Start with flat, add chunks later |
| Board-viewer widget can't handle mutaclysm's 3D features | Medium — lose 3D view | Port features incrementally; 2D view works without 3D |
| CHTPM ACTIVATE submenu integration for craft/inventory panels | Medium — breaks panel UI | Test early in Sprint 5; fall back to ASCII overlay if needed |
| Item state complexity (russian-doll directories) | Medium — hard to debug | Keep items simple initially (just id + category + power) |
| Monster spawner performance with many monsters | Low — 4 per spawner, capped | Profile in Sprint 4; reduce cap if needed |

---

## 10. Success Criteria

The rewrite is complete when:
1. `./button.sh run` launches to a title screen
2. New Game generates a dungeon with rooms, monsters, items
3. Hero can move, fight, eat, craft, pick up/drop items
4. Monsters chase and attack, spawn from gravestones
5. Hunger/thirst drain over turns, starvation damages HP
6. Save/load works from title screen
7. 3D board-viewer shows the map with textured walls
8. No triple-fire, no state clobbering, no camera parity bugs
9. All tests in §8 pass
