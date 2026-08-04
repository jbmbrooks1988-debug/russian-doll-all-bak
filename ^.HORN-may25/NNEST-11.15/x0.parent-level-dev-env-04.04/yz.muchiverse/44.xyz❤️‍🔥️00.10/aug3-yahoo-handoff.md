# HANDOFF — yahoo-app broker list stuck on "Back to Bank"

## Date
2026-08-03 23:54 PST

## Current symptom
```
sh button.sh r
--- FRAME UPDATE ---
[>] 1. [Select Broker]
 (Ctrl+C to quit)
```

Only the static `<button label="Back to Bank">` renders. The `${piece_methods}` block is empty — no broker entries from `pieces/system/brokers.txt` appear. This is identical regardless of how many times we navigate, recompile, or restart.

---

## What we KNOW works

| Component | Status |
|-----------|--------|
| App compiles clean (`build ok`) | ✅ |
| GL mirror renders terminal content | ✅ confirmed by user |
| `frame_changed.txt` fires on keypress | ✅ terminal updates |
| `broker_select.chtpm` parses without crash | ✅ (template renders) |
| `brokers.txt` exists in session dir | ✅ 3 brokers present |
| `yahoo_compose_frame.c` writes `piece.pdl` for `broker_select` | ✅ verified via manual run |
| `mychara_menu_input.c` pattern (no parser mods) | ✅ works in my-chara-txt |

---

## What we KNOW is broken

### The core loop that SHOULD populate `${piece_methods}`:

1. User presses Enter on "Select Broker"
2. `process_key()` in `chtpm_parser_pal.c` sees `el->href` → calls `parse_chtm()`
3. `parse_chtm()` → `load_vars()` → `load_dynamic_methods(active_id)`
4. `load_dynamic_methods()` calls `resolve_dynamic_pdl_path(active_id)`
5. If PDL found → sets `${piece_methods}` → `${piece_methods}` renders in terminal

### Where it breaks (confirmed by instrumentation):

**A. `active_target_id` is never updated by `process_key()`**

The href handler in `process_key()` does:
```c
strncpy(current_layout, el->href, MAX_PATH-1);
// ... sets active_target_id in vars memory ...
// ... writes state.txt ...
cleanup_module();
parse_chtm();  // <-- load_vars() runs HERE
```

But `load_vars()` does:
```c
load_state_file("pieces/apps/player_app/state.txt", NULL);  // reads OLD state.txt
// ... THEN checks get_var("active_target_id") ...
```

We confirmed via debug output that `active_target_id` in `state.txt` is stale at the moment `load_dynamic_methods()` checks it. The in-memory `set_var()` call happens BEFORE `parse_chtm()`, but `load_vars()` calls `var_count = 0` which clears all vars — including the one we just set.

**B. The mychara_menu_input pattern doesn't translate directly**

`mychara_menu_input.c` uses `key == 0` as a "screen sync" signal that writes `state.txt` BEFORE the parser's main loop runs. But in yahoo-app, the PAL module (`broker_select_module.pal`) calls `yahoo_menu_input x9` (where x9=0) at the TOP of every loop iteration, not just on navigation. This means:

- On broker_select screen, the module calls `yahoo_menu_input 0` 
- That writes `state.txt` with `active_target_id=broker_select`
- BUT `load_dynamic_methods()` already ran during `parse_chtm()` BEFORE the module had a chance to write the correct value
- The parser's main loop checks `state_changed.txt` marker size, reloads vars, and re-parses — but `load_dynamic_methods()` only runs if `existing_methods == "[No Methods]"`, which it isn't (it has the stale value from the previous screen)

**C. The broker_select PDL path is never found**

Even if `active_target_id` were correct, `resolve_dynamic_pdl_path("broker_select")` checks:
1. `projects/yahoo-app/pieces/broker_select/piece.pdl` — doesn't exist
2. `projects/yahoo-app/pieces/broker_select/broker_select.pdl` — doesn't exist
3. `pieces/apps/yahoo-app/pieces/broker_select/piece.pdl` — doesn't exist
4. `pieces/apps/yahoo-app/pieces/broker_select/broker_select.pdl` — doesn't exist
5. `pieces/apps/playrm/broker_select/broker_select.pdl` — doesn't exist
6. `pieces/world/map_01/broker_select/broker_select.pdl` — doesn't exist
7. `pieces/broker_select/broker_select.pdl` — **this is where it SHOULD be**

