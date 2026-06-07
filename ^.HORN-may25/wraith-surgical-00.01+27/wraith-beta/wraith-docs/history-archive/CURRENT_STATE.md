# Current State

This file is the single source of truth for the workspace history and the
current Wraith direction.

## What Wraith is

Wraith is an ASCII-first desktop/window manager. It is frame-first and
metadata-first. The visible product should be a real desktop surface, not a
flattened markup dump.

The durable model is:

- desktop orchestration owned by Wraith
- windows as map pieces with `x`, `y`, `z`, focus, drag, and resize state
- projected file-backed window content
- deterministic frame composition before any RGB or GL presentation

## What was corrected

Earlier session notes kept repeating false or premature assumptions. The main
corrections were:

- the visible shell should be a GL-OS-style desktop composition, not a panel
  demo
- the sketch is only a composition reference, not the long-term content model
- hard-coded menu rows are not the real subwindow abstraction
- mouse drag and true window movement were not yet implemented just because
  state files existed
- numeric nav and selection movement were only partially proven and still
  needed proper routing
- the current Wraith bridge still depended on legacy TPMOS theater behavior
  rather than being a complete Wraith-owned window system

## Historical arc

### J2

- Wraith was defined as an ASCII-first, frame-first, metadata-first desktop.
- The project was steered away from tag-flattened markup thinking.
- The intended runtime path became: layout/state -> parser -> frame bundle ->
  optional RGB -> optional GL.

### J3 and J4

- The intended visible shell was clarified as a large outer desktop shell with
  a left launcher list, a `Wraith Terminal v1.0` shell, an overlapping `GAME
  MAP` window, and footer/taskbar semantics.
- Wraith was treated as a sovereign map composer with projected window
  surfaces.
- GL-OS was kept as the behavior reference for focus, drag, resize, z-order,
  and window lifecycle.
- `fuzz-op` style projected surfaces were kept as the content reference for
  what visible subwindows should be.

## Current truth to preserve

- Keep visible content file-backed and auditable.
- Keep geometry and focus in explicit state files such as
  `session/windows_state.pdl`.
- Keep the desktop frame as the product, not the parser text.
- Keep the outer shell and overlapping windows composition if Wraith is being
  rebuilt.
- Do not reintroduce tag-flattened or panel-demo interpretations.

## Removed note trees

The old note/session trees were collapsed into this file so future work starts
from one place:

- `_codex_dox/`
- `_codex_archives/`
- `previous-sessions/`
- `_codex-tpmos/reports/` if/when it is no longer needed

## Working rule

If a new decision matters, append it here and delete the old competing note
instead of scattering versions across multiple folders.
