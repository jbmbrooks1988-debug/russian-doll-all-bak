# Symlink elimination plan (full scope: Windows compatibility)

**Status: planning only, nothing changed. Written 2026-08-19 for another agent to execute.**
Direct instructions, same day, two messages:
1. "we don't want to use symlinks, we want to just link to the actual binary. we will move the
   binary to a shared folder if it's not in one."
2. Follow-up, changing the scope: **"we can't have any symlinks for windows compatibility."**

**This second instruction supersedes §0's original scope narrowing below.** Windows (without admin
rights / developer mode enabled) cannot reliably create real symlinks at all — `ln -s`/`symlink()`
either fail outright or silently fall back to a plain file copy depending on the tool, and NTFS
directory junctions (the closest native alternative) are NOT the same thing as a symlink and
weren't asked for. **Read this doc as covering ALL 125 symlinks found in the tree, not just the 35
binary-source ones** — see §6 for the much larger second category this instruction added.

Repo root for everything below (confirmed via direct `find`):
`/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.17/`
(all relative paths below are relative to this root, referred to as `$ROOT`)

## 0. Scope — the full picture, in two categories

A full symlink inventory of the tree found **125 symlinks currently on disk**, in two genuinely
different categories that need genuinely different fixes:

- **Category A (§1-5 below): 35 static, repo-committed symlinks** — source-file symlinks that
  stand in for "the actual binary" not existing in a shared place yet, sitting in the repo at rest
  (found by any `find -type l`, no session needs to be running). Mechanical fix, low risk, no
  design decision needed — see §1-5.
- **Category B (§6 below): symlinks created AT RUNTIME by `button.sh` scripts**, one full set per
  game/app session, under `pieces/sessions/<id>/...`. The 90 non-Category-A symlinks the original
  version of this doc found were just the currently-existing on-disk instances of this pattern —
  the REAL count is much bigger: **40 different `button.sh` files, 342 total `ln -s`/`ln -sf`/
  `ln -sfn` call sites**, each one creating a fresh batch of session-local symlinks every time that
  project's session/game starts. This is a real architectural pattern repeated per-project, not a
  handful of leftover files, and needs a real design decision before an agent starts changing it —
  see §6 for the full inventory and the open options.

**The 35 in scope for Category A**: every project that symlinks one of 3 shared SOURCE files instead of having its
own real, buildable copy — `prisc+x.c` (13×), `chtpm_parser_pal.c` (13×), `chtpm_rgb_render.c` (9×).
Today's shape: 13 different projects each `#include`/compile the SAME symlinked `.c` file and
build their OWN separate binary from it via their own `scripts/build.sh` (confirmed by reading
`101.mutaclsym🧟‍♂️️19.00/scripts/build.sh` lines 16/22/25 — plain `gcc ... "system/prisc+x.c" -o
"system/prisc+x"` etc., once per project). That's 13+ redundant compiles of byte-identical source
producing 13+ redundant identical binaries. Boss wants ONE real compiled binary in a shared folder
that all these projects reference directly, no symlink.

## 1. Full inventory of the 35 in-scope symlinks

All point into `&.widgits/_shared-lib/` (system/ for prisc+x.c and chtpm_parser_pal.c, ops/ for
chtpm_rgb_render.c) — this is already the house's designated "shared source" location, just not a
shared BINARY location yet.

**Projects with `system/prisc+x.c` → symlink** (13):
`300.rtp-xyz`, `101.mutaclsym🧟‍♂️️19.00`, `_BACKUP_101.mutaclsym-old+18.01`, `300.rpg-xyz`,
`@.apps/piececraft-xyz`, `@.apps/yahoo-app`, `@.apps/TSC_ELO`, `@.apps/civ-txt`,
`@.apps/aomorai-editor`, `@.apps/tactics-txt`, `@.apps/my-chara-txt`, `&.widgits/yahoo-chart`,
`&.widgits/yahoo-broker`

**Projects with `system/chtpm_parser_pal.c` → symlink** (13, same project list as above)

**Projects with `system/chtpm_rgb_render.c` → symlink** (9 — narrower list):
`300.rtp-xyz`, `300.rpg-xyz`, `@.apps/yahoo-app`, `@.apps/TSC_ELO`, `@.apps/civ-txt`,
`@.apps/tactics-txt`, `@.apps/my-chara-txt`, `&.widgits/yahoo-chart`, `&.widgits/yahoo-broker`
(NOT `101.mutaclsym🧟‍♂️️19.00`, `_BACKUP_101.mutaclsym-old+18.01`, `@.apps/piececraft-xyz`,
`@.apps/aomorai-editor` — those 4 have a REAL local copy of `chtpm_rgb_render.c`, not a symlink;
out of scope, see §2).

