# oc-linux-fix — Linux compatibility restore after the Win house conversion

**Date:** 2026-08-07
**House root:** `44.xyz❤️‍🔥️00.11`
**Status:** FIX APPLIED + verified. Kept here as the recurrence/prevention playbook.
**Related:** `16.A7_GROK_WALKOFF.md` (Win handoff), `!.linux-absolute-FIXME-a6.txt`
(path portability), `#.WIN-COMPAT-RULE.md`.

---

## 0. TL;DR

The Win conversion pass broke Linux in **two** independent ways, not one:

1. **Execute bits stripped house-wide.** ~540 `*.+x` ELF binaries, ~255 `*.sh`
   scripts, and every bare `system/*` ELF binary lost `+x`. Any `.+x`/`system/*`
   that is ELF but not executable will silently fail to spawn.
2. **PE binaries overwrote Linux ELFs under `*.+x` names.** During "donate PE"
   the Win binaries were copied to the `ops/+x/*.+x` path instead of a sibling
   `.exe`, clobbering the Linux ELF. 54 such files (fixed, none remain outside
   ephemeral `pieces/sessions/`).

Both symptoms the user reported trace to these:

| Symptom | Cause |
|---------|-------|
| `$.crypts` (autostart) "not running" | `tp_taskbar.+x` + `tp_desktop_window.+x` (ELF) had no exec bit → `setsid nohup ... &` failed with permission denied, rc hidden by `>/dev/null` |
| aomorai-editor "viewer widget won't open" | board-viewer `ops/+x/bv_*.+x` + `ledger_*.+x` were **PE** (not runnable on Linux) AND board-viewer `system/*` (prisc+x, gl_mirror, chtpm_rgb_render, renderer, keyboard_input, chtpm_parser_pal) had no exec bit |
| aomorai "maybe not taking keys" | widget path's `gl_mirror`/`keyboard_input` were not executable; host `./system/keyboard_input` itself was fine. Verify after re-run via `pieces/keyboard/history.txt` growth |

It is **not** the relative-vs-absolute path work from `!.linux-absolute-FIXME-a6.txt`
that broke these — `crypt_autostart.c` already resolves house-relative LAUNCH rows
and the PDL is already relative. Paths are fine; exec bits and file formats were the
problem.

---

## 1. Diagnosis commands (evidence)

```bash
# .+x that are ELF but lost exec bit
find . -name '*.+x' -type f ! -perm -u+x | wc -l          # was 539
# .sh that lost exec bit
find . -name '*.sh' -type f ! -perm -u+x | wc -l          # was 255
# PE binaries misnamed as .+x (Win overwrote ELF)
find . -name '*.+x' -type f | while read f; do
  file -b "$f" | head -1
done | grep -c PE32                                   # was 54
# example of a clobbered file
file '&.widgits/board-viewer/ops/+x/bv_compose_frame.+x'   # PE32+ ... for MS Windows
```

Critical files affected by the PE-overwrite:

- `014.wsr-pal💸️📌️+2/ops/+x/*.+x` (37 ops) — rebuilt from `ops/*.c`
- `&.widgits/board-viewer/ops/+x/{bv_compose_frame,bv_menu_input,bv_render_3d,ledger_append,ledger_peers}.+x` — rebuilt
- `&.widgits/event-ez/ops/+x/ez_menu_input.+x` — rebuilt
- `@.apps/aomorai-editor/ops/+x/emoji_xtract.+x`, `@.apps/piececraft-xyz/ops/+x/emoji_xtract.+x` — restored from ELF copy

---

## 2. Fix applied (this pass)

### 2.1 Restore exec bits (idempotent — safe to re-run)

```bash
# every *.+x and *.sh
find . -name '*.+x' -type f -exec chmod +x {} +
find . -name '*.sh' -type f -exec chmod +x {} +
# every ELF under system/ and ops/+x dirs
find . -type d \( -name system -o -name '+x' \) | while read d; do
  for f in "$d"/*; do
    [ -f "$f" ] && case "$(file -b "$f" 2>/dev/null | head -c 3)" in ELF) chmod +x "$f";; esac
  done
done
# every other ELF anywhere (fast magic check, skips noise trees)
find . -type f ! -perm -u+x \
  -not -path '*/pieces/sessions/*' -not -path '*/.claude/*' \
  -not -path '*/xyzfs/*' -not -path '*/#.tmp/*' -not -path '*/#.notes/*' \
  -not -path '*/#.haiku/*' -not -path '*/#.DOX/*' -print0 |
  xargs -0 -P8 -I{} sh -c \
  'm=$(head -c4 "{}" 2>/dev/null | od -An -tx1 | tr -d " \n"); [ "$m" = "7f454c46" ] && chmod +x "{}"'
```

