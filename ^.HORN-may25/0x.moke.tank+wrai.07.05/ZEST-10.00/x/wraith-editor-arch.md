# wraith-editor-arch.md — what wrai-text-editor is actually trying to be

Date: 2026-07-11
Purpose: high-level statement of intent for wrai-text-editor, independent
of whether `legacy-vs-wraith.txt`'s refactor question gets a yes or no.
Whoever picks this up next (possibly from the x/ZEST-09.00+8TWIN/
snapshot) should be able to read this alone and know what "done" looks
like, without needing the blow-by-blow implementation history.

---

## The one-sentence goal

**wrai-text-editor should look and behave exactly like agy-text-editor,
except it opens inside a Wraith desktop window (launcher row, taskbar,
focus/close like any other Wraith project) instead of taking over the
whole screen.**

## What agy-text-editor gets right (the reference, not up for debate)

- A box-drawn frame: `╔═══... EDITOR ...═══╗`.
- One `INTERACT` button that enters "type into the document" mode.
- A scrollable document viewport inside the frame, cursor visible as
  `[X]` at the current position.
- A row of real, numbered, nav-indexed action buttons below the
  viewport: `NEW FILE`, `CLEAR FILE`, `FILE MENU`, `EXIT TO OS`.
- All of this driven by ~40 lines of manager code that compute one
  string (`editor_map`) and write it to `gui_state.txt` — no hand-drawn
  box-drawing duplicated anywhere else, no parallel copy of the UI
  structure living in two places.

## What "a Wraith version of agy" means concretely

Take that exact experience and make it live inside a Wraith window:

1. It appears in the Wraith desktop's launcher row / taskbar, like
   `piececraft-wraith`, `settings`, `web-cam` already do.
2. Opening it shows the same box frame, INTERACT button, document
   viewport, and nav buttons agy shows — not a plain-text dump, not a
   differently-shaped UI improvised because "that's what the embedded
   window system currently supports."
3. Scrolling/editing/nav buttons work the same way they do in agy:
   pressing a button or key updates the document view live, staying
   inside the same window (no full-screen takeover required for basic
   use).
4. It can be minimized, closed, refocused — normal Wraith window
   lifecycle — the same as `piececraft-wraith` or `terminal` already
   support.

## Why this turned out to be non-trivial (one paragraph, see legacy-vs-wraith.txt for the full story)

agy gets its look "for free" because a generic parser genuinely parses
its layout file and substitutes values into it. Wraith's embedded-window
system, as it currently exists, does not do that for any project — every
embedded project that wants real buttons/framing has had to hand-write
that structure directly in C, duplicating what the layout file already
says. That duplication is the actual reason wrai-text-editor's first
working version looked "stupid" compared to agy: it was accurate to what
the ops code hand-wrote, but that hand-written code didn't match agy's
real structure closely enough, and maintaining two copies of the same UI
(one in the unused `.chtpm` layout, one in the ops's `fprintf()` calls)
is exactly the kind of drift this project should avoid going forward,
refactor or not.

## Definition of done (refactor-independent)

Regardless of whether the embedded-rendering gap described in
`legacy-vs-wraith.txt` ever gets fixed at the shared `wraith-alpha_manager.c`
level, wrai-text-editor is "done" when:

- Opening it from the Wraith launcher shows agy's exact visual structure
  (box frame, INTERACT button, document viewport, nav button row) —
  whether that structure is produced by a fixed shared renderer or by
  wrai-text-editor's own ops hand-matching it line-for-line is an
  implementation detail, not a requirement.
- Scroll/nav actually changes what's visible in the document viewport,
  verified live (not just "the ops writes the right file when tested
  standalone" — confirmed inside an actual running Wraith session).
- No other previously-working Wraith project regresses as a side effect
  — this is the hard constraint the previous attempt violated
  (piececraft-wraith stopped opening correctly). Any future attempt
  needs to verify this live, per project, before considering the work
  finished, not just reason about it from reading the code.
- The emoji-rendering work from earlier in this effort (FreeType + Noto
  Color Emoji glyphs, LRU cache, UTF-8 decode, `wraith_rgb_daemon.c`
  integration) is unaffected by any of this — that layer is below and
  independent of the layout-rendering question this document and
  `legacy-vs-wraith.txt` are about, and was working and tested
  separately before this regression.

## What comes after "done" (the original three-phase plan, not yet started)

wrai-text-editor/emoji rendering was never the end goal by itself — it
was Phase 1 of a three-phase plan, and the later phases are the actual
reason emoji rendering was prioritized first:

- **Phase 1 — emoji rendering (this effort, described above).**
- **Phase 2 — right-click context menus for window-specific actions.**
  Each Wraith window (wrai-text-editor included, but not exclusively —
  this is a general desktop-shell feature) should support a right-click
  context menu exposing actions scoped to that specific window/project,
  rather than only the fixed chrome-icon row (minimize/maximize/close)
  every window currently shares.
- **Phase 3 — window-geometry verification and resize, plus
  overlapping-window focus handling.** This covers actually resizing a
  window (not just the existing fixed `project.pdl` `WINDOW.width`
  precedence system), and correctly handling focus/z-order when windows
  overlap on screen.

**The stated reason Phase 1 (emoji) came before Phase 3 (window-geom):
Phase 2's context menus were intended to double as the manual testing
tool for Phase 3.** Rather than building window-geometry/resize
verification against raw keybindings or a headless test harness first,
the plan was to get right-click context menus working, then use a
context-menu-triggered "resize"/"geometry" action as the actual live,
interactive way to drive and verify window-geom changes — i.e. Phase 2
is infrastructure Phase 3 depends on for testability, not just a nice
UI addition alongside it.

None of Phase 2 or Phase 3 has been started. They are not part of
wrai-text-editor's own "definition of done" above, but they are the
reason wrai-text-editor (and the emoji work generally) was being pushed
toward completion first — closing out Phase 1 cleanly is what unblocks
starting Phase 2.
