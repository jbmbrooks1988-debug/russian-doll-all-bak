# Handoff: Settings → Window Geometry embedded navigation

Written for a fresh agent with zero conversation history. Read this before touching
`settings_manager.c` or `window-geom_manager.c` again — the last two sessions built,
then partly reverted, two competing implementations of the same feature, and the
current state has two known, diagnosed-but-unfixed bugs.

## The feature

Wraith desktop windows are meant to work like browser tabs: clicking a link inside a
Window navigates that Window's content in place — the desktop, taskbar, and other
open windows stay untouched. This is being built for the "settings" project, whose
one entry so far is "Window Geometry" (view/edit the x/y/width/height a wraith
sub-project's Window opens at).

Intended navigation, three steps, all *inside* the same open SETTINGS window:

1. **Settings menu** — "choose an option:" → `[Window Geometry]` button
2. **Project picker** — "choose a project to edit:" → list of every wraith
   sub-project (terminal, screen-record, settings, window-geom itself, etc. — *all*
   of them, no filtering, confirmed with the user)
3. **Geometry editor** — cli_io fields + [-]/[+] buttons for x/y/width/height of
   whichever project was picked in step 2, plus "Back to Project List"

Separately: opening Window Geometry from its own chrome button (not through
Settings) should open it as its own standalone `Window`, scoped to whichever
project window is currently focused, skipping the picker entirely. **That standalone
path is out of scope for this handoff** — nobody has touched it recently and it's
not known to be broken. Everything below is about the *embedded* (inside-Settings)
path.

## Where the logic lives (current, working design)

Earlier in this work there were briefly *two* competing implementations — one built
directly into `settings_manager.c`, another built into `window-geom_manager.c` using
a different command name (`WINDOW_GEOM_SELECT:`). The `window-geom_manager.c` one
was reverted. **The live implementation is entirely inside
`settings_manager.c`**. `window-geom_manager.c` is NOT involved in the embedded
path at all — it only matters for the standalone/chrome-button path (untouched,
see above).

Files:

- `projects/wraith-alpha/wraith-projects/settings/manager/settings_manager.c`
  — owns all three embedded pages (menu / picker / editor). Hot-path manager
  (`while(1)` loop, not init-only).
- `projects/wraith-alpha/manager/wraith-alpha_manager.c` — the desktop shell.
  `route_command()`'s `SETTINGS_PAGE:` branch (~line 3689) is the only place that
  ever writes to settings' page-state marker.
- `projects/wraith-alpha/wraith-projects/settings/window-geom/manager/window-geom_manager.c`
  — standalone-mode only, not used by the embedded flow. Leave alone unless
  you're specifically working the standalone/chrome-button path.

### The command scheme

Everything routes through one command prefix, `SETTINGS_PAGE:<page>`, handled in
`wraith-alpha_manager.c`'s `route_command()`:

```c
if (strncmp(cmd, "SETTINGS_PAGE:", 14) == 0) {
    // appends cmd+14 (the page name) as a new line to:
    // projects/wraith-alpha/wraith-projects/settings/session/state_changed.txt
    // then trigger_render()
}
```

`<page>` is one of://
- `settings` — go to the top menu
- `window-geom` — go to the project picker
- `window-geom:<project_id>` — go to the editor for that project (e.g.
  `window-geom:wraith-alpha/wraith-projects/terminal`)

`settings_manager.c`'s hot-path loop watches
`session/state_changed.txt` for **size growth** (the standard marker-file
convention used everywhere in this codebase — never `usleep`-poll without a size
check, and never watch mtime). On growth, it reads the **last line** of that file
as "active_page" and calls `write_wraith_body()`, which switches on that string
(`== "settings"` / `== "window-geom"` / `strncmp(..., "window-geom:", 12) == 0`)
and writes the corresponding page's markup to `session/wraith_body.txt`.

`wraith-alpha_manager.c` reads `session/wraith_body.txt` verbatim (any line
starting with `<` passes through as raw markup) and splices it into the focused
Window's body via `append_project_probe_body()`. This is the same generic
mechanism the launcher-row/embedded-Window path already used for the settings
menu itself (see the `2fix.txt, 2026-07-05` comment block in
`settings_manager.c` above `write_wraith_body()` for the history of *why* this
project needs its own `session/wraith_body.txt` at all, separate from
`manager/state.txt`'s `${var}`-substitution path).

The project picker is built by `discover_all_wraith_projects()` /
`discover_wraith_projects_recursive()` in `settings_manager.c` (~line 178-232),
which recursively walks `projects/wraith-alpha/wraith-projects/` for any
directory containing a `project.pdl` — at any nesting depth, deliberately
including hub/sub projects like `settings` and `window-geom` themselves. This
mirrors `compile_all.sh`'s own recursive `compile_wraith_project_tree()` (added
this session, see below) — the two should stay conceptually in sync about what
counts as "a project."

## Fixed this session

1. **`compile_all.sh` didn't compile nested projects.** `window-geom_manager.c`
   lives one level deeper than `wraith-projects/*/manager/` (it's under
   `wraith-projects/settings/window-geom/manager/`), and the old compile loop
   only went one level deep, so it silently never got compiled/updated. Fixed
   with a recursive `compile_wraith_project_tree()` function in
   `#.dev-storage/#.tools/compile_all.sh` (section "4c. NESTED WRAITH
   PROJECTS"). Verify with `bash "#.dev-storage/#.tools/compile_all.sh"` — should
   report 224 compiled, 0 failed, and specifically show
   `.../settings/window-geom/manager/+x/window-geom_manager.+x`.

2. **`ensure_project_manager()` never relaunched a dead manager.** In
   `wraith-alpha_manager.c` (~line 1072), this function only checked whether a
   `session/.manager.lock` file *existed*, never whether the pid inside it was
   still alive. Once written, a stale lock blocked relaunching forever, so a
   manager that crashed or got killed (e.g. by `kill_all.sh` between test runs)
   silently never came back. Fixed: now reads the pid from the lock and checks
   `kill(pid, 0)`/`errno == EPERM` before trusting it; `launch_project_manager()`
   now returns the actual child pid so the lock stores the right value (it was
   previously storing `getpid()` — the *parent* wraith-alpha_manager's own pid,
   which is always "alive," making the bug worse). Needed `#include <errno.h>`
   added.

3. **Embedded-page transitions didn't reliably redraw.** `settings_manager.c`
   was only bumping `pieces/display/frame_changed.txt`, which tells the
   *renderer* "redraw now" but does NOT make `wraith-alpha_manager.c` re-run
   `update_state()` — the function that actually re-embeds fresh
   `wraith_body.txt` content into the desktop markup via
   `append_project_probe_body()`. Every other `route_command` branch
   (`KEY:`, `PROJECT_ACTION:`, etc.) calls `update_state(0)` right alongside
   `trigger_render()`; `SETTINGS_PAGE:` was the one branch that didn't, so the
   screen only caught up whenever some *unrelated* event (a mouse move, etc.)
   happened to trigger `update_state()` afterward — a race, not a fix.

   The actual fix used the **same generic marker-file mechanism already used
   elsewhere** (per explicit user direction — don't invent a new mechanism when
   one already exists): `wraith-alpha_manager.c` already has
   `process_active_project_marker()` (~line 3260), called every tick (~60Hz)
   from `main()`, which watches `session/fs_watch.marker` **in whichever
   project is the currently-focused window** and calls
   `update_state(0); trigger_render();` on growth. `settings_manager.c`'s
   `trigger_render()` (and `window-geom_manager.c`'s, for consistency in the
   standalone path) now also bumps its own `session/fs_watch.marker`, plugging
   directly into that existing loop instead of relying on an unrelated later
   event.

   This part is confirmed working — see the "why no project picker" bug below,
   which is a *different*, still-open bug that was found while re-testing this fix.

## Two open bugs (not yet fixed)

### Bug A: opening Settings skips straight to the last-viewed editor page

**Symptom:** After using the picker once to reach the editor for some project,
closing and reopening the SETTINGS window (or even just the manager restarting)
goes straight back to that editor page — skipping the top menu *and* the picker
entirely, as if the whole "choose an option" / "choose a project" sequence never
happened.

**Root cause, confirmed via logs:** `session/state_changed.txt` is an
**append-only, never-truncated** file (correct, per the marker-file convention —
growth is what the hot-path loop watches). But `write_wraith_body()` also uses
this *same* file's **last line** as the authoritative "what page am I on" state,
read fresh every time it runs — including at manager startup. Since the file is
never reset, a brand new `settings_manager` process (or a freshly-opened Settings
window reusing an already-running one) reads whatever the *previous test
session* left as the last line and renders that page immediately.

Confirmed by inspection: after one test session that navigated menu → picker →
editor(chtmgl-video-isolate), `session/state_changed.txt` contained:

```
settings
window-geom
window-geom
window-geom
window-geom
window-geom
window-geom:wraith-alpha/wraith-projects/chtmgl-video-isolate
window-geom:wraith-alpha/wraith-projects/chtmgl-video-isolate
```

Every subsequent fresh open of Settings — even across full manager restarts —
rendered the editor page for `chtmgl-video-isolate` first, because that's still
the last line.

**This is a real design conflict, not a typo:** the file is being used as both
(a) a growth-triggered event log (fine, that's the marker-file convention) and
(b) a durable "current page" pointer read via last-line (wrong — this makes a
brand-new window session permanently remember wherever a previous session left
off, instead of always starting fresh at the menu).

**Fix path (not yet implemented):** the marker file itself should stay
append-only/growth-triggered (don't change that part — it's correct and matches
the rest of the codebase). What needs to change is *when* the "current page"
gets reset to `settings`. The natural point is **window open time**, not manager
start time — because the manager process can legitimately outlive a window
close/reopen cycle (it's a hot-path daemon; killing/reopening the *window* doesn't
necessarily kill the *manager*). Look at `launch_window_instance()` in
`wraith-alpha_manager.c` (~line 1107) — this already runs every time any wraith
sub-project's Window is opened/reopened (confirmed: repeated opens create
`SETTINGS #2`, `SETTINGS #3`, etc., so this function reliably re-runs each time,
it doesn't dedupe). Adding an unconditional
`fprintf(f, "settings\n")` append to settings' `state_changed.txt` here — gated on
`project_id` matching settings' id, or more generally on *any* project that owns
this kind of embedded sub-page state, if this pattern gets reused — would reset
the page to the top menu on every fresh window open, while still leaving the
growth-triggered marker semantics untouched for everything that happens *after*
the window is open.

Whoever picks this up should also sanity-check: does this need to be
settings-specific, or should it become a generic hook (e.g. any project whose
`project.pdl` declares itself as having "resettable page state" gets this
treatment automatically)? Given only one project (`settings`) currently has this
pattern, a hardcoded check is probably fine for now — don't over-generalize
before a second consumer exists.

### Bug B: the GL renderer shows the editor's cli_io fields as static text

**Symptom:** In ASCII mode, the flow above (now) renders correctly — buttons are
clickable, labels are readable. In GL mode, the same embedded content renders,
but the `<cli_io>` input fields show up as inert text, not editable fields (the
`[-]`/`[+]` stepper buttons and other plain `<button>` elements are fine in both
modes — this is specifically a `<cli_io>` problem).

**Root cause, confirmed by reading the code (not yet reproduced live — the
previous agent can't drive a TTY, only the human user can run this and report
back):** `wraith-alpha_manager.c` has a generic function,
`emit_embedded_line_objects()` (~line 1813), that's the *only* bridge between "a
line of raw markup in some project's `session/wraith_body.txt`" and "a semantic
`OBJECT` record the GL renderer (`wraith_gl.c`) can draw." It's called from
`write_semantic_projection_files()` (~line 2236), which in turn is called from
`update_state()` — so this is the GL-side equivalent of what
`append_project_probe_body()` does for ASCII.

The bug: `emit_embedded_line_objects()` uses `extract_attr()` to pull out only
**`label`, `onClick`, and `href`** from each embedded tag (~line 1817-1819):

```c
extract_attr(p, tag_end, "label", label, sizeof(label));
if (!extract_attr(p, tag_end, "onClick", action, sizeof(action))) {
    extract_attr(p, tag_end, "href", action, sizeof(action));
}
```

A `<cli_io id="edit_x" label="  X position" target_id="edit_x" />` tag has a
`label` attribute (so it extracts fine and gets *some* text rendered) but
**no `onClick` and no `href`** — so `action[0]` stays empty, `nav` stays `0`
(non-interactive, not Tab/click-focusable), and the whole element gets emitted
as a plain, inert `tag=text` OBJECT. This function has **zero concept of
`<cli_io>` as a distinct element type** — unlike the ASCII parser
(`wraith_parser_alpha.c`), which has real, fairly developed `cli_io` handling
(a `target_id`-keyed value store specifically so "multiple cli_io fields don't
collide," see its own comments around line 75 and 2031). That ASCII-side
handling was written for a project's own *statically-declared* `.chtpm` layout
elements, though — it is **not verified** to also correctly handle `cli_io` tags
arriving dynamically via the embedded-body-passthrough mechanism (raw lines in
someone else's `wraith_body.txt`). That's a second, related open question, not
just the GL gap.

**Fix path (not yet implemented, needs a design decision, not just a patch):**
at minimum, `emit_embedded_line_objects()` needs a `cli_io`-aware branch that
emits something GL can render as an actual input field (or, more conservatively,
some kind of clickable/focusable placeholder if a real GL text-input widget
doesn't exist yet — check `wraith_gl.c` for whatever OBJECT `tag=` values it
already knows how to draw before deciding what to emit). Whoever picks this up
should also independently verify whether the ASCII side's dynamically-embedded
`cli_io` handling actually works correctly for >1 simultaneous field (the user
flagged this as a known-suspect area before this session even started digging
into window-geom specifically — treat it as still open, not resolved just
because the picker/editor navigation itself now works in ASCII).

## What to do next, in order (updated 2026-07-06, end of session)

Bug A, Bug B, and the standalone chrome-button path are all now applied/built
(see the appendices below for exactly what changed and where). None of it has
been exercised live yet — everything below is "compiles clean," not "tested."

1. **Run it live first, before touching more code.** Open Settings → Window
   Geometry → pick a project (embedded path). Confirm: starts at the top menu
   every fresh window open (Bug A), cli_io fields hold independent text and
   show a real input box in GL (Bug B), buttons/Apply work in both ASCII and
   GL.
2. Click the new `[g]` chrome button on some other project's window (the
   standalone path). Confirm it opens a `WINDOW GEOM` window, shows that
   project's geometry, and behaves the same as #1. This is a brand-new code
   path with zero live runs so far — expect to find something wrong with it
   (the GL button's `x=96` placement is the most likely first thing to be
   visibly off, per the appendix below).
3. Cross-check: with both a Settings-embedded editor and a standalone
   window-geom window open at once (different target projects), confirm they
   don't clobber each other's state — they use separate `session/` dirs, but
   this hasn't been checked live.
4. Only after both paths are confirmed working: decide whether to fold
   settings' embedded editor into delegating to window-geom (removing the
   duplication flagged in the chrome-button appendix), or leave them as two
   independent implementations. That's a real design choice, not a bug fix —
   ask the user rather than picking one unilaterally.

## APPENDIX: Bug B — corrected fix strategy (2026-07-06, second pass)

**The first pass at this appendix (written earlier the same session) was wrong
and has been replaced.** It proposed inventing a new `CLI_IO:<target_id>`
action string and a matching new case in `wraith_gl.c`. That was written from
pattern-matching the handoff's own prose, without re-reading
`emit_embedded_line_objects()` or `hit_test_semantic_action()` directly. Once
actually read, both turned out to already have the exact mechanism needed —
no new action vocabulary required. Leaving this note so nobody re-invents the
same wrong fix from the same prose.

**What's actually true, confirmed by reading the code:**

`emit_embedded_line_objects()` (`wraith-alpha_manager.c`, real line ~1831, not
~1813) only assigns a nonzero `nav` value to a chunk when it has a non-empty
`action` (`onClick`/`href`) — see the real condition at ~line 1870:

```c
if (action[0] && next_nav) {
    nav = (*next_nav)++;
}
```

A `<cli_io>` tag has a `label` but no `onClick`/`href`, so `action[0]` is
always empty for it, so it never gets a nav slot — it's emitted as an inert
`tag=text` OBJECT with `nav=0`. That's the entire bug.

**Why nav=0 is enough to break it, and why the fix doesn't need a new action
string:** `wraith_gl.c`'s `hit_test_semantic_action()` (~line 253) already has
a generic fallback —

```c
if (kvp_value(line, "action", ...) && action[0] != '\0') {
    /* already populated */
} else if (kvp_value(line, "nav", ...) && atoi(value) > 0) {
    snprintf(action, sizeof(action), "SET_ACTIVE:%d", atoi(value));
}
```

Any OBJECT with `nav > 0` and no explicit `action` automatically becomes
clickable via a synthesized `SET_ACTIVE:<nav>`, and `SET_ACTIVE:` is already a
real, working command (`route_command()` → `dispatch_menu_index()`). This is
the same generic click-to-focus path chrome buttons, taskbar items, and the
ASCII/GL mode selectors all already use. **The fix is just: give `<cli_io>`
tags a nav slot too.**

**The one thing that made this safe to do without a wider nav-renumbering
pass:** `count_embedded_body_nav_slots()` (~line 747) — the function that
reserves how many nav slots `launcher_start`/`g_max_index` leave room for —
already counts *any* labeled tag, cli_io included, not just actionable ones
(its own comment: "mirrors emit_embedded_line_objects()'s own chunk-detection
criterion... same label check"). So the numeric range for cli_io tags is
**already reserved as a safe upper bound** — assigning them a real nav value
inside that already-reserved range doesn't collide with anything else's
numbering. No changes needed to `count_embedded_body_nav_slots()`,
`dispatch_menu_index()`, or `recompute_nav_bounds()`.

**Applied this session** (`emit_embedded_line_objects()`, ~line 1878):

```c
if ((action[0] || strncmp(p, "<cli_io", 7) == 0) && next_nav) {
    nav = (*next_nav)++;
}
```

That's the only line that changed in `emit_embedded_line_objects()`. Compiled
clean (225/225, 0 failed) — not yet exercised live, see the standard caveat.
Everything else — click resolution, focus, keyboard capture, target_id state
isolation — already works once the element has a nav slot, because ASCII and
GL both funnel keyboard input through the same
`pieces/keyboard/history.txt` → `wraith_parser_alpha.c` pipeline regardless of
which renderer captured the keystroke (see `parsers.txt`, section 3).

**Testing after applying it:**
1. Compile, open Settings → Window Geometry → pick a project (embedded path),
   or click the new `[g]` chrome button on any project window (standalone
   path, see below).
2. GL mode: click a cli_io field — should now visually focus (same highlight
   convention as other focused elements) and accept typed characters.
3. Type in field X, click field Y, click back to field X → X's text should
   have persisted (this is the target_id fix already confirmed live in
   `wraith_parser_alpha.c`, doing its job).
4. ASCII mode: same sequence, should already work (unaffected by this fix).

---

## APPENDIX: Chrome button — implemented this session (2026-07-06)

**Superseded — read "Chrome button — second regression..." near the end of
this doc instead for the numbering scheme.** This appendix's nav=0 approach
for `[g]` (described below) shipped, was live-tested, and turned out to
cause a real ASCII/GL numbering divergence. Everything else in this
appendix (the manager/ops files created, the `project_dir_for_window()`
nesting fix, the duplication-vs-window-geom design note) is still accurate
and doesn't need re-reading with corrections in mind — only the nav
numbering part was wrong.

The chrome-button/standalone path did not exist at all before this session —
`window-geom_manager.c` only ever wrote `manager/state.txt` (the older,
deferred href-based convention from the original design doc), never
`session/wraith_body.txt`, so opening it as a Window would have hit the
generic "Missing project body file" fallback. It's now implemented, in
parallel with the existing embedded (Settings → picker → editor) path — the
two do **not** share a manager process or files; see the next section for
exactly why, and what's still duplicated.

**What's actually shipped this session** (compiled clean, 225/225 binaries,
`0 binaries failed` — not yet exercised live, see the standard caveat at the
bottom of this doc):

1. `wraith-alpha_manager.c`:
   - `project_dir_for_window()` (~line 1465) no longer rejects a nested
     `project_id` (it was rejecting anything containing `/` after the
     `wraith-alpha/wraith-projects/` prefix — `settings/window-geom` has one).
     The `..`-traversal check, the actual security boundary, is untouched.
     Without this fix, window-geom could never resolve its own directory once
     opened as a standalone Window.
   - New `DESKTOP_ACTION:open_window_geom:<project_id>` handler in
     `route_command()` (next to the other `DESKTOP_ACTION:` handlers, before
     the generic `.chtpm` fallback): writes the clicked project's id into
     `.../window-geom/session/wg_target.txt`, bumps
     `.../window-geom/session/regenerate_marker.txt` (so a retarget while
     window-geom is already running from an earlier open is picked up —
     `wg_target.txt` is a whole-file overwrite, not an append, so its own
     size can't be trusted as a growth signal), then calls
     `launch_window_instance()` the same way every other wraith sub-project
     opens.
   - A new `[g]` chrome button, added to both the ASCII `raw` markup buffer
     (right after the `x` close button) and the matching GL `OBJECT` emission
     (right after `chrome_close`). Deliberately given `nav=0` in the GL
     OBJECT — it doesn't need a `SET_ACTIVE:<N>` slot in the 1-4 chrome-button
     numbering, because it carries its own explicit
     `action=DESKTOP_ACTION:open_window_geom:<project_id>` directly, and
     `hit_test_semantic_action()` already prefers an explicit `action` over
     the nav-based `SET_ACTIVE` fallback (see Bug B's fix above for that same
     mechanism). ASCII gets it "for free" — `wraith_parser_alpha.c` numbers
     interactive elements itself, on every reparse, independent of this
     file's own GL nav math (confirmed pre-existing behavior, not something
     added this session). GL placement (`x=96`) is a first-pass estimate,
     not verified against a live render — check it doesn't visually overlap
     `window_chrome_row`'s own declared `w=94` once someone can eyeball it.

2. `window-geom_manager.c`: new `write_wraith_body()` function (mirrors
   `settings_manager.c`'s editor-page branch almost exactly — same cli_io +
   KEY:5-13 markup shape, same target-mismatch-resets-working-values logic),
   writing to its own `session/wraith_body.txt`. Reads the target project id
   from `session/wg_target.txt` (written by the new chrome-button handler
   above) rather than settings' active-page string. Wired into `main()`'s
   init and into the existing hot-path loop (same `regenerate_marker.txt`
   growth-watch that was already there for `write_state()`, now also
   triggers this). `write_state()`/`manager/state.txt` were left untouched —
   they're for the older, deferred href-based convention; nothing currently
   reads them, but removing them wasn't this session's job.

3. New `ops/src/wraith_project_input.c` for window-geom (project had no
   `ops/` directory at all before this session): mirrors settings'
   `wraith_project_input.c` almost line-for-line (KEY:5-12 nudge x/y/w/h,
   KEY:13 applies to the target's `project.pdl` WINDOW section) — the one
   real difference is it has no "active page" gate to check, since this
   project *is* the editor unconditionally whenever it's the focused window,
   unlike settings which hosts several pages behind one manager. Confirmed
   picked up automatically by `compile_all.sh`'s existing
   `compile_wraith_project_tree()` (`ops/src/*.c` under a nested wraith
   project was already a handled case) and lands at
   `ops/+x/wraith_project_input.+x`, exactly where
   `run_active_project_input_op()` looks.

**Why this duplicates settings_manager.c's editor markup instead of settings
delegating to window-geom for the embedded path too:** that reuse — settings
picker launches/associates the real `window-geom.chtpm` + manager for
whichever project was picked, the same way the chrome button now does — is
the architecturally-intended shape (see
`settings-hub-window-geom-design-j5.md`'s Implementation Addendum, line 353:
"Both point to the same window-geom layout file, but different entry
contexts"). It is **not** what's shipped: `settings_manager.c`'s embedded
editor page is fully self-contained (hand-writes its own `<cli_io>`/button
markup directly into `wraith_body.txt`, has its own `edit_state.txt`, its own
KEY:5-13 handling in its own input op) and does not invoke window-geom at
all. That divergence pre-dates this session and was not touched — the ask
this session was specifically to get the standalone/chrome-button path
*reachable* so both could be tested side by side, not to reconcile the two.
Whoever picks up next should treat "make settings delegate to window-geom
instead of duplicating it" as its own separate refactor, informed by
whichever of the two paths tests better once both are actually runnable.

**Known gaps in what shipped, flagged not hidden:**
- GL chrome-button x-position (96) is a first-pass estimate, unverified live.
- No picker inside the standalone window-geom Window — it only ever shows
  whichever single project `wg_target.txt` points at. Multi-window picking
  still only exists in the embedded (Settings) path.
- The `window-geom.chtpm` / `layouts/` file that already existed before this
  session is **not** read by either path (embedded or the new standalone
  one) — both paths hand-write their own markup into `wraith_body.txt`
  instead. That `.chtpm` file is effectively orphaned by both today; it was
  built for the original, since-superseded href/`current_layout`-swap design
  (see the design doc's own revision history) and never updated for either
  the embedded-Window or new standalone-Window convention.
- Bug B's one-line fix (above) **is applied** — both the embedded and new
  standalone paths' cli_io fields should now get a nav slot in GL. Not yet
  confirmed live (needs the human user to actually open both and click a
  field).

---

## APPENDIX: Secondary bug in chtpm_parser.c (independent of window-geom, but documented in same codebase)

File: `pieces/chtpm/plugins/chtpm_parser.c` (~2690 lines)

This is a **separate, second parser** (not the same as `wraith_parser_alpha.c`; wraith-alpha forked its own copy). It's used by the orchestrator to launch ~40 other projects (user, quiz-engine, bot-editor, etc.).

**Identical bug, independent of window-geom:**
- Same single "input_text" collision: all cli_io fields on the same screen share one gui_state key
- Same fix needed: add target_id parsing and keyed restore/save
- Same line numbers (approx): UIElement struct (~line 78), parse_attributes (~1492), sync_cli_input_from_gui_state (~159-188), write sites (~2485, 2508, 2519)
- **Live broken today:** `projects/user/layouts/user_signup.chtpm` has `target_id` attributes that are currently ignored, so username and password fields collide on reparse

**Scope:** This is NOT required to fix window-geom (which uses wraith_parser_alpha.c, not chtpm_parser.c). BUT it is a real user-facing bug in a shipped project. Recommend fixing as a **separate task** after window-geom is stable, since it touches a shared engine used by many projects and has its own verification needs (relaunch "user" project standalone, test both fields hold independent text).

---

## How to test (you cannot drive this yourself)

This is an interactive TTY desktop application. The agent cannot type into it or
see its live output directly — only the human user can run it and paste frame
dumps back. Do not conclude something is broken from stale files alone; ask the
user to run it and report the actual frame output before diagnosing further.
Useful non-interactive checks you *can* run directly:

- `bash "#.dev-storage/#.tools/compile_all.sh"` — rebuild everything, check for
  `0 binaries failed`.
- `bash "#.dev-storage/#.tools/check_binaries.sh"` — confirm no missing
  binaries.
- `cat projects/wraith-alpha/wraith-projects/settings/session/state_changed.txt`
  and `.../manager/debug_log.txt` — see exactly what page-state and render
  events actually happened, cross-referenced against
  `projects/wraith-alpha/manager/alpha_manager.log` (search for `Command
  received:` lines) to confirm a click's command actually got routed before
  assuming a rendering-layer bug.
- **Frame debugging:** See `!.gem-flashlite--yolo/!.evo&obs/TPMOS_BIBLE_APPENDIX_FRAME_DEBUG.md` and `WRAITH_RGB_ARCHITECTURE.md` for how to inspect current_frame.txt, verify wraith_rgb_daemon is rasterizing correctly, and trace which layer a bug is actually in.

---

## APPENDIX: Chrome button — second regression, root cause, and the fix that actually holds (2026-07-06, third pass)

The chrome-button appendix above ("implemented this session") shipped a
version that compiled clean but was live-tested by the user and found
broken in a specific, diagnosable way. This section replaces that version's
numbering approach. Read this appendix instead of trusting the nav=0
approach described earlier in this doc — it's kept above only so the
reasoning trail isn't lost, not as current guidance.

### What the user observed, live

Screenshot (GL mode, embedded Settings → Window Geometry → piececraft-wraith):
cli_io fields showed as `[ ] 5. [  X` / `[ ] 6. [  Y` / `[ ] 7.` / `[ ] 8. [`
— visually truncated, and not accepting typed input. The `[-]`/`[+]`
nudge buttons below them, present in ASCII, were **entirely absent** in GL.
Separately, in ASCII, pressing the number key `5` (expecting to activate
"Window Geometry" in the Settings menu, which was numbered `6` on screen)
instead focused the new `[g]` chrome button — confirmed via a frame-history
dump showing `Idx:5 Key:5` selecting `[>] 5. [g]`, not the menu item.

### Root cause of the numbering mismatch (confirmed by tracing, not guessed)

The shipped version gave `[g]` `nav=0` in the GL `OBJECT` emission,
deliberately, specifically to avoid touching the hardcoded "4 fixed chrome
slots, content starts at nav=5" arithmetic scattered across
`dispatch_menu_index()`, `recompute_nav_bounds()`, and
`write_semantic_projection_files()`. That shortcut created exactly the bug
it was trying to dodge, one layer removed:

- **ASCII** doesn't use any of that arithmetic for its own on-screen
  numbers. `wraith_parser_alpha.c` re-parses whatever markup it's given
  every frame and assigns each interactive element the next sequential
  display number itself, independent of anything in
  `wraith-alpha_manager.c`. Inserting `<button label="g" .../>` into the
  `raw` markup buffer — textually, before the Settings menu's own button —
  made ASCII's own numbering shift everything after it down by one,
  automatically and correctly *from ASCII's perspective*. This is why
  "Window Geometry" became `6` on screen.
- **GL** does NOT re-derive numbering from parsed markup. Its numbers come
  from `wraith-alpha_manager.c`'s own fixed arithmetic (`launcher_start`,
  `taskbar_start`, `next_body_nav`, all hardcoded to assume exactly 4
  chrome slots). Giving `[g]` `nav=0` meant it consumed **zero** slots in
  that arithmetic, so nothing after it moved. "Window Geometry" stayed `5`
  in GL.
- Two independent numbering systems, one of which shifted and one of which
  didn't, for the same click. That's the divergence the user caught.

The cli_io truncation and missing `[-]`/`[+]` buttons turned out to be
**unrelated** to this numbering bug — see "Two open questions, not yet
resolved" below. Don't assume fixing the numbering fixes those too.

### First attempted fix, and why it was reverted before shipping

The obvious fix is to actually reserve `[g]` a real 5th chrome slot:
bump every hardcoded `4`/`5` in `dispatch_menu_index()`,
`recompute_nav_bounds()`, `write_semantic_projection_files()`, and
`build_desktop_shell_markup()` up by one, matching ASCII's own auto-shift.
This was implemented, then reverted after checking
`next_body_nav`'s own comment (`wraith-alpha_manager.c` ~line 2490), which
documents that embedded project-body nav values start at `5` specifically
so they never collide with a project's own **hand-authored**
`session/scene.objects.pdl` nav range, which *also* starts at `5` by
established convention. Confirmed via `grep`:

```
projects/wraith-alpha/wraith-projects/piececraft-wraith/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/wraith-browser/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/chtmgl-wraith/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/wraith-ed/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/emoji-studio-wraith/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/web-cam/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/chtmgl-video-isolate/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/fs/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/wraith-3d-cube/session/scene.objects.pdl
projects/wraith-alpha/wraith-projects/screen-record/session/scene.objects.pdl
```

Nine projects with their own hand-written `nav=5..N` values. Bumping the
chrome base to 5 (content starts at 6) would have required renumbering
every one of those files too, plus checking whether any op code
(`mining.c`, `placing.c`, `zombie_ai.c`, etc.) reads those specific numbers
elsewhere. That's a much bigger, riskier change than this task's actual
scope, and not something to do silently as a side effect of adding one
chrome button. **Reverted before compiling further.**

### The fix that shipped: a trailing nav slot, not a low fixed one

`wraith-alpha_manager.c` already has exactly this pattern, twice, for the
`ASCII*`/`GL` mode-selector row:

```c
static int debug_selector_ascii_index(void) { return g_max_index - 1; }
static int debug_selector_gl_index(void)    { return g_max_index; }
```

Both are offsets *from the top* of whatever the current nav range is, not
low fixed numbers — so they never collide with anything else, regardless of
how many launcher/taskbar/scene slots exist below them. Added a third,
identical function:

```c
static int chrome_geom_index(void) { return g_max_index - 2; }
```

And used it everywhere `[g]`'s number is needed:
- `recompute_nav_bounds()`: base chrome count stayed `4` (unchanged); the
  trailing `+2` (which reserves the two debug-selector slots) became `+3`
  to also reserve this one.
- `dispatch_menu_index()`: `launcher_start` stayed `5 + extra_project_slots`
  (unchanged); added `if (menu_index == chrome_geom_index())` right after
  the existing `menu_index == 4` (close) case, calling the shared
  `open_window_geom_for_project()` helper (see below).
- `write_semantic_projection_files()`: `taskbar_start` stayed
  `5 + launcher_count` (unchanged). `next_body_nav` stayed `5` (unchanged
  — this is the value that would have collided with a low-fixed `[g]`
  slot; leaving it alone is exactly why the trailing-slot approach doesn't
  need the scene.objects.pdl renumbering the first attempt would have).
- The `[g]` `OBJECT`'s own `nav=` and `action=` fields: `chrome_geom_index()`
  (computed fresh each call), not a literal `5`.
- `build_desktop_shell_markup()` (the ASCII `raw` buffer): `[g]`'s `onClick`
  is now `SET_ACTIVE:<chrome_geom_index()>` (formatted with `%d`, not a
  literal string) — this is the value that actually gets sent to
  `route_command()` on click; ASCII's own on-screen display number for
  this button (whatever `wraith_parser_alpha.c` happens to auto-assign it)
  doesn't need to match this and isn't expected to.

**Refactor alongside this:** the `open_window_geom_for_project(const char
*target_project_id)` helper (writes `wg_target.txt`, bumps window-geom's
`regenerate_marker.txt`, calls `launch_window_instance()`) used to be
inlined directly in `route_command()`'s `DESKTOP_ACTION:open_window_geom:`
handler. Pulled it out to a shared static function so both that named
command *and* `dispatch_menu_index()`'s new `chrome_geom_index()` case call
the same code — otherwise this would have been the third place this logic
existed, with a third chance to drift.

**Also retightened the GL chrome-row spacing**, independent of the
numbering fix: the very first version placed `[g]` at pixel `x=96`, past
this panel's own declared `w=94` — arithmetically guaranteed to render
clipped/off the edge in GL regardless of the numbering bug, since
`o`/`-`/`x` at width=10/delta=11 already used the full `63→95` span with no
room left for a 4th button. Retightened `o`/`-`/`x`/`g` to width=7/delta=8
(`63,71,79,87`, last one ending at `94`) to actually fit within bounds.
**Not verified against a live render** — this is a first-pass recalculation,
not something visually confirmed; if `[g]` still looks cramped or
overlapping once someone can see it live, this spacing is the first thing
to adjust, not the nav-slot logic above it.

**Verification done:** compiles clean (`gcc -Wall -Wextra`, exit 0, no
errors — only the same pre-existing snprintf-truncation warning style
already common throughout this file) and `compile_all.sh` reports 225/225,
0 failed. **Not yet exercised live.** Caught one real bug during this pass
that a live test would eventually have surfaced anyway: an early draft of
this fix's comments wrote literal `ASCII*/GL` inside a `/* */` block
comment — the `*/` inside that prose closed the comment early, turning the
rest of the sentence into invalid C and failing to compile. Fixed by
writing `ASCII / GL` instead. Mentioned here only as a reminder to actually
run the compiler after any comment edit, not just code edits — it's cheap
and this session skipped it once already this exact way earlier (see the
path-escaping memory entry from the same day).

### Two open questions, not yet resolved

**1. cli_io fields not accepting input, and/or rendering truncated in GL.**
Checked whether this was a data-generation bug: it is not. The live
`current_frame.objects.pdl` on disk (captured from some earlier point in
the user's test session, for the `terminal` project rather than
`piececraft-wraith` — these two files are from different moments and
projects, not a matched pair, so treat this only as a partial data point)
shows fully correct, untruncated `label=` values for each cli_io field
(`label=  X position`, `label=  Width`, etc., each with a `w=` matching the
label's real length) and correctly assigned `nav=5..8`. So whatever the
user saw truncated in the screenshot is **not** because
`emit_embedded_line_objects()` generated bad data.

That narrows it to one of:
- `wraith_gl.c` doing something to the object beyond hit-testing that this
  session didn't find (re-checked: `wraith_gl.c` has no text-drawing code
  of its own at all — grep for `nav_selector_glyph`/`draw_text` there
  returns nothing — it is a pure `current_frame.rgba32` texture blit plus
  click hit-testing against `objects.pdl`; it does not render labels, so it
  can't be truncating them independently of what's in the texture).
- `current_frame.txt` (the ASCII text) itself already containing
  truncated/wrapped content by the time `wraith_rgb_daemon.c` rasterizes
  it — i.e. a bug upstream of GL entirely, in how the embedded cli_io
  markup gets composed into the desktop's own ASCII frame. Not confirmed
  either way; the `current_frame.txt` captured on disk during this
  investigation was for yet another different project/moment (`terminal`,
  showing pre-existing `hi` test input in all 4 fields, unrelated leftover
  data from a prior session, not helpful for checking truncation).
- `wraith_rgb_daemon.c`'s rasterization itself doing something project
  wrong for this specific text (font metrics, glyph lookup) — not checked
  this pass.
- The GL **window** itself (the actual OS-level window in the screenshot)
  being narrower than the full 960×540 texture's aspect ratio, and
  `update_texture_viewport()`/`map_window_mouse_to_texture()`'s scaling
  math clipping rather than proportionally scaling — also not checked this
  pass.

**Whoever picks this up next: get a fresh, matched `current_frame.txt` +
`current_frame.objects.pdl` + `current_frame.rgba32` pair, all from the
exact same render, for the exact same project, before diagnosing further.**
The pair inspected this session was mismatched (different projects,
different moments) specifically because the disk state had moved on since
the user's screenshot — don't repeat that mistake by trusting stale files
as if they were live.

**2. The `[-]`/`[+]` nudge buttons (and Apply/Back) missing entirely from
GL**, not just non-interactive. `emit_embedded_line_objects()` already
handles plain `<button>` tags with a real `onClick` (unlike cli_io, they
get `action[0]` set directly, so they were never gated by this session's
nav fix at all) — there's no code-level reason they should be *absent*.
Given cli_io fields at least showed *something* (garbled), while these
showed *nothing*, whatever's happening likely affects them differently, not
identically — meaning this may not have the same root cause as #1 above.
Not investigated further this pass; needs the same fresh, matched
frame-file triplet as #1 before guessing at a cause.

**Do not attempt a third fix for these two based on pattern-matching the
nav investigation above.** That's exactly the mistake that produced this
session's *first* regression (guessing a fix from prose without reading the
current code) and its *second* (a real fix for one bug that turned out to
need a wider check before shipping). Get live, matched frame data first.

---

## APPENDIX: Two more confirmed bugs, found from live evidence (2026-07-06, fourth pass)

The user tested the trailing-nav-slot fix above live and reported two new
symptoms: `[g]` **disappeared from GL entirely**, and separately, in ASCII,
"the numbers start over at 1" for the chrome row. Both were root-caused this
pass by reading live `objects.pdl` and `wraith_body.txt` directly — not
guessed. Also root-caused, from the same live evidence, why the
`[-]`/`[+]` buttons were missing from GL (open question #2 in the appendix
above). Both fixes are applied and compiled clean.

### Bug: `chrome_geom_index()` collided with the title button's own nav=1

Live `current_frame.objects.pdl` (captured mid-investigation, `terminal_2`
window) showed:

```
id=terminal_2_title ... nav=1 ... action=SET_ACTIVE:1
id=terminal_2_geom  ... nav=1 ... action=SET_ACTIVE:1   <- same as title
```

`chrome_geom_index()` (`g_max_index - 2`) had computed to `1` — meaning
`g_max_index` was `3` at that moment, which is the **"no active window"**
formula (`1 + taskbar_count + 2`), not the "window is active" formula that
should have applied (which computes to 8 or more). `g_max_index` was stale.

**Why:** `recompute_nav_bounds()` (the only function that (re)computes
`g_max_index`) was called from exactly two places: `route_input()` (every
keyboard press) and once at startup in `main()`. It was **never** called
when a window opens via a *mouse click* — which covers most of how windows
actually open: `dispatch_menu_index()`'s taskbar/launcher branches,
`launch_wraith_project_command()`, and this session's own
`open_window_geom_for_project()` all call `update_state()` afterward, but
none of them called `recompute_nav_bounds()` first. So the first GL render
after opening a window via mouse click could use a `g_max_index` left over
from whatever the desktop's *previous* state was (e.g. no window focused at
all) — and `chrome_geom_index()`, sitting right next to the 1-4 fixed
range, is far more exposed to this than `debug_selector_ascii_index()`/
`gl_index()` were, since even a modestly-stale small `g_max_index` lands
`g_max_index - 2` inside 1-4 instead of safely above it.

**Fix:** moved `recompute_nav_bounds()` into `update_state()` itself (top of
the function, right after `normalize_registry()`), so every single caller
that ends in `update_state(0); trigger_render();` — which is effectively
all of them, by existing convention — gets a freshly-recomputed
`g_max_index` before any GL objects or ASCII markup referencing it are
built. Needed a new forward declaration (`recompute_nav_bounds()` is
defined ~line 4074, far below `update_state()`'s ~line 2838). This closes
the gap for every current and future caller at once, rather than adding
`recompute_nav_bounds()` calls at each of the (many) places that open a
window — the latter would have been the third time this session added a
call site instead of fixing the shared function once.

**Why the ASCII "numbers start over at 1" symptom is the same bug, seen
from the other renderer:** ASCII's own auto-numbering
(`wraith_parser_alpha.c`) is unaffected by `g_max_index` — it assigns
its own on-screen digits by parsing order, always. What the user saw as
"numbers start over at 1" in ASCII was the **command that actually got
sent** on click resolving wrong (`SET_ACTIVE:1` embedded as `[g]`'s onClick
via `chrome_geom_index()`, computed with the same stale `g_max_index` at
the moment the ASCII raw buffer was built) — clicking the button visually
labeled some higher number silently did whatever `SET_ACTIVE:1` does
(select the title), not open window-geom. Same root cause, different
visible symptom per renderer, exactly as with the first regression earlier
in this document.

### Bug: `[-]`/`[+]` buttons (and Apply) missing from GL — root cause found, not the numbering bug

Live `wraith_body.txt` for the standalone window-geom editor (after the
chrome button successfully opened it — confirmed working) is **20 lines**:

```
1  WINDOW GEOMETRY EDITOR (standalone)
2  (blank)
3  Project: ...
4  Current: ...
5  (blank)
6  Edit via CLI Input:
7-10  <cli_io ...> x4
11 (blank)
12 Or use buttons (working values: ...):
13-16 <button [-]/[+] ...> x4 pairs
17 (blank)
18 <button Apply Changes ...>
19 (blank)
20 Status: ...
```

`write_semantic_projection_files()` (GL) read this body via
`project_probe_body_lines(window, project_lines, 12)` — **a hardcoded
`max_lines=12`**, with the caller's own array declared `const char
*project_lines[12]`. Line 12 is "Or use buttons...", the last line inside
the cap — every button line (13 onward) was never read into
`project_lines[]` at all, so `emit_embedded_line_objects()` never saw them
and never emitted an `OBJECT` for them. This has nothing to do with the
nav-numbering bug above; it would have truncated the body identically even
before the chrome button existed, for any project whose embedded body
happens to exceed 12 lines.

**This exact bug class already has a known fix, applied once and not
carried over:** `append_project_probe_body()` (ASCII's equivalent function,
~line 1542) has a comment describing being raised from a 12-line cap to 40,
specifically because it silently truncated `piececraft-wraith`'s 21-line
body "after the 8th map row in every mode." `project_probe_body_lines()`
(GL's equivalent) has its own internal storage capped at 16
(`body_lines[16][256]`) and clamps `max_lines` to 16 even if a caller asks
for more — but the actual call site asked for only 12, an even smaller,
separate ceiling stacked on top.

**Fix:** raised `project_probe_body_lines()`'s internal storage from
`body_lines[16][256]` to `body_lines[40][256]` and its clamp from 16 to 40
(matching ASCII's already-fixed ceiling exactly, so the two renderers can't
silently diverge on this again), and raised the call site's array
(`project_lines[12]` → `project_lines[40]`) and argument
(`project_probe_body_lines(window, project_lines, 12)` → `..., 40`) to
match. The line-iteration loop right after already used
`project_line_count` (the function's actual return value, not a literal
12), so no change was needed there — only the storage ceiling and the
number requested at the call site were wrong.

### Status after this pass

Both fixes compiled clean (`gcc -Wall -Wextra`, exit 0) and
`compile_all.sh` reports 225/225, 0 failed. **Not yet re-tested live** —
the user should re-open both the embedded (Settings → Window Geometry) and
standalone (chrome button) editors in both ASCII and GL, and confirm:
`[g]`'s nav no longer collides with anything (check `objects.pdl` directly
if in doubt — `nav=` for `*_geom` should be a large number, clearly not
1-4), and the `[-]`/`[+]`/Apply buttons now appear and are clickable in GL.

The still-genuinely-open item is the **cli_io label truncation / non-input
symptom** from the original screenshot (open question #1 in the appendix
above) — ruled out as a data-generation bug (confirmed via `objects.pdl`:
full untruncated labels, correct nav), and now also ruled out as being
caused by either bug fixed in this pass (the truncated fields were within
the old 12-line cap, so they were never affected by it; and their nav
values 5-8 were never stale-colliding the way `chrome_geom_index()` was).
Still needs a fresh, matched `current_frame.txt` +
`current_frame.objects.pdl` + `current_frame.rgba32` triplet, from the
exact same render, to diagnose — most likely candidates remaining are
`wraith_rgb_daemon.c`'s rasterization or the GL window's texture-viewport
scaling, neither checked yet.
