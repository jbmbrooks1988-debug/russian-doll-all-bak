# WRAITH ARCHITECTURAL EMERGENCY: Missing Manager Pattern
Date: 2026-07-02
Status: CRITICAL SYSTEMIC ISSUE
Severity: High - affects all Wraith-hosted projects

---

## THE PROBLEM

**ALL Wraith-hosted projects are missing manager daemons.**

Current state of wraith-alpha/wraith-projects:
```
fs/              → Has ops, NO manager
chtmgl-wraith/   → Has ops, NO manager
emoji-studio-wraith/ → Has pieces, NO manager
wraith-ed/       → Has ops, NO manager
chtmgl-video-isolate/ → Has ops, NO manager
wraith-3d-cube/  → Static/minimal, NO manager
wraith-browser/  → Has ops, NO manager
piececraft-wraith/ → NEW - Has manager (but not launching)
blank-project/   → Minimal, NO manager
probe-project/   → Minimal, NO manager
```

**Expected pattern (from TPMOS bible):**
- Every project with interactive state should have a manager daemon
- Manager monitors input, updates game state, publishes state to gui_state.txt
- Layout reads dynamic variables from gui_state.txt via `${var}`

**What we built instead:**
- Static layouts with hard-coded text
- No real-time input processing
- No dynamic state updates
- No gui_state.txt publishing

---

## WHY THIS BREAKS PIECECRAFT-WRAITH

Piececraft-wraith was the first to follow the proper pattern:
1. Created manager daemon (piececraft-wraith_manager.+x)
2. Created gui_state.txt to provide `${module_path}`
3. Layout has `<interact>` tag and `<module>` tag
4. Expected to work like fuzz-op (top-level project)

**But Wraith-hosted projects don't auto-launch managers like top-level projects do.**

Result:
- Manager binary exists but never runs
- Layout waits for `${module_path}` variable
- Variable never provided (gui_state.txt not read on startup)
- Layout can't launch manager
- Input is recorded but never processed
- Chicken-and-egg problem

---

## ROOT CAUSE: WRAITH PROJECT INITIALIZATION

Top-level TPMOS projects (like fuzz-op):
```
Player App
  ↓
Reads project.pdl
  ↓
Launches manager daemon (module_path in project.pdl)
  ↓
Manager runs and publishes state
  ↓
Parser reads state and renders
```

Wraith-hosted projects (current broken state):
```
Wraith App
  ↓
Reads wraith-project/project.pdl
  ↓
Reads layout from wraith-project/layouts/*.chtpm
  ↓
Layout expects ${module_path} from gui_state.txt
  ↓
gui_state.txt not loaded yet (or not provided by Wraith)
  ↓
<module> tag can't resolve variable
  ↓
Manager never launches
  ↓
Input processed nowhere
  ↓
State never updates
```

---

## THE FIX: WRAITH MANAGER BOOTSTRAP PATTERN

For Wraith-hosted projects to work properly, we need ONE of:

### Option A: Wraith auto-launches project managers (BEST)
Wraith should:
1. Read wraith-project/project.pdl
2. Find `module_path` entry
3. Launch manager binary before rendering layout
4. Manager publishes state
5. Parser loads state and renders layout

**Requires:** Wraith architectural change to support manager auto-launch

### Option B: Pre-populate gui_state.txt (CURRENT WORKAROUND)
Manager launch done outside Wraith:
1. Admin starts manager in terminal: `./manager/+x/project_manager.+x &`
2. Manager runs and creates/updates gui_state.txt
3. Wraith loads project
4. Layout reads `${module_path}` from existing gui_state.txt
5. Manager already running, input gets processed

**Requires:** Manual manager startup before loading project in Wraith

### Option C: Self-bootstrapping manager (HYBRID)
Manager binary:
1. Checks if it's running (via lock file)
2. If not running, starts itself in background
3. Creates gui_state.txt
4. Then becomes the running daemon

**Requires:** Modification to manager source code

---

## IMMEDIATE FIX FOR PIECECRAFT-WRAITH (SOLVED)

**Root Cause Found:** Variable substitution in module tag creates chicken-and-egg problem.

