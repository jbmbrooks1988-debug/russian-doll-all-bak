# LIVEDESK SESSIONS + DESKS — WORKLOG (2026-08-09)

Working through the sessions/desks feature (K9 + K10). Full design:
`livedesk-editor-design.md`. This file is the resume-point checklist —
read it first if the session was interrupted.

## DONE before this pass
- C1 "USER: jb" fix (double-encode bug) — taskbar rebuilt, verified.
- Schema cleanup EXECUTED (§9.3): user homes now ONLY at
  `<house>/xyzfs/users/<uuid>/home/`; login/avatar ops + tsc_elo/yahoo
  fallbacks re-pointed; stale guest identity deleted; jb's avatar recreated
  (brown skin) and shows in the taskbar USER cell as an MC-style sprite.
- Backup of the pre-migration schema: `/tmp/opencode/schema-backup-1786340314/`.

## THIS PASS — K9/K10 session + desk scaffold (in-progress)

### Code landed in `&.widgits/livedesk-taskbar/ops/tp_taskbar.c`
- New `livedesk:*` reserved commands, dispatched IN-PROCESS by
  `livedesk_dispatch()` (no shell-out). Wired into:
  - `run_popup_row()` (now takes `GC gc`) — File submenu rows.
  - `open_cell_popup()` — the no-submenu `desks` button.
- `#.desktop/livedesk_taskbar.pdl` now sets the cmd rows:
  - file > new-desk = `livedesk:new-desk`
  - file > save     = `livedesk:save`
  - file > save-as  = `livedesk:save-as`
  - file > load     = `livedesk:load`
  - desks button    = `livedesk:desks`
  (same defaults hard-coded in `load_strip_config()` as fallback).
- New pure-logic block (after `active_avatar_dir()`): storage at
  `<house>/xyzfs/users/<uuid>/home/livedesk/sessions/`:
  - `sessions/session.pdl` — `STATE|active_session`, `STATE|last_session`.
  - `sessions/<id>/session.pdl` — `STATE|name`, `STATE|active_desk`.
  - `sessions/<id>/desks/<desk>.pdl` — rows
    `DESK | entity | path | x | y | grid_x | grid_y | glyph | index`.
  - Functions: user_uuid (emoji-free walk via `0.user-pal*` glob),
    sessions_root, root pdl read/write, ensure_session, next_id,
    set_name, session_name, active_desk read/write, desk_list,
    next_desk, read_open (livedesk_open.txt), glyph/pos readers,
    snapshot_desk (Q9 File->save / auto-save-on-switch), close_all
    (CLOSE relay to every live tab + 450ms nanosleep), spawn_desk
    (rewrite desktop_pos.txt then `setsid nohup
    <tile-picker>/ops/+x/tp_desktop_window.+x '<path>' &`),
    default_session (auto-create `s1` "pre-design" snapshotted from the
    CURRENT live desktop), switch_desk, load_session, new_session,
    new_desk (empty desk_0N + switch = Q6), save, save_as (cp -r clone
    to next id, name "session<N>"), and the dynamic popups for
    session list / desk list (reuse strip-popup machinery via
    `g_livedesk_dyn` + `livedesk_open_dyn_popup()`).

### Flow semantics implemented
- File->save: snapshot live desktop into active session's active desk.
- File->save-as: clone active session dir to next id `s<N>` named
  "session<N>", snapshot current desk, switch.
- File->load: dynamic popup listing sessions by display name; choosing
  one = snapshot outgoing session, close all, spawn its active desk,
  update root session.pdl (active/last).
- File->new: new empty session (next `s<N>`, name "session<N>"),
  desk_01 empty, switch.
- File->new-desk: empty desk_0N in current session + switch.
- desks button: popup listing each desk of the active session +
  "+new-desk" + "cancel" (K4).
- First livedesk action with no active session auto-creates `s1`
  "pre-design" containing a snapshot of the current live desktop.

### Build status
- `gcc -std=c11 -Wall -O2 ops/tp_taskbar.c -o ops/+x/tp_taskbar.+x -lX11`
  compiles EXIT=0. Only warnings: pre-existing `-Wformat-truncation`
  noise (PATH_BUF 4352) + now-fixed `ent` unused var.
- NOT yet runtime-tested.

## NEXT (resume here)
1. Rebuild taskbar binary in place + restart (pkill taskbar; entity
   `ensure_taskbar_running()` relaunches it — verify via
   `ps`/`/proc/<pid>/cmdline`).
2. Runtime-test File/desks flows with xdotool/xte clicks; verify files
   under `<house>/xyzfs/users/0a9558a7-…/home/livedesk/sessions/`.
3. Update `livedesk-editor-design.md` §5 step-4/5 status.
4. LATER: C5 per-session entity data (FULL copies of state.txt/
   inventory/event_pkg), then C2b polish + C3 default-open bootstrap.

## KNOWN RESERVED COMMAND SET
```
livedesk:new        livedesk:new-desk    livedesk:save
livedesk:save-as    livedesk:load        livedesk:desks
livedesk:open-session:<id>
livedesk:switch-desk:<id>/<desk>
```

## CRITICAL ENV FACTS
- jb uuid = `0a9558a7-7c74-4358-833c-2d5b21edc421`.
- GRID_CELL_PX = 80 (`LIVEDESK_GRID_PX` in taskbar mirrors it).
- Entity spawn binary: `<house>/&.widgits/tile-picker/ops/+x/
  tp_desktop_window.+x <package_dir>`.
- Entity positions live in `<package_dir>/desktop_pos.txt` (x= / y=),
  written live on drag-end (Q9), read by `read_initial_pos()` at spawn.
- Registry: `<house>/#.desktop/livedesk_open.txt` rows
  `PID=|INDEX=|ENTITY=|PATH=`. Entities re-derive INDEX from the master
  ledger on spawn, so desk pdl INDEX is informational only.
- Relaunch rule: `setsid nohup CMD </dev/null >/dev/null 2>&1 & disown`
  in a minimal command — never combine pkill+sleep+launch+inspect in
  one bash call (hangs the tool).