### 2.2 Rebuild PE-clobbered ops from their `.c` sources

```bash
# wsr-pal (canonical donor tree) — ops/*.c → ops/+x/<name>.+x
cd '014.wsr-pal💸️📌️+2'
for src in ops/*.c; do
  name="$(basename "$src" .c)"
  case "$name" in
    emoji_gen_atlas|emoji_xtract) continue ;;   # FreeType dep; ELF exists
    dump_rgb_png) gcc -Wall -Wextra -O2 -I"ops/lib" "$src" -o "ops/+x/$name.+x" -lm ;;
    *)            gcc -Wall -Wextra -O2 "$src" -o "ops/+x/$name.+x" -lm ;;
  esac
done

# board-viewer (rebuilds ops, copies wsr system + emoji, chmod +x)
cd '../&.widgits/board-viewer'
bash scripts/build.sh

# event-ez
cd '../&.widgits/event-ez'
gcc -Wall -Wextra -O2 -o ops/+x/ez_menu_input.+x ops/ez_menu_input.c -lm

# aomorai + piececraft — emoji tools are house-copied, not local-built
# (aomorai/piececraft have no emoji_xtract.c; their build.sh copies from wsr-pal)
cp '014.wsr-pal💸️📌️+2/ops/+x/emoji_gen_atlas.+x' '<proj>/ops/+x/'
cp '014.wsr-pal💸️📌️+2/ops/+x/emoji_xtract.+x'    '<proj>/ops/+x/'
chmod +x '<proj>/ops/+x/emoji_*.+x'
```

> Note: `emoji_gen_atlas.c` needs `ft2build.h` (FreeType dev). Don't rebuild it —
> the working ELF already exists in `014.wsr-pal`, `&.widgits/tile-picker`,
> `&.widgits/board-viewer` (and now aomorai/piececraft via the copy above).

### 2.3 Smoke tests (done, all green)

```bash
bash '$.crypts/button.sh' check                    # OK crypt_autostart + pdl
'$.crypts/ops/+x/crypt_autostart.+x' <abs pdl>     # all 7 LAUNCH rows rc=0
pgrep -af 'tp_taskbar|tp_desktop_window'           # taskbar + entities alive
bash '@.apps/aomorai-editor/button.sh' check       # all system + ops OK
file '&.widgits/board-viewer/ops/+x/*.+x'          # all ELF, all +x
find . -name '*.+x' -type f ! -perm -u+x | wc -l   # 0
# zero PE still misnamed .+x outside ephemeral sessions
```

A full `$.crypts/button.sh run` was executed live during this pass — taskbar and all
entity windows (ava, asa, book-stack, m6, m8, m1) relaunched successfully.

---

## 3. How to prevent recurrence

1. **Win donates must go to `<name>.exe`, never over `<name>.+x`.** The walkoff
   law `.+x PE → stage to temp .exe` was violated during copy: PE landed on the
   `. +x` path and clobbered the ELF. Verify with
   `find . -name '*.+x' -type f | xargs file | grep PE32` after any Win pass.
2. **Don't strip exec bits.** Whatever tool wrote the tree back from Windows
   (zip/7z/cp without perms) dropped `+x`. If you must re-import the tree, re-run
   §2.1 afterwards — it is idempotent and cheap.
3. **Re-verify `$.crypts` after any Win pass:** `bash '$.crypts/button.sh' run`
   must show `launch 'tool-bar' done (rc=0)` etc. rc=0 with a non-executable
   `setsid nohup ... &` is a trap (rc is the exit of `setsid`, not the app).
4. **board-viewer is the shared widget** — after Win work on it, re-run its
   `scripts/build.sh` on Linux so `ops/+x/*. +x` are ELF again before relying on
   any host app's `OPEN_BOARD_WIDGET`.

---

## 4. Still-on-radar (not fixed here, not blocking)

- `pieces/sessions/` snapshots contain stale PE copies of old ops (`event-ez`,
  `board-viewer` smoke sessions). Ephemeral — each run recreates them. Ignore.
- aomorai-editor host keyboard: `system/keyboard_input` is raw-termios over stdin
  and is executable again. If a session still shows no keys, check the session's
  `pieces/keyboard/history.txt` grows `KEY_PRESSED: N`; if not, the terminal isn't
  passing keystrokes (native terminal, not mintty — same rule as the Win notes).
- The stray `44.xyz❤️‍🔥️00.10` livedesk processes seen during the smoke belong to
  the older house tree; not touched by this pass.

---

## 5. App drift fix vs 44.xyz❤️‍🔥️00.10 (diff-and-port pass)

Compared both apps (`@.apps/aomorai-editor`, `@.apps/piececraft-xyz`) and the shared
widget `&.widgits/board-viewer` against the pure pre-Win tree 00.10.

### Drift found and fixed (the only behavioral change)

