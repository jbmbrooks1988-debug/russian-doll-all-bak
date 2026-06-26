# Wraith Architecture J25

## Purpose

This document captures the intended Wraith model for TPMOS development in J25:

- Wraith is a virtual OS, not a single game.
- Session state is sovereign and file-backed.
- Save/load applies to the entire Wraith environment.
- `terminal`, `blank-project`, and the desktop shell are all real contexts.
- Reusable Ops are preferred wherever a lifecycle action already exists or can be shared.

This is a reference document, not an implementation commit log.

## Design Precedent

`op-ed` is the closest existing precedent:

- It treats each game as a sovereign folder-context.
- Save/load copies whole folders, not isolated UI widgets.
- The browser and the working project are distinct views over a real project tree.

That is the right mental model for Wraith too, except the folder-context is the whole session environment.

## Core Model

Wraith is split into three layers:

### 1. Piece

Pieces own their own files:

- `piece.pdl`
- `state.txt`
- any project-owned history or memory files

### 2. Module

Modules orchestrate pieces:

- launch work
- route input
- publish state projections
- invoke Ops for real work

Modules should stay thin. They should not become one-off business logic dumps.

### 3. OS / Theater

The OS layer renders the current truth:

- current layout
- active focus
- current frame
- session-visible debug/projection data

The theater should compose and route, not invent hidden state.

## Wraith Session Scope

The Wraith session is the unit that should be saved and loaded.

That means the restore boundary includes:

- desktop shell state
- terminal windows
- focused project
- active layout
- registry and window stack
- session history
- any project-owned session files needed to recreate the same environment

This is larger than a terminal buffer and larger than a single project.

## Real Contexts

### Terminal

The terminal is a real context.

- It should have persistent state.
- It should be launchable more than once.
- It should be restorable as part of the session.
- It should participate in the session model, not sit outside it.
- It should resolve to a real project directory under `wraith/wraith-projects/terminal`.
- This applies to every terminal entry point, including the default bootstrap/taskbar terminal and any later terminal windows opened from inside Wraith.
- The toolbar terminal is not a fake shell shortcut; it is a real project-backed launcher entry that should resolve through the same project contract as other Wraith contexts.

### Blank Project

`blank-project` is also a real context.

- "Blank" means empty or minimal, not fake.
- It should have its own project directory under `wraith/wraith-projects/blank-project`, modeled like a template-style project.
- The launcher alias can still be `blank-proj`, but it should resolve to that project directory and open a `Blank Project` window in the current Wraith toolbar/taskbar.
- It must not open eagerly or automatically during bootstrap. It should open only when the user explicitly selects or activates the `blank-proj` launcher row.
- Every project opened inside Wraith uses the same standard desktop window chrome and controls: half-size, minimize, close, and taskbar participation.
- The project ID must preserve the full nested path, not collapse to the first path segment.
- The project should live inside the same Wraith desktop session, not as a separate OS/session.
- The GL presenter is an IO mirror for the single Wraith desktop session. `blank-project` must not spawn a second GL window.
- Launcher dispatch should treat the launcher row order consistently with the manifest. The current fix path is to use a one-based walk over the launcher PDL so `blank-proj` can resolve to `DESKTOP_ACTION:launch_blank_project` instead of falling off the end of the list.

### Desktop Shell

The desktop shell is the top-level Wraith frame.

- It is part of the session.
- It owns shell-level context and window projection.
- It should be restorable exactly like the other contexts.
- Its bottom toolbar should list every non-closed window in the current session, not only minimized windows.
- Shell launch controls should be authored as real CHTPM buttons in the layout, not injected as markup-like text through manager state.
- Manager output should carry session truth and projection values, not fake UI fragments that require a second parse pass.
- Launcher Enter dispatch should resolve the selected launcher method from `projects/wraith-alpha/wraith-projects/launcher/piece.pdl`, so `blank-proj` and `terminal` share the same reusable launch contract.
- The parser-to-manager handoff for launcher focus should use `pieces/display/active_gui_index.txt` as the live selection seam, so Enter dispatch reads the same row the layout just exported.
- Launcher selection is not the same thing as window-chrome count; the manager must preserve launcher-range indices instead of clamping them to `1` when the desktop shell is focused.
- The shell should render its buttons from runtime state: launcher rows come from the launcher PDL and taskbar rows come from the live non-closed window registry.
- Static menu slot math is a bug, not a feature. The desktop shell should derive interactive indices from the current manifest and registry size every frame.
- When the shell needs to inject a dynamic button region, that region must be expanded before tokenization so the parser sees real tags instead of literal markup text.
- The desktop shell must still emit the full chrome frame even when no active window is focused; a missing active window should degrade to an empty desktop body, not a one-line taskbar.
- When no desktop window is focused, the shell should degrade to the minimal taskbar-only bootstrap view shown in the reference frames. The full chrome appears only after a real window is active.

