# Settings Hub + window-geom Design (J5)
Date: 2026-07-05
Status: First slice IMPLEMENTED and standalone-verified. Not yet
reachable in a live session — see "How to reach it" below.

## Revision note (same day)

The first version of this doc designed `window-geom` as an internal
*mode* inside a single persistent `settings` window, to avoid a nesting
restriction found in `project_dir_for_window()`. That was based on an
incomplete picture — I hadn't yet traced how TPMOS's existing `href`
navigation actually works. Having now confirmed it (below), the
restriction doesn't apply to this design at all, and the simpler,
originally-intended shape — real, separate, standalone projects,
navigated between via `href` — is what's recorded here instead.

## What `href` actually does (traced, not assumed this time)

`wraith_parser_alpha.c` (Wraith's own layout parser) keeps a global:

```c
char current_layout[MAX_PATH] = "pieces/chtpm/layouts/os.chtpm";
```

Every frame, `parse_chtm()` does `read_file_to_string(current_layout)`
and renders *that* file as the entire screen. Clicking any element with
a non-empty `href` runs `strncpy(current_layout, el->href, ...)` —
directly reassigning which file is "the current screen." This is not
vestigial: `terminal.chtpm` and `fs.chtpm` already ship a working
`<button label="Back to Wraith Desktop" href="projects/wraith-alpha/layouts/alpha-shell.chtpm" />`
today, using exactly this mechanism to leave their own layout and land
back on the main desktop shell. Same mechanism exists in the classic,
non-Wraith `chtpm_parser.c` too — it's shared, real, and working in
both places.

**Why this changes the design:** `href` navigation doesn't go through
`Window` / `g_windows[]` / `project_dir_for_window()` at all — it's a
much simpler "swap which file is being read" operation. The nesting
restriction I found in `project_dir_for_window()` only matters for
projects that need to appear as a `Window` in the desktop-shell's
taskbar. A project reached purely by `href` never touches that function
— so `window-geom` can be a genuinely real, independent project, nested
on disk under `settings/` or not, with no conflict at all.

## Architecture (revised)

### `settings` — a real, standalone project

- `projects/wraith-alpha/wraith-projects/settings/` — full project
  shape: `project.pdl`, `manager/+x/settings_manager.+x` (init-only,
  per `wra-mana-checklist.txt`), `ops/+x/wraith_project_input.+x`,
  `layouts/settings.chtpm`, `session/state.txt`. Same convention as
  every other Wraith project — nothing special-cased.
- Reached via `href` from wherever makes sense (a link somewhere in the
  main desktop, or a taskbar/launcher entry) — same pattern
  `terminal.chtpm` already uses to reach `alpha-shell.chtpm`, just in
  the other direction.
- **Discovery, not hardcoding**: its ops binary scans its *own*
  directory for immediate subdirectories declaring themselves a
  settings entry (a small marker file — see Open Questions), rendering
  each as a clickable `<button href="...that entry's own layout...">`.
  Mirrors the *conceptual* pattern `discover_launcher_projects()`
  already uses (scan a directory, find markers, list as options),
  reimplemented locally since this is a separate binary.
- Clicking a discovered entry's `href` **navigates away from
  `settings.chtpm` entirely** — `current_layout` becomes that entry's
  own layout file. This is the literal meaning of "settings window
  closes, then it's a real window": `settings` isn't a persistent
  wrapper the sub-project renders inside of, it's a menu you navigate
  *through*, the same way `terminal.chtpm` is something you navigate
  *away from* via its own "Back to Wraith Desktop" link.

### `window-geom` — the first settings entry, and the POC

- A genuinely real, independent project:
  `projects/wraith-alpha/wraith-projects/settings/window-geom/` (nested
  on disk purely for organization — for `href`-reached projects, disk
  nesting has no functional consequence, per the finding above). Full
  project shape of its own: `project.pdl`, `manager/`, `ops/`,
  `layouts/window-geom.chtpm`, `session/state.txt`.
- Its own layout carries a `href="projects/wraith-alpha/wraith-projects/settings/layouts/settings.chtpm"`-style
  link back, mirroring `terminal.chtpm`'s existing "Back to Wraith
  Desktop" convention exactly — go there, do a thing, come back.
- **Two ways in, same underlying edit view — this is the dual-purpose
  part:**
  1. **Generic**: `settings` menu → "Window Geometry" → `href` into
     `window-geom.chtpm`, which — finding no target pre-set — shows its
     own internal picker: a nav list of currently open windows. Pick
     one, and the same edit view renders for it.
  2. **Targeted (chrome-bar button)**: a window's own `[i]`-style chrome
     button sends `DESKTOP_ACTION:open_window_geom:<target_id>` →
     `route_command()` writes `wg_target_project_id=<target_id>` into
     `window-geom`'s own `session/state.txt` *before* navigating
     `current_layout` straight to `window-geom.chtpm` — skipping the
     picker, since a target is already on file when it loads.
- **The edit view reads/writes `<target>/project.pdl`'s `WINDOW`
  section directly** (same shape KPI A1 already built:
  `WINDOW | x | ...`). `window-geom` doesn't own this data — it's an
  editor over it, the same "sovereign artifact, host is a front door"
  principle already recorded in `0x-pet-wraith-architecture-j29.md`.

## How dynamic `${var}` content works for a standalone (non-desktop-embedded) project — traced, confirmed, zero shared-file changes needed

This was the one remaining unknown before implementation could start
safely: when a project is reached via `href` (not desktop-shell-embedded
via `Window`/`g_windows[]`), how does its layout get *any* dynamic
content at all?

Traced in `wraith_parser_alpha.c`:
- `set_project_id_from_layout()` derives `project_id` from
  `current_layout`'s own path (e.g.
  `projects/wraith-alpha/wraith-projects/settings/layouts/settings.chtpm`
  → `project_id = wraith-alpha/wraith-projects/settings` — same
  convention `Window->project_id` already uses).