Get the live, authoritative list any time with:
```
find "$ROOT" -type l -name "prisc+x.c" -o -type l -name "chtpm_parser_pal.c" -o -type l -name "chtpm_rgb_render.c"
```

## 2. Real finding: the NON-symlinked copies have already drifted — do not touch them here

Ran a direct diff of every real (non-symlink) `prisc+x.c` in the tree against the canonical
`&.widgits/_shared-lib/system/prisc+x.c`. **Every single one differs** — `014.wsr-pal💸️📌️+2`,
`101.ledger-player-npc-simple+3`, `01.muchi-pals-🥚️-13.01`, `101.lpns+map+4`,
`102.editor-📄️00.00`, `101.mutaclsym🧟‍♂️️+18.0G`, `044.pal-chat-irc👥️+2`,
`002.zoo__🦓️🐒️0000/02.z00-INK.lo.sur]PEN🏟️`, `@.apps/my-biotech`, `@.apps/my-lawyer`,
`*.START_BUTTON`, `045.muchi-pal-agent🤖️+1++`, `041.pal-forum👥️`, `0.user-pal👤️/01.avatar-creation👤️`,
`0.user-pal👤️/00.login-signup`, `@.apps/myne-qrypto/qtc`, `041.pal-chain⛓️` — all diverged from
canonical, presumably each carrying its own project-specific patches over time.

**This is exactly why the symlinked group (§1) is the safe, well-defined scope for this task and
the non-symlinked group is not**: a symlink is byte-identical to canonical by construction, so
consolidating those 35 into one shared binary changes nothing behaviorally. The 17+ real local
copies have genuinely diverged content — collapsing them into a shared binary would silently
discard real per-project patches. **Do not attempt to fold the non-symlinked copies into this
migration.** If Boss wants those unified too later, that's a separate, much more careful
audit-each-diff task — flag it back to Boss, don't guess-merge.

## 3. Why this is safe to do (verified, not assumed)

All 3 shared source files resolve their own project root via `getcwd()`, never via
`argv[0]`/executable location:
- `prisc+x.c` main() (`&.widgits/_shared-lib/system/prisc+x.c` L907): takes `argv[1]` as the
  `.pal` program file path (per-project, passed by caller) — no house-root assumption baked into
  the binary's own location.
- `chtpm_parser_pal.c`'s `resolve_root()` (same dir, L568-579): `getcwd(project_root_path, ...)` —
  purely current-working-directory based.
- `chtpm_rgb_render.c` (`&.widgits/_shared-lib/ops/chtpm_rgb_render.c` L85): same,
  `getcwd(project_root, ...)`.

**This means a single shared binary works correctly for every project AS LONG AS the launcher
`cd`s into that project's own root before exec'ing the shared binary** — exactly the pattern
`run_khtpm_strip.sh` already uses for the taskbar (`(cd "$HOUSE" && ... "$PARSER" "$HOUSE" &)`).
No source changes needed to these 3 files themselves.

## 4. The fix, step by step

### 4a. Build the 3 real binaries once, in a shared bin folder

Recommended location, matching the house's own `+x/` convention for compiled binaries (used
throughout, e.g. `ops/+x/khtpm_strip_parser.+x`):
```
&.widgits/_shared-lib/+x/prisc+x.+x
&.widgits/_shared-lib/+x/chtpm_parser_pal.+x
&.widgits/_shared-lib/+x/chtpm_rgb_render.+x
```
Build once from the canonical sources already sitting in `&.widgits/_shared-lib/{system,ops}/`,
using the SAME gcc flags each project's own `scripts/build.sh` already uses (check e.g.
`101.mutaclsym🧟‍♂️️19.00/scripts/build.sh` lines 16/21-22/25 for the exact flags per binary,
including the documented `-Wno-unused-result -Wno-stringop-truncation` needed for
`chtpm_parser_pal.c`). Confirm all 13 projects' build.sh use identical flags for these 3 binaries
before assuming one shared build command covers all of them — if any differ, that's a real
discovery to report back, not to silently pick one.

### 4b. Remove the 35 symlinks

Delete each `system/prisc+x.c`, `system/chtpm_parser_pal.c`, `system/chtpm_rgb_render.c` symlink
listed in §1 (`rm` the symlink itself, never the target — the target is the real, shared canonical
source and must stay at `&.widgits/_shared-lib/{system,ops}/`).