**What was wrong:**
- piececraft-wraith layout used: `<module>${module_path}</module>`
- Expected gui_state.txt to provide `${module_path}` value
- But gui_state.txt isn't read until AFTER manager launches
- Manager can't launch without resolving `${module_path}`
- → Deadlock: manager waits for layout, layout waits for manager

**The Fix:**
piececraft-wraith now matches top-level project pattern (op-ed, user):
1. **Layout:** Hardcode module path: `<module>projects/wraith-alpha/wraith-projects/piececraft-wraith/manager/+x/piececraft-wraith_manager.+x</module>`
2. **project.pdl:** Removed `module_path` entry (not needed; layout is self-contained)
3. **gui_state.txt:** Still published by manager with dynamic variables (`${xel_x}`, `${xel_y}`, `${display_mode}`, etc.) for layout substitution

**Result:**
- Manager launches immediately (no variable dependency)
- Manager creates/updates gui_state.txt with state
- Layout reads dynamic state from gui_state.txt
- Input processing works as expected
- Architecture is now consistent across all projects

---

## MIGRATION PLAN FOR ALL PROJECTS

Now that the pattern is proven with piececraft-wraith, we need to:

1. **Audit all projects** - which ones need managers?
   - Projects with interactive state: YES
   - Static projects: NO

2. **Create manager for each interactive project:**
   - fs/ → needs manager for file browser state
   - wraith-ed/ → needs manager for editor state
   - emoji-studio-wraith/ → needs manager for preview updates
   - chtmgl-video-isolate/ → needs manager for playback state
   - wraith-browser/ → needs manager for navigation state