- **`ops/pc_menu_input.c` (both apps): the Win pass made `CONFIRM_START` /
  `CONFIRM_START_DEBUG` auto-open the board-viewer GL widget** via the new
  `open_board_widget()` helper, so the widget popped up without the user pressing
  "View Board" (that was the "opened from wrong button"). Removed the auto-call and
  restored the 00.10 messages (`World generated (seed N). Game started.` /
  `Debug flat world generated. Game started.`). The widget now only opens via the
  `OPEN_BOARD_WIDGET` menu command. `open_board_widget()` itself is kept (the
  `OPEN_BOARD_WIDGET` handler and the Win `button.ps1` path both use it).
- Rebuilt both apps with `bash scripts/build.sh` (also re-copies the canonical
  `emoji_gen_atlas.+x`/`emoji_xtract.+x` from wsr-pal). Smoke-verified on a scratch
  copy: CONFIRM_START sets the 00.10 message and spawns **no** widget/gl_mirror;
  clock daemon still launches.

### Confirmed NOT drift (byte-identical to 00.10 — keep as-is)

- `&.widgits/board-viewer` widget core: `ops/bv_menu_input.c`, `ops/bv_compose_frame.c`
  (behavior), `ops/bv_render_3d.c` (behavior), `pieces/chtpm/layouts/board_viewer.chtpm`,
  `button.sh`, and the engine ELFs (`gl_mirror`, `prisc+x`, `keyboard_input`,
  `renderer`, `chtpm_parser_pal`) are identical between 00.10 and 00.11. So
  INTERACT-engagement behave exactly as the original — "interact defaults active" is
  not Win-pass drift. The "`0` twice to get to 3D" case was root-caused to the
  hardcoded `render_mode` default and fixed as a **config-driven feature change** —
  see "had to press 0 twice..." below.
- `arrow_config.txt`, `keybinds.txt`, app menu PDLs (incl. the pre-existing
  `OPEN_VIEW_EDITOR` menu row): identical to 00.10.
- `ops/pc_compose_frame.c`, `ops/pc_generate_chunk.c`, `ops/pc_clock_daemon.c`,
  board-viewer `bv_*.c`: Win-pass edits are all `#ifdef _WIN32` shims
  (`host_fopen`, `host_access`, `house_root`/`resolve_host_root`, BOM stripping,
  atomic rename) — no-ops on Linux, required for Win. Keep.
- `host_project_id()` reads `PRISC_PROJECT_ID` env, defaulting to the app's own name
  (aomorai-editor / piececraft-xyz) — matches 00.10's hardcoded scoping when unset.

### "had to press 0 twice to get to 3d mode" — root-caused and fixed (config-driven default)

Root-caused empirically via key injection (`KEY_PRESSED: 48` into
`pieces/keyboard/history.txt`): the widget chain is byte-identical in 00.10/00.11, so
this was **not** house-vs-house drift. The real cause:

- `render_mode` is UNSET on a fresh session → `default_render_mode()` returned 3D
  (hardcoded `has_z_manifest() ? 1 : 0`, and both houses now have
  `pieces/system/board_manifest.txt` `z_count=32`).
- So at open the status line already said "3D mode 4 (bird's-eye)" but the visible
  frame was the 2D emoji grid (3D overlay not up yet) → the **1st `0` press wrote
  `render_mode=0` (2D), a visible no-op**, and only the **2nd** landed on 3D.