### 4c. Update each of the 13 projects' `scripts/build.sh`

Remove the 3 `gcc ... "system/prisc+x.c" -o "system/prisc+x"` (and the other two) build steps
entirely for these 13 projects — there is nothing local left to compile once the symlink is gone.

### 4d. Update each of the 13 projects' launcher (`button.sh` or equivalent) to exec the shared binary

Wherever a project currently runs its own local `system/prisc+x`, `system/chtpm_parser_pal`, or
`system/chtpm_rgb_render`, point it at the shared path instead, with the SAME `cd`-into-project-root-first
pattern already proven safe (§3):
```sh
(cd "$PROJECT_ROOT" && "$SHARED_LIB/+x/prisc+x.+x" "$@")
```
Find each project's actual launch site first — grep each project's `button.sh`/manager code for
`system/prisc+x`, `system/chtpm_parser_pal`, `system/chtpm_rgb_render` (as a relative path or via
`fork`/`exec`/`system()` call) before editing; do not assume every project launches these the same
way `run_khtpm_strip.sh` does — some of these are older/legacy projects (`_BACKUP_101.mutaclsym-old+18.01`
is explicitly an archived backup — confirm whether it should be touched at all, or left exactly as
it is, before changing anything there).

### 4e. Verify each of the 13 projects still runs after the change

Same house standard as everywhere else this session: don't trust a clean build, actually launch
and confirm real behavior (per this house's own `run_khtpm_strip.sh` discipline — "never trust a
bare exit code"). At minimum, confirm each project's own `.pal`-driven flow still runs correctly
end to end after pointing at the shared binary.

## 5. Open questions to resolve before/while executing (don't guess — check or ask)

1. Do all 13 projects' `build.sh` actually use identical gcc flags for these 3 binaries? (§4a) —
   confirmed only for `101.mutaclsym🧟‍♂️️19.00` above; the other 12 need the same check.
2. `_BACKUP_101.mutaclsym-old+18.01` is a backup/archive dir — should it participate in this
   migration at all, or be left alone as a frozen snapshot? Flag to Boss if unclear.
3. Confirm no project passes something OTHER than a plain relative path to these binaries in a way
   that assumes the binary's OWN location (e.g. a relative include/data path computed from
   `argv[0]`) — §3's `getcwd()`-only finding covers the 3 files read directly, but double-check
   each project's OWN launcher code doesn't add an argv[0]-relative assumption on top.
4. Once this migration is done, the 17+ genuinely-diverged real local copies (§2) remain as a
   separate, real question: are they meant to be permanently project-specific forks, or drift that
   should eventually be reconciled? Not this task's job to answer — just don't lose track of it.

---

## 6. Category B — the real, bigger scope: runtime session-scaffolding symlinks

**Confirmed via direct grep, not estimated**: `grep -rl "ln -s" --include="*.sh"` finds **40
different `button.sh` files** across the tree; `grep -r "ln -s"` on the same set finds **342 total
call sites** (`ln -s`, `ln -sf`, `ln -sfn`). This pattern is copy-pasted independently into each
project's own `button.sh` — there is no shared/centralized session-creation helper anywhere in the
tree (checked, none found). Every one of the 90 non-Category-A symlinks the earlier scan found on
disk was just a currently-live INSTANCE of this pattern (a session someone actually started) — most
projects that HAVEN'T had a session started recently show zero on-disk symlinks right now even
though their `button.sh` would create the same ~10-18 symlinks the moment they're launched.

### 6a. What the pattern actually does (read directly from `101.mutaclsym🧟‍♂️️19.00/button.sh`)

On session start, `button.sh` creates `$SESSION_DIR` (a fresh per-run directory under
`pieces/sessions/<timestamp>-<pid>/`) and symlinks a whole set of project-root paths INTO it, so
the session directory looks like a self-contained copy of the project without actually copying
anything:
```
ln -s "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh"
ln -s "$SCRIPT_DIR/system"                "$SESSION_DIR/system"
ln -s "$SCRIPT_DIR/ops"                   "$SESSION_DIR/ops"
ln -s "$SCRIPT_DIR/pal"                   "$SESSION_DIR/pal"
ln -s "$SCRIPT_DIR/default_op.txt"        "$SESSION_DIR/default_op.txt"
ln -s "$SCRIPT_DIR/pieces/chtpm"          "$SESSION_DIR/pieces/chtpm"
ln -s "$SCRIPT_DIR/pieces/registry"       "$SESSION_DIR/pieces/registry"
ln -s "$SCRIPT_DIR/projects/mutaclysm/pieces" "$SESSION_DIR/projects/mutaclysm/pieces"
ln -s "$SCRIPT_DIR/data"                  "$SESSION_DIR/data"
```
...plus several more conditional ones later in the same file for board/entity/hero/world state
(`config.txt`, `board.txt`, `entities.txt`, `board_manifest.txt`, `chunks`, `terrain_legend.txt`,
`hero_01`, `world_01`, `xelector_01`, `widget_cmds`, `board_widget_bridge.txt`) — 18 `ln -s...`
call sites in this ONE file alone. The other 39 projects' `button.sh` files follow the same shape
with project-specific path lists.