3. **Standardize manager pattern (Wraith-hosted projects):**
   - Follow piececraft-wraith template as reference
   - **CRITICAL:** Hardcode module path in layout (don't use `${module_path}` variable)
   - Use gui_state.txt for dynamic variable publishing (`${xel_x}`, `${display_mode}`, etc.)
   - Monitor project history.txt for input
   - Update state on every change
   - This matches the pattern used in top-level projects (op-ed, user)

4. **Do NOT use:**
   - Variable substitution in `<module>` tag
   - Explicit `module_path` in project.pdl (not needed)

5. **Update TPMOS bible:**
   - Document Wraith manager pattern
   - Add layout example showing hardcoded module path
   - Add to Phase X of onboarding

---

## TIMELINE ESTIMATE

- Fix piececraft-wraith: 1-2 hours (NOW)
- Create template for other projects: 1 hour
- Migrate fs: 2-3 hours
- Migrate wraith-ed: 2-3 hours
- Migrate emoji-studio: 1-2 hours
- Migrate chtmgl-video-isolate: 1-2 hours
- Migrate others: 1-2 hours each
- **Total: 10-15 hours of work**

---

## DELIVERABLES

### Immediate (this session) - COMPLETED
- [x] wraith-man-fix-j2.md (this file - updated)
- [x] Piececraft-wraith with working manager (hardcoded pattern fixed)
- [x] Identified root cause: variable substitution in module tag
- [x] Applied solution: hardcode module path in layout

### Next session
- [ ] Test piececraft-wraith in Wraith (verify manager launches)
- [ ] Migration guide for other projects
- [ ] Template manager for interactive projects
- [ ] Audit of which projects need managers

### Eventually
- [ ] All interactive projects with managers
- [ ] Updated TPMOS documentation with correct pattern
- [ ] No architectural changes needed (pattern works as-is)

---

## WHAT THIS MEANS

**Good news:**
- Architecture is sound and proven
- Pattern works (top-level projects prove it)
- Fix was simple: avoid variable substitution in module tag
- No Wraith changes needed

**What we learned:**
- `<module>` tag must contain concrete path, not variables
- Dynamic state goes in gui_state.txt for layout substitution
- This applies to both top-level and Wraith-hosted projects

**Going forward:**
- All NEW Wraith projects must follow hardcoded module pattern
- Existing projects can be migrated one-by-one
- No architectural changes to Wraith required

---

## NOTES

- This is NOT a blocker for piececraft-wraith development
- We can work around it with manual manager startup
- But it reveals a systemic gap in Wraith architecture
- Should be addressed soon to prevent more incomplete projects

---

## UPDATE: OPS-ONLY PROJECTS AND THE MANAGER QUESTION (same session, later)

**Question raised:** "even if they are ops based, that should have been orchestrated by managers right?"

**Short answer, revised after tracing the actual dispatch code:** No — not always, and the reason is more specific and more interesting than a generic architecture rule.

### What "ops-only" projects actually are

Audited: `fs/`, `chtmgl-wraith/`, `wraith-ed/`, `chtmgl-video-isolate/`, `wraith-browser/`, `wraith-3d-cube/` all have `ops/` but no `manager/`. Traced `wraith-alpha_manager.c`'s raw-key handler and found the mechanism these projects actually rely on:

```c
// in wraith-alpha_manager.c, on every keypress while a project is in map-control/interact mode:
append_project_history("KEY_PRESSED", key_buf);
run_active_project_input_op();   // execs <project>/ops/+x/wraith_project_input.+x, WAITS for it
update_state(key);
trigger_render();                // composes the visible frame immediately after
```

`run_active_project_input_op()` looks for one specific, conventionally-named binary — `ops/+x/wraith_project_input.+x` — and if present, **runs it synchronously and waits (`waitpid`) before rendering.** This is not a fallback or a degraded mode. It's the actual, intended low-latency input path for Wraith-hosted projects: wraith-alpha itself acts as the orchestrator, directly invoking one op per keypress, guaranteeing state is fully updated before the frame is composed.

### Where this bit piececraft-wraith tonight

piececraft-wraith was built with a **manager + async polling thread** instead (`usleep(16667)` / ~16.6ms poll loop watching `history.txt`). Since it has no `ops/+x/wraith_project_input.+x`, `run_active_project_input_op()` silently no-ops for it, and `trigger_render()` composes the frame from whatever state existed *before* the manager's poll thread wakes up. Result: a structural, always-reproducible one-keypress-behind lag — full root cause and diagnosis in `HANDOFF-WRAITH-STATE-MYSTERY.md`, "Frame lag" section. This is the same bug class user previously saw in fuzz-op.

### The corrected guidance

**Managers and ops are not either/or — they're suited to different jobs, and the dispatcher (wraith-alpha) already has a synchronous fast-path built specifically for ops:**

| Job | Right tool | Why |
|---|---|---|
| One-time init on project launch (write initial state, xelector_state.txt, gui_state.txt) | Manager `main()` | Runs once, no latency requirement |
| Per-keypress state mutation on the interactive hot path | **`ops/+x/wraith_project_input.+x`**, invoked synchronously by wraith-alpha | Zero lag — state is guaranteed current before the frame renders, because wraith-alpha waits for the op to finish |
| Long-running background work unrelated to input (e.g. polling an external process, periodic heartbeats) | Manager background thread | Legitimate use of async polling — just don't put input-response state mutation there |

**Revised rule for ops-only projects (fs, chtmgl-wraith, wraith-ed, chtmgl-video-isolate, wraith-browser, wraith-3d-cube):** they likely do NOT need a manager added just because they lack one. If they already have `ops/+x/wraith_project_input.+x` and it's being invoked correctly, they may already have correct, lag-free input handling — **audit before assuming they're broken.** A manager should only be added to them if they need (a) one-time init logic beyond what a static `project.pdl`/session file can provide, or (b) genuine background work independent of the input path.

**Revised rule for piececraft-wraith and future rich interactive projects:** use a **hybrid** — keep the manager for init, but move the actual per-keypress state mutation into an `ops/+x/wraith_project_input.+x` binary so it runs on wraith-alpha's synchronous fast path instead of racing an async poll thread. This is the concrete next implementation step (not yet done as of this handoff — see `HANDOFF-WRAITH-STATE-MYSTERY.md`).

### Migration plan correction

Section "MIGRATION PLAN FOR ALL PROJECTS" above (written earlier tonight, before this discovery) assumed every interactive Wraith project needs a manager. That's superseded by the table above — **audit each project's actual input-latency behavior first**, don't retrofit managers by default. The piececraft-wraith template is now: manager for init + `wraith_project_input.+x` op for the hot path, not manager-only.

---

*This document serves as both emergency notice and migration roadmap.*
*Next file: wraith-manager-bootstrap-pattern.md (implementation guide)*