But `button.sh` creates the session dir and symlinks `pieces/chtpm` and `pieces/registry` — it does NOT copy the dynamically-written `pieces/broker_select/` directory from the source tree. The `yahoo_compose_frame` op writes to `$BANK_SESSION/pieces/broker_select/broker_select.pdl`, but that directory is inside the session dir which is created fresh each launch. The `brokers.txt` IS present in `$BANK_SESSION/pieces/system/` (because `button.sh` creates it), but the `broker_select/` PDL directory is only created when `yahoo_compose_frame` runs — and it runs AFTER `load_dynamic_methods()` has already checked.

---

## Root cause summary

**Race condition between PDL generation and PDL consumption:**

1. Session starts, `button.sh` creates `/tmp/.yahoo-app-bank-<id>/`
2. `chtpm_parser_pal` starts, reads `current_layout.txt` → `bank.chtpm`
3. `load_dynamic_methods("bank")` → reads `pieces/bank/bank.pdl` ✅
4. Module loop starts `bank_module.pal`
5. User navigates to "Select Broker" (Enter key)
6. `process_key()` → `parse_chtm()` → `load_vars()` → `load_dynamic_methods("selector" or stale value)
7. `load_dynamic_methods()` fails because `active_target_id` wasn't persisted correctly
8. Terminal shows `[No Methods]` → only static "Back to Bank" button renders
9. `bank_module.pal` calls `yahoo_compose_frame` → writes `pieces/broker_select/broker_select.pdl`
10. PDL now exists BUT parser already rendered frame with empty `${piece_methods}`

The fundamental issue: **the PDL is generated by an op (`yahoo_compose_frame`) that runs INSIDE the module loop, but `load_dynamic_methods()` runs in the parser's main loop which doesn't know the PDL exists yet.**

---

## What a stronger model needs to do

### Option A: Move PDL generation OUT of the module loop (preferred)

Generate the `broker_select.pdl` **before** `load_dynamic_methods()` needs it:

1. In `process_key()`, when navigation to `broker_select.chtpm` is detected, call the equivalent of `yahoo_compose_frame` **synchronously** before `parse_chtm()`
2. OR: have `button.sh` pre-generate ALL dynamic PDLs at session startup (before launching orchestrator)
3. OR: make `load_dynamic_methods()` fall back to generating the PDL itself if it doesn't exist (call `system("yahoo_compose_frame")` or embed the logic)

### Option B: Break the circular dependency between `load_vars()` and module state

The core problem is `load_vars()` resets `var_count = 0` which wipes the `active_target_id` that the op just wrote. Solutions:

1. **Persist `active_target_id` to a separate file that `load_vars()` reads LAST** — create `pieces/apps/player_app/active_target.txt` that `load_vars()` reads AFTER `state.txt`, so it always wins
2. **Make `load_dynamic_methods()` idempotent and always-run** — remove the `existing_methods == "[No Methods]"` guard so it refreshes on every `state_changed` event
3. **Have the module write a "PDL ready" marker** — `yahoo_compose_frame` writes `pieces/display/pdl_ready.txt`; parser checks this before rendering and forces re-parse

### Option C: Use the broker_select PDL as a STATIC file (simplest)

Instead of dynamically generating `broker_select.pdl` from `brokers.txt`:

1. Create `@.apps/yahoo-app/pieces/broker_select/broker_select.pdl` as a **static checked-in file**
2. Populate it with the actual broker entries from `brokers.txt` at build time or on first run
3. No more race between generation and consumption

This is the approach `my-chara-txt` uses — all PDLs are static files in the source tree.

### Option D: Direct debug instrumentation (to confirm theory)

Before trying A/B/C, add minimal logging to confirm the race:

1. In `load_dynamic_methods()`: `fprintf(stderr, "load_dynamic_methods(%s) path=%s\n", active_id, path);`
2. In `process_key()` href handler: `fprintf(stderr, "navigating to %s active_id=%s\n", el->href, layout_name);`
3. In `yahoo_compose_frame`: `fprintf(stderr, "compose_frame wrote %s\n", pdl_path);`
4. In `compose_frame()`: `fprintf(stderr, "compose_frame active_id=%s methods=%s\n", active_id, methods_raw);`

Run with `stderr` redirected to a file and grep the sequence.

---

## Files currently modified (NOT upstream)

| File | Change |
|------|--------|
| `@.apps/yahoo-app/ops/yahoo_menu_input.c` | Added `key==0` screen-sync, `resolve_piece_pdl_path()`, `state.txt` write |
| `@.apps/yahoo-app/ops/broker_menu_input.c` | Same pattern as above |
| `@.apps/yahoo-app/pal/bank_module.pal` | Reordered compose before menu_input |
| `@.apps/yahoo-app/pal/broker_select_module.pal` | Reordered compose before menu_input |
| `@.apps/yahoo-app/pieces/system/brokers.txt` | Added 3 sample brokers |
| `@.apps/yahoo-app/ops/yahoo_compose_rgb_frame.c` | NEW — RGBA composer for GL mirror |
| `@.apps/yahoo-app/ops/yahoo_compose_frame.c` | Added `render_piece_pdl_brokers()` |
| `@.apps/yahoo-app/default_op.txt` | Added rgb composer |
| `@.apps/yahoo-app/scripts/build.sh` | Added rgb composer compile |
| `&.widgits/yahoo-broker/` | Mirrored all of the above |

**`system/chtpm_parser_pal.c` is UNMODIFIED** (reverted to upstream after your feedback).

---

## Comparison to working apps (my-chara-txt)

### How my-chara-txt does it (WORKS)

**PDL path resolution** (`mychara_menu_input.c:113`):
```c
snprintf(pdl_path, sizeof(pdl_path), "%s/projects/my-chara-txt/pieces/%s/piece.pdl", root, piece_id);
```

This reads from **`projects/my-chara-txt/pieces/<piece_id>/piece.pdl`** — static files checked into the source tree. They exist at session start, before any module runs.

**Module loop** (`main_module.pal`):
```
mychara_menu_input x9    ← reads static PDL, dispatches actions
mychara_compose_frame    ← renders view.txt → current_frame.txt
hit_frame
```

No dynamic PDL generation. No race. `${piece_methods}` just works.

### How yahoo-app tries to do it (BROKEN)

**PDL path resolution** (`yahoo_menu_input.c`):
```c
resolve_piece_pdl_path(current_layout, project_root, pdl_path, sizeof(pdl_path));
// Tries: pieces/<layout_name>/<layout_name>.pdl first
// Falls back to: pieces/apps/player_app/piece.pdl
```

The primary path `pieces/broker_select/broker_select.pdl` is **dynamically generated** by `yahoo_compose_frame` running inside the module loop. It doesn't exist when `load_dynamic_methods()` first checks.

**Module loop** (`broker_select_module.pal`):
```
yahoo_menu_input x9      ← tries to read PDL that doesn't exist yet
yahoo_compose_frame      ← generates PDL (too late!)
hit_frame
```

### The exact mismatch

| Aspect | my-chara-txt (works) | yahoo-app (broken) |
|--------|---------------------|-------------------|
| PDL location | `projects/<id>/pieces/<piece>/piece.pdl` | `pieces/<layout>/<layout>.pdl` |
| PDL timing | Static, exists at launch | Generated at runtime by module |
| PDL generation | None (checked-in files) | `yahoo_compose_frame` op |
| Race condition | None | Yes — consumer runs before producer |
| `${piece_methods}` population | Immediate | Never (PDL missing at parse time) |

### The one-line diagnosis

**yahoo-app is trying to use my-chara-txt's `${piece_methods}` dynamic-loading machinery with a PDL that doesn't exist at load time.** The working apps all have their PDLs as static source-tree files. yahoo-app's broker list is the ONLY dynamic PDL in the project, and it's generated by the same module loop that's waiting for it.

---

## Recommended next step

**Option C from above (static PDL)** is the fastest path to working, because it matches the proven pattern:

1. Create `@.apps/yahoo-app/pieces/broker_select/broker_select.pdl` as a static file
2. Populate it from `brokers.txt` at build time or session start (in `button.sh`, before launching orchestrator)
3. Remove the dynamic generation from `yahoo_compose_frame.c` (or keep it as a refresh mechanism that runs AFTER the PDL is already loaded)

This eliminates the race entirely. The parser finds the PDL, `${piece_methods}` populates, the broker list renders. Simple.

If you want me to implement Option C before you quit, it's a small change. Otherwise the handoff captures everything needed for the next session.