## Save / Load Contract

Save/load is for the entire Wraith environment.

The contract should be:

- Save writes the whole session to a folder or session slot.
- Load restores the whole session from that folder or slot.
- Restore should be deterministic and round-trip safe.
- The restored session should match the saved session as closely as practical.
- Window actions such as `terminal` and `blank-proj` remain inside the same session and should update the shared window registry rather than creating a new OS instance.
- `blank-proj` should resolve to a real project directory, not a hardcoded placeholder window.
- The bottom toolbar is a live session list. Open windows should appear there as well, so `terminal` and `blank-project` remain visible while they are open.
- Opening `blank-project` must stay inside the existing Wraith desktop shell and window registry. It is not a shell-to-project layout swap and it is not a separate GL session.
- The launcher row should reuse existing parser button semantics and `DESKTOP_ACTION:` routing rather than inventing a custom recursive-markup parser path.
- The shell should build its own index order from the current launcher PDL plus the live taskbar registry so the visible row numbers are never hardcoded.

This is the same folder-level idea as `op-ed`, applied to an OS session instead of a game world.

## Login / Auth Extension

The save/load boundary can later become login/auth.

The path is straightforward:

- Save slot selection becomes session selection.
- Restore can require a password before rehydration.
- A saved session can become a user identity + session bundle.

That should be an extension of the same save/load model, not a separate system invented from scratch.

## Reusable Ops

Reusable Ops are the default pattern.

Rules:

- Managers orchestrate.
- Ops do the reusable work.
- If a lifecycle action already exists as an Op, reuse it.
- If a new action belongs to more than one flow, make it an Op before embedding logic in the manager.

Good candidates for reuse:

- save session
- load session
- restore session
- project launch
- terminal launch
- auth-gated restore

This keeps the Wraith architecture modular and prevents manager bloat.

## State Ownership

The source of truth should be explicit.

Suggested ownership split:

- Manager-owned: registry, focus, current session selection, launch/restore bookkeeping
- Parser-owned: frame composition, layout projection, visible session output
- Renderer-owned: final display only
- Project-owned: project-specific state and history

If a file is the truth, only one subsystem should own it.

## Input and History

Input provenance should be durable.

Current expectation:

- keyboard and mouse events should be written into the appropriate session history files
- session history should survive enough to support replay, audit, and debugging
- the presenter/renderer path should not silently swallow input if that input is supposed to matter later

If the GL presenter is display-only, then the manager or session host must be responsible for event logging.

## Debug Output

Debug output must not dominate the actual frame contract.

Rules:

- debug text can exist, but it should be intentional
- frame truth should not be buried under redundant diagnostics
- if the shell becomes noisy, the layout contract should be tightened rather than adding more logging

## Practical Interpretation

For J25, the Wraith direction is:

- build a real session environment
- make `terminal` and `blank-project` real project-backed window actions
- preserve whole-session save/load semantics
- reuse Ops instead of hardcoding bespoke flows
- keep manager code thin
- keep the frame contract explicit
- on a fresh Wraith Alpha launch, bootstrap a clean session registry instead of inheriting stale terminal instances from the previous run
- treat `projects/wraith-alpha/manager/gui_state.txt` and the alpha session files as startup state, not durable cross-run truth, when bootstrapping a fresh desktop

## Related Files

- [wrai-tasks-j25.txt](./wrai-tasks-j25.txt)
- [projects/op-ed/README.md](./1.TPMOS_c_+rmmp.0102.0026/projects/op-ed/README.md)
- [projects/op-ed/manager/op-ed_manager.c](./1.TPMOS_c_+rmmp.0102.0026/projects/op-ed/manager/op-ed_manager.c)
- [projects/wraith-alpha/manager/wraith-alpha_manager.c](./1.TPMOS_c_+rmmp.0102.0026/projects/wraith-alpha/manager/wraith-alpha_manager.c)
- [projects/wraith-alpha/ops/wraith_parser_alpha.c](./1.TPMOS_c_+rmmp.0102.0026/projects/wraith-alpha/ops/wraith_parser_alpha.c)
