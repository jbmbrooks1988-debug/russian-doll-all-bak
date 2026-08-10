# #.livedesk-editor-design.md — Livedesk → RPG-Maker-style Session/Desk Editor

House: 44.xyz…00.17  |  Date: 2026-08-09  |  Status: DESIGN — Q1–Q10 RESOLVED, ready to implement

## 0. Goal

Make the livedesk behave like an RPG Maker project editor:

1. **File menu (strip btn 3) creates/saves/loads "sessions".**
   A session = one RPG-Maker-style "project" (its own set of desks, and its
   own per-entity data — events, inventory, HP, positions).
2. **Sessions live in the logged-in user's folder**, not in the house preset.
   The state that opens on start/reset is **not** a preset — it opens because
   the user's storage has a **default open script/session**.
3. **Desks** populate dynamically under the **desks tab (strip btn 4)** with a
   constant `+new-desk` row and `cancel`. A **desk is what shows on the desktop
   screen** (which entities are open + where), and a session/project can have
   **many desks**.
4. Each session's entities can carry **different associated data** (events,
   inventory, hp, etc.) than the same entity in another session.

This doc records: what is correct in the current code, what must change, the
current technical state of the header, the recommended order of work, the
user-testable KPIs, and the questions I will grill you with.

---

## 1. Investigation: who coordinates what today (verified, 2026-08-09)

### 1.1 Identity chain (login → user → per-user storage) — WORKS

Single source of truth: `0.user-pal👤️/00.login-signup/current_login.txt`

```
current_user_id=jb
current_user_uuid=0a9558a7-7c74-4358-833c-2d5b21edc421
current_xyzfs=xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421
```

- `userpal_whoami.+x` (0.user-pal…/00.login-signup/ops/) reads it → prints `jb …`.
- `xyzfs/session.pdl` (same dir) mirrors mode/user_id/uuid/xyzfs_path.
- Apps already resolve per-user storage via the **current_xyzfs** key:
  - `@.apps/TSC_ELO/ops/ledger_append.c` → `<house>/<current_xyzfs>/home/runtime/ledger.txt`
  - `@.apps/TSC_ELO/ops/tsc_elo.c` → `<house>/<current_xyzfs>/home/games/tsc_ratings.txt`
  - `&.widgits/file-menu/house-ledger-arch.md` documents the same resolution chain.
- House-root `xyzfs/users/<uuid>/home/runtime/ledger.txt` already exists for jb.

**Conclusion:** the "user folder of the logged-in user" already exists as a
working convention: `<house>/xyzfs/users/<uuid>/home/`. **This is where
sessions belong.** This is correct and should be reused, not reinvented.

### 1.2 Entity lifecycle (open/register/nav/quit) — WORKS

`&.widgits/tile-picker/ops/tp_desktop_window.c` (LEGACY Linux megafile):

- Opens a package dir, registers in `#.desktop/livedesk_open.txt`
  (`PID|INDEX|ENTITY|PATH`) with flock + dead-PID self-healing.
- `ensure_livedesk_index()` assigns persistent INDEX via
  `#.desktop/livedesk_master_ledger.txt` + `livedesk_next_index.txt`, and
  writes per-entity `livedesk_index.txt`.
- Per-entity `desktop_pos.txt` (x=,y=) — the one deliberate live state write
  on drag. Grid-snapped (GRID_CELL_PX 80), WIN_PX from meta.pdl footprint.
- Per-entity `interact_relay.txt` — remote command inbox (CLOSE, OPEN_CONTEXT,
  RUN_METHOD, ACTIVATE_NAV, FOCUS_NAV). This is the A-Bus into every entity.
- Context menu = `meta.pdl` METHOD rows (Events, Play, Chat, Ledger, Dir,
  Close, Cancel). Re-read every right-click.

**Conclusion:** loading a session = launching `tp_desktop_window.+x <pkg_dir>`
per entity + reading/restoring positions. Closing = writing CLOSE to each
relay (already done by `quit_and_save_session`). This is correct and reusable.

### 1.3 Taskbar / header — WORKS, fully data-driven

`&.widgits/livedesk-taskbar/ops/tp_taskbar.c` (LEGACY — see §2.4):