- `load_vars()` then does
  `load_state_file("projects/%s/manager/state.txt", NULL)` with that
  `project_id` — a **generic**, already-existing mechanism, not
  something added for this feature.
- `load_state_file()` reads plain `key=value` lines and calls
  `set_var(key, value)` for each — confirmed multi-line values work too
  (`\n` in the value gets unescaped to a real newline during
  substitution), so a whole block of markup can be one `key=value` line.

**This means**: `settings`/`window-geom` each just need their own
manager to write plain `key=value` lines into their own
`manager/state.txt` (note: `manager/state.txt`, the classic-style path
this generic loader expects — **not** `session/state.txt`, which is a
different, Wraith-desktop-embedded convention used by
`append_project_probe_body()` for `Window`-registered projects). Their
own `.chtpm` layouts reference `${their_own_var}`, and it Just Works —
**zero changes to `wraith_parser_alpha.c` or any other shared file.**
Fully additive, exactly the "uninvasive" bar for this first slice.

**Where "what windows are currently open" comes from for `window-geom`'s
picker**: `window-geom`'s manager is a separate process — it can't read
`wraith-alpha_manager.c`'s in-memory `g_windows[]` directly. It reads the
same file-backed source of truth that already exists for this:
`projects/wraith-alpha/session/alpha_state.txt` (or
`desktop_ui_state.txt`), which already contains
`desktop_window_count=`/`desktop_window_N_id=`/`desktop_window_N_title=`
— written every frame by `write_projection()`. Reading this is the
file-backed way to do it, not a new mechanism.

## Reused, not reinvented

- `href` / `current_layout` navigation — real, already shipping in
  `terminal.chtpm`/`fs.chtpm`, traced and confirmed today, not touched.
- `project.pdl`'s `WINDOW` section + `read_pdl_value()` — already built
  (KPI A1).
- `DESKTOP_ACTION:<name>` named dispatch via `route_command()` — already
  confirmed fully modular/order-safe (yesterday's chrome-bar
  investigation), used here for the targeted-invocation handoff only.
- Manager-per-project / init-only-manager-plus-hot-path-ops convention
  — `wra-mana-checklist.txt`.
- Directory-scan-for-markers discovery — conceptually mirrors
  `discover_launcher_projects()`, reimplemented locally.
- Atomic temp+rename writes for `session/state.txt` — same discipline
  used everywhere state gets mutated in this codebase.

## First slice — implemented (2026-07-05)

Built, one adjustment from the original proposal (below), fully
additive except one line:

1. `settings` project exists
   (`projects/wraith-alpha/wraith-projects/settings/`, full
   `project.pdl`/`manager`/`layouts` shape). Its manager scans its own
   directory for `settings_entry.pdl` markers and writes discovered
   entries as `<button href="...">` markup into `manager/state.txt` —
   **verified standalone**: ran the compiled manager directly, it
   correctly found the one real entry (`window-geom`) and produced
   `settings_menu_markup=<button label="Window Geometry" href="..." />`.
2. `window-geom` project exists
   (`projects/wraith-alpha/wraith-projects/settings/window-geom/`, same
   full shape), reached via `href` from `settings.chtpm`.
3. **Scope adjustment, flagged not hidden**: the original proposal said
   step 2 would show "a nav list of currently open windows." Building
   it surfaced a real gap — no existing file exposes *every* open
   window, only the currently *active* one (see the one additive line
   below). Building a full multi-window picker would have meant a
   second, larger change to `write_projection()` (a full window-list
   export), which wasn't run past you. So **this slice reads and
   displays the currently active window's geometry only** — not a
   picker across all open windows yet. That's the honest scope of what
   shipped; the full picker is now a clearly separate next slice (see
   Explicitly Deferred).
4. One additive line, approved before it was made: `write_projection()`
   in `wraith-alpha_manager.c` now also writes
   `desktop_focused_window_project_id=%s` — no existing line touched.
   Without it, `window-geom` had no way to know which project's
   `project.pdl` to read at all.