The user confirmed the intended design ("start in 3d 3rd person as a default, read
from a config file so it's flexible") — so the defaults are now **config-driven**,
not hardcoded:

- `pieces/system/arrow_config.txt` (both apps) gained two human-editable keys:
  `default_render_mode=1` (start in 3D) and `default_camera_mode=2` (third-person).
- `default_render_mode()` / new `default_camera_mode()` in all three widget ops
  (`bv_menu_input.c`, `bv_compose_frame.c`, `bv_render_3d.c`) read those keys first;
  missing/invalid falls back to the old real defaults (`has_z_manifest()` conditional
  for render_mode — so 2D-only hosts civ-txt/tactics-txt stay 2D unless they opt in —
  and bare `2` third-person for camera_mode; was `4` bird's-eye).
- **Enter is NOT a 3D toggle** (final confirmation): on Linux `keyboard_input.c`
  relays Enter as raw `13` (the 13→10 remap is `_WIN32`-only), and `bv_menu_input.c`
  has no Enter case — it falls through to the possession-gated keybind block (no-op
  unless possessing hero_01). The earlier "Enter → view 4" report was the
  **fallback-path artifact**: with config missing/invalid the old camera fallback was
  `4` bird's-eye, surfaced by a re-render, not by Enter itself. That fallback is now
  `2` (third-person) everywhere, so "view 4" only appears via an explicit `4` press.
- Rebuilt the widget (`&.widgits/board-viewer/scripts/build.sh`, "build ok").
  Verified matrix with the rebuilt binaries: config present (aomorai-editor) →
  **"Camera: 3D mode 2 (third-person)"**; 3D host with config stripped (z-manifest
  only) → same; 2D-only host (board, no manifest/config) → still **"2D flat"**
  (regression-safe); Enter(13) → no change; `0` → 2D, `0` again → 3D mode 2; `4` →
  3D mode 4 (explicit). No state key is written at open — defaults are read, never
  persisted.

### "still starts as view 4" — the real leftover (fresh cam_pitch was -90)

Even with `default_camera_mode=2`, a **fresh** session still rendered top-down:
`bv_compose_frame.c` and `bv_render_3d.c` both defaulted `cam_pitch` to **-90**
(straight down) in *every* mode, so mode 2's label said "third-person" while the
GL view was visually identical to bird's-eye "view 4". (This is what the 02:56
ledger session showed — it ran before this fix was built.) Real fix, consistent
with the existing `'1'-'4'` switch and `'f'` reset handlers:

- Fresh `cam_pitch` default is now **mode-consistent**: `6` for modes 1/2
  (third/first-person near-level, mode 2 adds its own `tp_look_down_deg` tilt onto
  it), `-90` for free-roam 3 / bird's-eye 4.
- Applied to `bv_render_3d.c` (the actual rendered camera) and
  `bv_compose_frame.c` (the status line), rebuilt. Fresh sessions now read
  **"Camera: 3D mode 2 (third-person) yaw=180 pitch=6"** with config present AND
  with config stripped (z-manifest only). `4` still switches to pitch -90 when
  explicitly pressed; `f`/`2` reset to pitch 6 unchanged.
- The old "brief starts-2D flash while the overlay lands" was NOT by design —
  root-caused and fixed in the next section (PAL pass order). Fresh sessions now
  open directly on the 3D third-person frame.

### Book-stack bible-verse popup clipped at the right edge

`tp_desktop_window.c`'s SHOW_TEXT_FILE popup sized itself `max_w * 7 + 24` — a
hardcoded **7px/char guess** — but the popup actually draws with the 18px fixed
fontset (`-misc-fixed-...-c-90-iso10646-1`, **9px/glyph**, CJK/emoji wider), so
long `bible_text` verses overflowed the window and got cut off. Real fix:

- The popup width is now computed from the **real `Xutf8TextExtents`** of the
  widest line (`popup_text_px`, the same metric the drawing uses), + padding.
- Rebuilt `&.widgits/tile-picker` (`ops/+x/tp_desktop_window.+x`, rc=0; the only
  build warnings are pre-existing). Verse lines are `fold -s -w 70`-wrapped by
  `bible_text/run.sh`, so the widest line ≈ 70 cols × 9px = 630px — comfortably
  inside the 900px cap, nothing clipped.
- **STATUS: on hold** — user redirected away from the book window (their
  Chinese font renders fine; font-substitution theories here are moot). Revisit
  only if the user reports the popup clipping again.

### Board-viewer "starts 2D, needs Enter + Enter + '4' to reach 3rd person" — root-caused and fixed (PAL pass order)

On a fresh launch the view opened in **2D** and only reached 3D mode 2 after
interacting. The widget C files are byte-identical to 00.10, so the culprit was
in `pal/main_module.pal`'s first pass. The mechanism:

1. `bv_compose_frame` force-falls-back to the 2D emoji grid whenever
   `rgb_frame_3d_overlay.raw` is missing/short (`bv_compose_frame.c` ~761-773).
2. Pass 1 ran `bv_compose_frame` **before** `bv_render_3d` — so the very first
   compose found no overlay and pinned the view to 2D.
3. The PAL only re-renders when `bv_screen_changed.txt` grows (bumped only by
   real key actions in `bv_menu_input.c`, never by render/compose) or a key
   arrives — so the 2D first frame stuck until the user pressed Enter/'4'.

Real fix (verified by live pass-1 simulation on a fresh session): swap the pass-1
order in `pal/main_module.pal` (and the loop's `render:` block) to run
`bv_render_3d` **first**, then `bv_compose_frame`:

```
bv_render_3d      # writes rgb_frame_3d_overlay.raw (+ receipt)
bv_compose_frame  # now sees the fresh overlay -> 3D mode 2 immediately
hit_frame
```

`bv_render_3d` only reads host/pieces state (never `bv_compose_frame`'s output),
so the swap is safe; it still early-returns when `render_mode=0` (2D is
`chtpm_rgb_render`'s job). Fresh-session result now reads
**"Camera: 3D mode 2 (third-person) yaw=180 pitch=6"** with no key input.

**END oc-linux-fix.md**