- Persistent top-left strip (cells[0]=HQ, cells[1]=USER tag, cells[2..]=
  file, desks, player, db, plugins). `sync_strip_claims` gives buttons fixed
  nav 1..N (KIND=btn); tabs/menu rows claim from the same shared
  `livedesk_nav_claims.txt` pool.
- Buttons/menus are PDL-driven from `#.desktop/livedesk_taskbar.pdl`
  (`strip_btn_N_label/_cmd/_menu_M_label/_cmd`) — no recompile for labels/cmds.
- `run_popup_row`: `"quit"` → quit_and_save_session; any other cmd →
  `setsid nohup …` from house_root (chdir'd at startup, §F-18).
- Digit accumulation now defers activation to Enter (au9-accum-fix applied,
  verified at tp_taskbar.c:2300).

**Conclusion:** the header already has the exact UI plumbing we need
(submenus, nav, cmd execution). We add behavior behind the existing
file/desks menus rather than building new chrome.

### 1.4 Startup — WORKS but is a HOUSE PRESET (the thing to change)

- `$.crypts/autostart.pdl` (STATE|enabled, MOUNT, LAUNCH rows) →
  `$.crypts/ops/+x/crypt_autostart.+x` quits current livedesk then launches
  each LAUNCH row (toolbar, asa&ava, hard-vvar-agent, m1, m8, book-stack).
- `$.crypts/restore-list.txt` + `$.crypts/scrypts/openall/run.sh` are the
  "always open" set, used when restore-last-open=1.
- `button.sh run` → crypt_autostart (or openall).

**Conclusion:** this is the "preset" the user says must stop being the reason
things open. It must become: "read logged-in user's **default open script /
default session** from their xyzfs home; if none, offer/create one."

### 1.5 Save/load guidance that exists (the "fm" widget) — WORKS as reference

`&.widgits/file-menu/` (fm widget):

- `focus.txt` contract: `session_root`, `inbox_path`, `state_path`,
  `status_path`, `kind=text_buffer|game_world`, `live_world`, `saves_root`.
- Verbs: NEW / SAVE / SAVE_AS <path> / LOAD <path> via `fm_enqueue_cmd`.
- Pairing: ledger discovery / menu pick / drag-drop; file browser UI exists.
- `house-ledger-arch.md`: runtime ledger in user xyzfs, ONLINE/OFFLINE rows,
  peers discover each other.

**Conclusion:** this is the house's own precedent for save/load UX and for
"widget commands a project's inbox". Our X11 session layer should follow its
vocabulary (NEW/SAVE/SAVE_AS/LOAD, saves_root) but implemented in C/X11
(tp_taskbar/tp_desktop_window), not GL/PAL. RPG-maker-clone
(`201.rpg-maker-clone/ARCHITECTURE.md`) gives the data-model shape:
`project.pdl` + `maps/` + `events/ev_X_Y/state.txt` + `switches.pdl` —
PDL tables we can echo for `session.pdl` + `desk.pdl`.

### 1.6 Entity per-session data — the gap

Today entity mutable state is **one shared copy per package dir**:
- `#.desktop/entities/asa/state.txt` → `name,type,glyph,hunger,hp,created_at`
  (the `hp=100`, `hunger=0` the user mentioned).
- `@.apps/asa-&-ava/pieces/asa/state.txt` + `inventory/` + `event_pkg/`
  (events/pages) + `master_ledger.txt` + `history.txt`.
- `desktop_pos.txt` is written live on drag into the same shared package.

So today there is exactly ONE asa with ONE set of HP/events/inventory across
the whole house. "Entities per session have different data" does not exist yet.
**This is the largest design decision** (§4.5 Q1).

---

## 2. What is correct (keep) vs. what will change

### 2.1 Correct / keep

| Area | Keep |
|---|---|
| Identity | `current_login.txt` as whoami source; `userpal_whoami`; `current_xyzfs` resolution. |
| Storage root | `<house>/xyzfs/users/<uuid>/home/` as per-user home. |
| Entity open/close | tp_desktop_window package model; livedesk_open.txt registry; flock; PID self-healing. |
| Nav | Shared livedesk_nav_claims.txt pool; digit-accum/Enter-only. |
| Header | PDL-driven strip; HQ/user/file/desks/player/db/plugins layout; submenu popups. |
| Quit | `quit_and_save_session` (CLOSE relays + pid unlink). |
| Save/load vocab | fm's NEW/SAVE/SAVE_AS/LOAD + focus.txt/saves_root; rpg-mkr's project.pdl shape. |
| Startup reboot | crypt_autostart's quit-then-launch machinery (reuse for "switch session"). |

### 2.2 Will change

| # | Change | Why |
|---|---|---|
| C1 | **Fix "guest" bug** in tp_taskbar.c:1850 (double-encoded emoji path breaks whoami popen). | Real, verified root cause of "USER: guest" while jb is logged in. |
| C2 | **Sessions**: new `sessions/` layer under user home; File menu items become real ops. | No session concept exists today. |
| C3 | **Default-open**: autostart.pdl/openall defer to user's default session/script. | Today the house preset decides; user says storage must decide. |
| C4 | **Desks**: desk registry per session; desks tab lists desks + `+new-desk` + cancel; switching desk re-renders desktop. | No desk concept exists; "desks" button is a no-op. |
| C5 | **Per-session entity data**: FULL-copy model (Q1) — each session snapshots/restores its own state.txt/inventory/event_pkg/desktop_pos; compress/decompress scripts deferred to shipping. | Entities are single-copy today. |
| C6 | **User button**: wire strip_user_cmd to a real user-switcher (userpal login) OR make the tag show the live user + session. | Button is a no-op today; sessions are per-user. |
| C7 | **Where new design lives**: KHTPM-ARCH.txt says tp_taskbar.c/tp_desktop_window.c are LEGACY; new logic should land in `khtpm_core.c`. Strip port must land first. | Prevents re-adding design to LEGACY files (see §2.4 for the trap). |

### 2.3 The one confirmed bug today (C1), evidence

Source `tp_taskbar.c:1850` builds the whoami path with bytes:
`c3 b0 c5 b8 e2 80 98 c2 a4 c3 af c2 b8 c2 8f` (double-encoded UTF-8 👤️),
but the real dir is `f0 9f 91 a4 ef b8 8f`. Result: `popen` opens a path that
does not exist → no output → `username` stays `"guest"`. Verified identical
bytes in the running binary (`ops/+x/tp_taskbar.+x`). `userpal_whoami.+x`
called directly with the correct root prints `jb`. Fix = correct byte
encoding + rebuild + relaunch.

### 2.4 LEGACY architecture trap (must decide before writing code)

`&.widgits/tile-picker/ops/KHTPM-ARCH.txt`:
- `tp_desktop_window.c` and `tp_taskbar.c` = LEGACY Linux megafiles.
- New design belongs in `khtpm_core.c` + `khtpm_plat_x11.c` (+ taskbar
  core/plat equivalents).
- **But:** `ops/build_khtpm.sh` currently overwrites the running taskbar
  binary with one that has **zero strip-button support** (`grep -n strip
  khtpm_taskbar_core.c` is empty). The README explicitly warns: "Do not run
  that script until the strip feature is ported into the khtpm core."

Two viable strategies for this task (grill question Q5):
- **A. Stay LEGACY for now**: implement sessions/desks in the LEGACY files,
  keep them building/running on Linux, and port to khtpm core in a later
  dedicated pass. Fastest path to user-testable KPIs.
- **B. Port first**: add strip support to khtpm_taskbar_core, then implement
  sessions/desks in core. Correct long-term, but a big up-front cost with no
  user-visible KPI for weeks.

**RESOLVED (Q5, 2026-08-09): A — stay LEGACY.** Implement in
tp_taskbar.c / tp_desktop_window.c, keeping a hard rule: all new pure-logic
(session schema, desk registry, snapshot format) is written as self-contained
functions that later port cleanly into khtpm_core (no X11 calls inside them).
Do NOT run build_khtpm.sh until the strip is ported into the core.

---

## 3. Current technical state of the header (the strip), as-built

Order of cells (livedesk_taskbar.pdl): `HQ | USER:<name> | file | desks |
player | db | plugins`.

- **file** (btn[0]): submenu = `new-desk`, `save`, `save-as`, `load` — all
  four **cmds are empty** today → clicking any row is a no-op
  (run_popup_row ignores empty commands).
- **desks** (btn[1]): no submenu (`n_menu=0`), no cmd → clicking is a no-op.
- **player** (btn[2]): submenu `play`,`pause`,`reset` — all empty cmds.
- **db / plugins** (btn[3..4]): nothing.
- **USER** tag (cell[1]): `cmd = strip_user_cmd` (empty) → click is a no-op;
  label shows `USER: guest` today because of C1.
- Nav numbers: strip buttons = fixed 1..N (KIND=btn); tabs + menu rows share
  the same live pool above N; digit typing + Enter activates (already fixed).
- Popup rows claim KIND=row numbers while open, released on close.

Everything a session editor needs UI-wise already exists here. The work is
**behavior behind existing labels**, not new windows.

---

## 4. Session/desk data model (RESOLVED by Q&A, 2026-08-09)

### 4.1 Roles (Q2)

- **Session = RPG Maker "project"**: owns the overarching data — DB data,
  common events, plugins — that applies to all of its desks/maps.
- **Desk = RPG Maker "map"**: its own tiles + events position set (which
  entities are on screen + where). A session/project has many desks.

### 4.2 Location (Q3) — per-user, mirrors the widget `pieces/sessions` spirit

```
<house>/xyzfs/users/<uuid>/home/
├── runtime/ledger.txt                    # existing runtime ledger (keep)
└── livedesk/                             # livedesk's per-user storage root
    └── sessions/
        ├── session.pdl                   # WHICH session is default + last
        │     STATE | active_session  | <id>
        │     STATE | default_script  | <rel path>   # Q4 data pointer
        │     STATE | last_session    | <id>
        ├── <session-id>/                 # one FULL copy per saved session (Q1,Q10)
        │     ├── session.pdl             # STATE | name | <display name>
        │     ├── desks/
        │     │   ├── desk_01.pdl         # DESK rows: entity_id,path,x,y,
        │     │   │                       #   grid_x,grid_y,glyph,index
        │     │   └── desk_02.pdl
        │     └── entities/               # per-session FULL entity data (Q1)
        │         └── asa/                #   state.txt, inventory/, event_pkg/,
        │                                 #   pos — full copy of the package
        └── <user-saved-id>/              # File→save-as → new dirs
```

Note: the earlier `home/sessions/livedesk/sessions/` was invented — verified
(grep) that no such parent-sessions structure exists. The real house
convention is `<app>/pieces/sessions/<id>/` = one **full** copy per session
(e.g. `@.apps/piececraft-xyz/pieces/sessions/<ts>/`). We mirror that spirit
per-user at `home/livedesk/sessions/<id>/`.

### 4.3 Position storage (Q7)

Livedesk stores pixels itself (`x=`,`y=`, grid-snapped like `desktop_pos.txt`
today); other layers draw from "master data" of that sort — not their own
pixel copies. `grid_x`/`grid_y` kept for the editor.

### 4.4 Default open (Q4)

- Short-term: the **current desk** opens by default; you can name/save it
  into a session.
- Long-term: **last project + last desk**. **Last save wins** — saving a new
  session makes that session what opens thereafter, until changed.
- autostart.pdl shrinks to: ensure logged-in user → launch taskbar → run the
  user's default session/desk via the data pointer (no house preset).

### 4.5 Per-session entity data (Q1) — FULL copies now, delta on ship

Each session owns a **full copy** of its entity data (state.txt, inventory/,
event_pkg/, positions) — stored **decompressed**, matching the existing
`pieces/sessions/<id>/` house convention. No overlay/delta layer while
building.

Shipping-stage optimization (later, not now): a **compress** script creates a
**delta** of a session (space-efficient form); a **decompress** script expands
a delta back to a full session. During development we use full copies only;
delta packaging arrives when the desktop ships.

### 4.6 Save/load/desk semantics (Q6, Q9, Q10)

- **File→save** writes session entity data (Q9); only position writes live on
  drag (keep today's desktop_pos behavior).
- **`+new-desk`** (Q6) = create an **empty** desk in the current session and
  switch to it; `cancel` just closes the popup.
- **Load list** (Q10) = scan `sessions/` dirs; names from each `session.pdl`
  `STATE|name`. No master index file to keep consistent.
- Session switching = quit-then-launch via crypt_autostart-style teardown.
- File menu verbs: `new` (blank template), `save`, `save-as` (clone to new
  id), `load`.

---

## 5. Order of work (recommended sequence + dependencies)

| Step | Task | Depends | KPI gate |
|---|---|---|---|
| 1 | **C1 fix "guest"** (encode + rebuild taskbar) | — | Taskbar shows `USER: jb` |
| 2 | C6a minimal: user button reflects live user + opens a 1-row session picker | 1 | Click USER → see "jb / guest" correct |
| 3 | C3: default-open reads user's xyzfs `session.pdl`; migrate autostart.pdl → bootstrap | 1 | Reset opens what user's storage says, not preset |
| 4 | C2a: session scaffold — `home/livedesk/sessions/` dirs, save/load-as of a **desk snapshot** (positions only) | 3 | File→save/load round-trips positions |
| 5 | C4: desk registry + `desks` tab popup (`+new-desk`, cancel, list) + desk switching | 4 | Make 2 desks, switch, positions change |
| 6 | C5: per-session entity data — **FULL copies** model (Q1) + snapshot/restore state.txt/inventory/event_pkg | 4 | Change hp in session A, session B still old |
| 7 | C5b (shipping-stage, deferred): **compress**→delta / **decompress**→full scripts | 6 | Delta round-trips back to identical full project |
| 8 | C2b: File→new / save / save-as / load full sessions UI | 5 | Full RPG-maker cycle |
| 9 | C6b: user-switcher (userpal login) + per-user default sessions | 8 | Second user has their own sessions |
| 10 | (post-Q5=A) port pure logic to khtpm_core + strip support, if time | — | build_khtpm.sh no longer strips buttons |

---

## 6. User-testable KPIs (each step must be demonstrable by the user)

1. **K1 — Identity**: Top strip shows `USER: jb` (not guest) after C1, with no
   manual env hacks. (FAIL today — verified.)
2. **K2 — Default open**: `button.sh run` after `X.quit` reopens exactly the
   session named in the user's `sessions/session.pdl` — edit that file to
   point at a different desk, reset, and the desktop opens differently.
3. **K3 — File save/load (desk level)**: drag asa to a new grid cell, File→
   save; X.quit; File→load; asa is back at the saved cell. (No data yet, just
   position.)
4. **K4 — Desks tab**: under `desks` there are at least: each desk in the
   session, `+new-desk`, `cancel`. `+new-desk` creates desk_02 and switches
   to an empty desk. Switching back to desk_01 restores the old entity set.
5. **K5 — Per-session data**: open asa in session A, set `hp=42` via a
   method/editor; save; load session B; asa's hp is still the session-B value
   (e.g. 100), not 42.
6. **K6 — Multi-session**: `save-as` "my-jb-save"; load it; it appears as its
   own desk set and its own entity data; the original session is untouched.
7. **K7 — Multi-user**: log in as a second user (userpal), start desktop —
   their `sessions/` is empty/new; jb's sessions are untouched.
8. **K8 — No preset**: deleting `$.crypts/autostart.pdl` LAUNCH rows (or
   setting STATE|enabled|0) does NOT stop the default session from opening.

### User's first KPIs (flows, given 2026-08-09 — the first things to make work)

9. **K9 — Session flow (sanity round-trip)**: File→new project → load the old
   default project (named e.g. "pre-design") → File→new project again → open
   an app or monad → File→save as session2. Load session2 back and it shows
   that app where it was saved.
10. **K10 — Desk flow**: new (blank/clear) → load session2 → create a
   "new desk" → switch between desks → save → clear → load. Desks switch
   cleanly and save/load round-trips.

Order note: K9 exercises steps 4+8 (scaffold + File menu), K10 exercises
step 5 (desk registry) on top of K9. They are the user's acceptance flows,
so implementation order below follows them (C1 → scaffold → File menu →
desks → entity data).

---

## 7. Grill questions (RESOLVED — answers locked in Q&A, 2026-08-09)

**Q1 — Per-session entity data model.** Copy, delta/overlay, or pointer?
**RESOLVED: FULL copies, decompressed.** Each session owns a complete copy of
its entity data (state.txt, inventory/, event_pkg/, positions) — matches the
existing `pieces/sessions/<id>/` convention. Full copies during building;
**compress**→delta / **decompress**→full scripts arrive at shipping stage.

**Q2 — Is a "session" the RPG Maker "project", and a "desk" a saved camera/
view of that project?**
**RESOLVED: yes, and deeper — session = project (owns overarching DB data,
common events, plugins); desk = RPG Maker map (its own tiles + events
position set).** Sessions have many desks.

**Q3 — Where exactly is the user's home for sessions?**
**RESOLVED:** `<house>/xyzfs/users/<uuid>/home/livedesk/sessions/<id>/` —
per-user mirror of the widget `pieces/sessions` spirit. (The earlier
`home/sessions/livedesk/sessions/` was invented; grep-verified that no such
parent-sessions structure exists.)

**Q4 — What is "default open script in user storage"?**
**RESOLVED: short-term = current desk opens by default; long-term = last
project + last desk; saving a new session makes it the default (last save
wins) until changed.** Data pointer over code; `open.sh` allowed but
optional.

**Q5 — LEGACY vs khtpm-core (strategy A or B)?**
**RESOLVED: A — stay LEGACY.** Implement in tp_taskbar.c/tp_desktop_window.c
as self-contained pure functions; port to khtpm core in a later pass if time.
Do NOT run build_khtpm.sh until the strip is in core.

**Q6 — "new-desk" semantics.**
**RESOLVED: (a) empty new desk in the current session, switch to it; `cancel`
just closes the popup.**

**Q7 — Do desks/sessions persist positions in grid coords or pixels?**
**RESOLVED: livedesk stores pixels itself (x=,y=, grid-snapped like
desktop_pos today); other layers draw from "master data" — not their own
pixel copies.** grid_x/grid_y kept for the editor.

**Q8 — Multi-user scope now or later?**
**RESOLVED: per-user paths are mandatory now (K7); the in-desktop
user-switcher UI is deferred (C6b).**

**Q9 — What triggers a session's entity data write-back?**
**RESOLVED: File→save for session data; only position writes live (keep
today's desktop_pos behavior).**

**Q10 — Where does the "sessions live" list come from for the File→load
popup?**
**RESOLVED: scan `sessions/` dirs; names from each `session.pdl`
`STATE|name`. No master index file.**

---

## 8. Open risks

- **Encoding**: emoji paths are a real failure class (C1). New code must
  never hand-write emoji into source; read names from disk where possible.
- **Restart cost**: every session/desk switch is quit+relaunch of entities
  via relay CLOSE then spawn. If that feels heavy, a lighter "hide/unhide +
  reposition" path exists for desk switches (only spawn truly-new entities).
- **fm/GL vs X11**: fm widget vocab is guidance, not the implementation; do
  not pull the PAL/chtpm stack into the taskbar. C/X11 only.
- **build_khtpm.sh trap**: never run it during this work (it clobbers the
  strip taskbar). See README warning + §2.4.
- **history.txt binary-read pitfall**: entity history files are `data` (may
  contain binary), not line text — snapshot code must treat them opaquely.
- **Compress/decompress (Q1 shipping-stage)**: canonical format is full
  copies during building; compress→delta / decompress→full is deferred until
  the desktop ships. Round-trip equality is the KPI when it lands.
- **Default-open pointer**: K2/K8 depend on `session.pdl` fields
  (`default_script`/`active_session`) being present; until then desktop opens
  the last-saved desk (Q4 "last save wins").

## 9. User-storage schema cleanup (found during avatar-bug investigation)

Investigation of the lost-avatar bug (jb created a brown-skin avatar that
never saved) exposed that the per-user storage schema drifted. Two trees claim
to be "the user home" and the avatar module carries stale guest-mode identity
files. Cleanup scope; the livedesk sessions work must build on the CLEANED
schema.

### 9.1 Drift evidence

- **Two xyzfs trees, same concept.**
  - Tree A (house-root): `<house>/xyzfs/users/<uuid>/home/` — has `runtime/`.
    Used by `ledger_append`, `tsc_elo`, fm. Resolves `current_xyzfs` relative
    to `<house>`.
  - Tree B (login-signup): `<house>/0.user-pal👤️/00.login-signup/xyzfs/
    users/<uuid>/home/` — has `exchange/`, `net/`. Used by userpal ops,
    avatar system, `ledger_peers`. Resolves the SAME `current_xyzfs` string
    relative to `00.login-signup/`.
  - Same relative path `xyzfs/users/<uuid>`, two different absolute roots.
- **Duplicate identity files.** `01.avatar-creation👤️/` holds its own
  `xyzfs/session.pdl` (`mode=guest`, empty user) and `current_login.txt`
  (empty) — stale copies that can silently win when login_root resolution
  falls back to `project_root`.
- **Naive login_root resolution.** Ops use `../00.login-signup` from
  `project_root` (fallback chain ends at `project_root`). If launched from the
  wrong root the resolver reads the stale guest state instead of the live
  session.
- **Owner attribution drift.** The only avatar clone
  (`map_lobby/9bcba485…/state.txt`) is owned by `afx_3263291`, which matches
  no current userpal identity. jb's uuid never references it.

### 9.2 Clean schema (single source of truth)

```
<house>/
├── 0.user-pal👤️/00.login-signup/      ACCOUNT REGISTRY only
│   ├── current_login.txt               whoami (single source)
│   └── xyzfs/session.pdl               session pointer
└── xyzfs/users/<uuid>/home/            THE one per-user home
    ├── runtime/  exchange/  net/       (merged from Tree B)
    ├── livedesk/sessions/              (our sessions)
    └── avatars/                        (avatar truth)
        ├── inventory.txt
        └── <avatar_uuid>/state.txt …
```

- **Rule everywhere:** `current_xyzfs` is ALWAYS relative to `<house>`. No
  login-signup-relative interpretation.
- `00.login-signup/` keeps account registry (user meta.txt, current_login.txt,
  session.pdl) and NO per-user xyzfs tree.
- Avatar module keeps `pieces/world_01/map_lobby/` as a LOCAL working cache;
  `hydrate_avatars` syncs against `<house>/xyzfs/users/<uuid>/home/avatars/`.

### 9.3 Migration — EXECUTED 2026-08-09 (was "proposed, not yet executed")

1. Backed up both trees to `/tmp/opencode/schema-backup-1786340314/`.
2. Union-merged every old `00.login-signup/xyzfs/users/<uuid>/home` (meta.txt,
   home/{net,exchange}, runtime/ledger.txt, non-empty projects/) into
   `<house>/xyzfs/users/<uuid>/home/`.
3. Removed `00.login-signup/xyzfs/users/` (account registry only now). KEPT
   `00.login-signup/xyzfs/session.pdl` — it is the login session pointer.
4. Deleted stale avatar-module guest identity
   (`01.avatar-creation👤️/xyzfs/session.pdl`, its `current_login.txt`).
5. Login ops (`userpal_login.c`, `userpal_create_account.c`) + all 11
   avatar ops now mint/resolve per-user homes at `<house>/xyzfs/users/<uuid>/
   home/` (house_root via `HOUSE_ROOT` env else parent-of-0.user-pal, emoji-free
   upward walk). tsc_elo + both yahoo compose fallbacks re-pointed to
   `<house>/xyzfs/users`.
6. Recreated jb's avatar against the clean schema — persists at
   `<house>/xyzfs/users/0a9558a7-…/home/avatars/<uuid>/` (brown skin = skin_index
   3, black hair), appears on the avatar screen AND the taskbar USER cell.
7. Avatar sprite = full-body MC character synthesized from state.txt DNA
   (`make_avatar_sprite.c`, same pixel layout as avatar_window's
   synthesize_mc_front_sprite), NOT a font emoji — emoji fonts drop
   Fitzpatrick skin tones (base glyph renders default-skinned).

### 9.4 Livedesk coupling

Livedesk sessions (§4) already target `<house>/xyzfs/users/<uuid>/home/
livedesk/sessions/` = Tree A. This cleanup makes that unambiguous and fixes
the environment the sessions work depends on. Do not build sessions on the
login-signup-relative Tree B interpretation.

*End livedesk-editor-design.md*