### 6b. Why this can't be a simple find/replace — real design options, needs a decision

Unlike Category A (§1-5), there's no single mechanical fix here because the symlinks aren't
standing in for "one real thing that should exist in one place" — they exist so each session gets
an ISOLATED view of shared project state without actually duplicating it, which is exactly what
symlinks are good at and copies are bad at. Windows compatibility forces picking a real
alternative; each has a real tradeoff — **do not silently pick one, this is Boss's call**:

- **Option 1 — copy instead of symlink.** Simplest, works everywhere, matches what the fallback
  behavior would be anyway. Real cost: session creation now duplicates real data every time
  (`system`/`ops`/`pal`/`data` dirs, not just small text files) — disk use scales with session
  count, and stale-copy risk appears if the project root's own `system`/`ops` ever change WHILE a
  session's copy is in use (rare today, but a real behavior change from "always current" to
  "snapshot at session-start").
- **Option 2 — no session-local mirror at all; teach the C code a search-path/fallback.** Cleanest
  long-term (removes the whole category of problem), but means editing every binary that currently
  assumes `getcwd()`-relative `system/`, `ops/`, `pal/`, `data/` paths resolve inside the SESSION
  directory — a real, wide-reaching code change across many `khtpm_*`/`chtpm_*` binaries, not a
  script-only fix. Much bigger scope than this doc's Category A work.
- **Option 3 — pass the real project root as an explicit argument/env var instead of relying on a
  session-local mirrored path at all.** Similar in spirit to Option 2 but smaller: keep `getcwd()`
  meaning "session dir" for session-SPECIFIC state, but add one extra path (e.g. `PROJECT_ROOT` env
  var or an argv slot) the binaries consult for the shared, read-mostly pieces (`system/`, `ops/`,
  `pal/`) instead of expecting them symlinked into the session dir. Needs auditing each binary's
  path-resolution code, project by project — same rough size as Option 2 but more incremental
  (could migrate one binary/path at a time rather than all at once).
- **Windows directory junctions were deliberately NOT proposed as Option 4.** `mklink /J` doesn't
  need admin rights and behaves like a directory symlink for most purposes, so it's the closest
  "free" native alternative — but Boss's instruction was "no symlinks," and a junction is still a
  reparse-point-based link, just a different NTFS mechanism. Worth naming to Boss as a possible
  4th option since it might satisfy the underlying goal (portable session isolation) more cheaply
  than 1-3, but do not assume it's acceptable without asking — the whole point of this doc's
  section is not to guess on Boss's behalf here.

**Recommended next step for whoever picks this up**: don't start rewriting 40 `button.sh` files.
Take the 3 options above (plus the junction question) back to Boss first — this is a real
architecture decision (disk cost vs. code-reach vs. incremental migration) that changes how every
session-based project in the house works, not a mechanical cleanup like Category A.

## 7. Small, safe, low-risk fix included in this same sweep: `yahoo-app`'s convenience symlink

Separately, `@.apps/yahoo-app/ops/broker_menu_input.c` (around L208-216) has its own tiny symlink
creator, unrelated to both categories above — a pure convenience shortcut, not shared-binary or
session-scaffolding:
```c
snprintf(link, sizeof(link), "%s/+x", project_root);
snprintf(target, sizeof(target), "%s/ops/+x", project_root);
if (stat(link, &st) != 0 && access(target, F_OK) == 0) symlink(target, link);
```
This creates `<project_root>/+x` as a symlink to `<project_root>/ops/+x`, purely so something can
reference the shorter path. Grepped the rest of `yahoo-app` for any code that actually USES the
shortened `<project_root>/+x` path instead of the real `ops/+x` — found none. **This one looks safe
to just delete outright** (the function and its one call site) rather than replace with anything —
nothing appears to depend on the shortcut existing, only on `ops/+x` directly. Confirm with a grep
across the whole `yahoo-app` tree (not just this one file) before deleting, in case something
outside `ops/` was missed.
