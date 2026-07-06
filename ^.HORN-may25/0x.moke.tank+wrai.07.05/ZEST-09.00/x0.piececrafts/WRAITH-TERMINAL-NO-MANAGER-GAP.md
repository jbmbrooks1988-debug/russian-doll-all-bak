# Gap: the "terminal" Wraith project has no manager

**Status: confirmed gap, not yet fixed.** Found 2026-07-03 while auditing
which Wraith-hosted projects actually have a manager process behind them.

## What exists

`projects/wraith-alpha/wraith-projects/terminal/` has:
- `project.pdl`
- `layouts/terminal.chtpm`
- `README.md`

That's it. No `manager/`, no `ops/`, no `session/` directory — unlike every
other built-out wraith-project (`web-cam`, `piececraft-wraith`,
`wraith-3d-cube`, `screen-record`, `fs`, `chtmgl-wraith`,
`chtmgl-video-isolate`, `wraith-ed`, `wraith-browser`, `wraith-man-test`),
which all have the full `manager/ops/session` triad and a `META | manager |
...` line in their `project.pdl`.

`terminal/project.pdl` has no `manager=` key at all.

## Why this matters (the actual runtime effect)

`wraith-alpha_manager.c`'s launch path is:

1. `launch_window_instance()` — creates the `Window` entry (title, taskbar
   slot, registry state). This always succeeds regardless of whether a
   manager exists; it's just bookkeeping.
2. `ensure_project_manager()` → `launch_project_manager()` — reads the
   project's `project.pdl` looking for a `module_path`/`manager` line,
   resolves it to a path, and only forks/execs it if that path exists and
   is executable (`access(manager_path, X_OK)`).

Step 2 no-ops silently (`if (manager_path[0] == '\0') return;`) when the
key is missing. No error is logged, no fallback fires.

**Correction from an earlier draft of this doc:** this does NOT mean the
window is "non-functional." Opening it genuinely works, because
`terminal.chtpm` (the layout) is a static panel — an ASCII banner, some
info text, and two buttons: "Open Another Terminal"
(`onClick="DESKTOP_ACTION:launch_terminal"`) and "Back to Wraith Desktop"
(`href=...`). Both of those actions are handled generically by the core
Wraith desktop chrome itself (`DESKTOP_ACTION` routing in
`wraith-alpha_manager.c`, `href` layout navigation in
`wraith_parser_alpha.c`) — neither requires a project-specific manager or
`ops/` at all. So the window opening, rendering, and both buttons work
correctly and always have.

**What's actually missing** is any real shell/command-execution backend.
The layout's `<interact src="projects/wraith-alpha/session/history.txt" />`
line just taps into the same generic top-level keystroke-history file
every interact-mode window uses (also written by `wraith_parser_alpha.c`)
— it is not a project-specific input pipeline, and nothing consumes that
history to actually execute a typed command here. There is no
`ops/+x/wraith_project_input.+x` for this project (so
`run_active_project_input_op()` silently no-ops), no manager process, and
no code path anywhere that runs a program or returns output. So: a
working static info panel with two working navigation buttons, not yet an
actual interactive terminal.

This is exactly the ambiguity that prompted the question that produced
this doc: two taskbar entries ("Terminal" and "WRAITH TERMINAL") both
looked like real, working windows in a screenshot — and they genuinely
are working windows (see correction above), which is what made "it has no
manager" sound wrong at first. Both resolve to the same project, and
neither has a manager backing actual command execution.

**They are not two things — confirmed by tracing the exact call path.**
`discover_launcher_projects()` auto-discovers `terminal/` (any
`wraith-projects/*` dir with a `project.pdl` becomes a `WraithLauncher`)
and builds one record: `command="DESKTOP_ACTION:launch_terminal"`,
`project_id="wraith-alpha/wraith-projects/terminal"`,
`title_prefix="WRAITH TERMINAL"` (from `project.pdl`'s `STATE | title`).
The toolbar's fixed "Terminal" button (`find_terminal_launcher()`) just
looks up that *same* record by `dir_name=="terminal"` and reuses its
`.command` string — which is identical either way. Both the toolbar
button and any other route into "terminal" converge on the same
`route_command() -> launch_wraith_project_command() ->
find_launcher_by_command() -> launch_window_instance("terminal",
"WRAITH TERMINAL", "wraith-alpha/wraith-projects/terminal")` call. There
is no separate real-shell implementation elsewhere that the toolbar
button falls back to instead — grepped the manager for
`xterm`/`forkpty`/`openpty`/`/bin/sh` and found nothing. So the toolbar
shortcut is a fixed-position alias into this exact same project (same
static panel, same missing shell backend), not a different, more
functional terminal that happens to share a name.

## What the README already says the intent was

`terminal/README.md`:
> Contract: both the toolbar terminal and the shell terminal action should
> resolve to this same project-backed context

So this was scaffolded on purpose (hence `project.pdl` + layout existing
at all) with an explicit intent to back it with a real project context —
just never finished. This is a stub, not an oversight in the sense of
"forgot it existed."

## What finishing it would need (not yet started)

To match the shape of any other wraith-project:
1. `projects/wraith-alpha/wraith-projects/terminal/manager/terminal_manager.c`
   — a manager process, presumably one that runs an actual shell (fork/exec
   a `/bin/sh` or similar, pipe I/O) rather than rendering a fixed layout.
2. `projects/wraith-alpha/wraith-projects/terminal/ops/` — whatever
   input-forwarding op the manager needs (mirrors how other projects handle
   `KEY_PRESSED`/history via their own `ops/src/*_input.c`).
3. `projects/wraith-alpha/wraith-projects/terminal/session/` —
   `state.txt`/`history.txt`/`.manager.lock`, same convention as every
   sibling project.
4. Add `META | manager | projects/wraith-alpha/wraith-projects/terminal/manager/+x/terminal_manager.+x`
   to `project.pdl`.
5. Compile the manager binary into `manager/+x/terminal_manager.+x`
   (matches the `+x`-binary convention used everywhere else in this repo).

Not attempted yet — this doc exists to record the finding so it isn't
rediscovered from scratch later, per the standing "document gaps before
building" convention already used for the ray-marching pivot (see
`ARCHITECTURE-RGB-RENDERING.md`).