5. `window-geom`'s own "Back to Settings" `href` is in its layout,
   matching `terminal.chtpm`'s existing pattern exactly.

**Verification done:**
- Both managers compile clean (`gcc -Wall`: exit 0, 0 errors; only the
  same style of pre-existing `snprintf`-truncation notices already
  common throughout this codebase).
- `settings_manager` run standalone: correctly discovered the one real
  `settings_entry.pdl` and produced correct button markup.
- `window-geom_manager` run standalone against the **real**
  `alpha_state.txt`, in its actual current (stale, pre-fix) state:
  correctly fell back to "No active window found" rather than crashing
  or showing garbage.
- `window-geom_manager` run again against a **temporarily modified
  copy** simulating what `alpha_state.txt` will look like once a live
  session re-runs with the new line: correctly showed the active
  window's title/id/project, and correctly reported "not set" for a
  target with no saved `WINDOW` section (confirmed via `diff` that the
  real `alpha_state.txt` was restored byte-for-byte afterward — nothing
  live was left modified).
- **Not yet verified**: an actual real `.chtpm` render inside a running
  Wraith session (need the live app for that) — see "How to reach it
  live," next.

## How to reach it live — not yet decided, needs your call

Both projects work at the file/manager level, but nothing currently
reachable from a live Wraith session points at `settings.chtpm` yet.
`href` navigation only works by clicking *something already on
screen* — and no existing screen has a link to `settings.chtpm` today.
Two ways to actually see this render live:

1. **Add one `href` link somewhere already-reachable** (e.g. a new line
   in `alpha-shell.chtpm`, or `terminal.chtpm`, mirroring their existing
   "Back to Wraith Desktop" convention in reverse) — small, additive,
   but does touch a live, currently-used layout file, so flagging
   rather than just doing it.
2. **Launch `wraith_parser_alpha.c` directly** with
   `projects/wraith-alpha/wraith-projects/settings/layouts/settings.chtpm`
   as its startup argument, outside the normal boot sequence — a
   pure test/dev invocation, zero files touched, but not how a real
   user would ever reach it.

Note separately: `settings` (not `window-geom`, since it's nested)
would *also* already show up today in the existing
`discover_launcher_projects()` launcher-row list (it's a real,
non-nested project with a `project.pdl`, so that already-generic
scanner picks it up for free) — but opening it *that* way would route
through the desktop-embedded `session/wraith_body.txt` convention,
which this project doesn't use, and would show the existing "Missing
project body file" fallback instead of the real menu. Worth knowing so
it isn't mistaken for a bug if tried.

## Targeted-vs-generic discovery — implemented (2026-07-05, same day)

Added the piece originally deferred: `window-geom` now reads
`session/wg_target.txt` (a single line, a `project_id`) before falling
back to the active-window read.

- **File present with content** → "opened from a project" (what a
  future chrome-bar button would set up by writing this file before
  navigating here) → uses that `project_id` directly, skips
  `alpha_state.txt` entirely, shows `Source: targeted (session/wg_target.txt)`.
- **File absent/empty** → "opened from settings" (the generic path) →
  falls back to the active-window read exactly as before, shows
  `Source: active window (opened via settings)`.

This is also the manual test hook: writing a `project_id` into that
file by hand (`echo "wraith-alpha/wraith-projects/terminal" > .../session/wg_target.txt`)
simulates the targeted path completely, without needing the chrome-bar
button itself built yet.

**Verified standalone, all three cases:**
1. No target file, no active-window data available →
   "No target set and no active window found..."
2. Target file set to `wraith-alpha/wraith-projects/terminal` →
   correct `Source: targeted (...)` line, correct project, correct
   "not set" geometry (terminal has no saved `WINDOW` section).
3. No target file, active-window data simulated (same technique as
   before — temp-modified copy, `diff`-confirmed restored afterward) →
   correct `Source: active window (...)`, correct `Window: ...` title
   line, correct project/geometry.
4. Test target file removed afterward — nothing left behind for a real
   test run to trip over.

## Explicitly deferred, not this round

- Actual editing (`<cli_io>` fields) and save-back to `project.pdl`.
- The chrome-bar `[i]` button / `DESKTOP_ACTION:open_window_geom:<id>`
  targeted path (needs the plain generic path proven first).
- Arrow-key nudges / mouse drag on the target window directly (that's
  `todo-j5.txt`'s Path 1, still separate future work).
- A second real settings entry (proves the discovery mechanism holds up
  with >1 item) — worth doing once this slice works, not bundled in.

## Open questions

- Exact marker-file format for a settings entry (a small
  `settings_entry.pdl` with a `label=`/`entry_layout=` pair is the
  simplest thing that would work — worth confirming before
  implementation, not a hard blocker).
- Whether `window-geom`'s own picker should exclude `settings` itself
  from the pickable list (editing settings' own geometry from within
  itself is a real edge case worth deciding on purpose).
- Whether `settings` needs its own entry point in the main desktop
  right now, or whether it's fine to reach it only via a temporary
  test `href` for this first slice, adding a permanent entry point
  later.
