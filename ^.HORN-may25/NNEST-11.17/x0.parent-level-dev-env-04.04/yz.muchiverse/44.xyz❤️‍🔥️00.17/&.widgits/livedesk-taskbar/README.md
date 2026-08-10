# livedesk-taskbar

A persistent, two-part window pair drawn by `ops/tp_taskbar.c`, auto-launched by `tp_desktop_window.c`'s own `ensure_taskbar_running()` the first time any livedesk entity opens. Real singleton check (PID file + `kill(pid,0)` liveness probe) — every entity after the first just adds a tab, never a second instance.

## What it is

- **Top-left command strip** (`strip_win`): HQ, user/guest, then data-driven buttons (file, desks, player, db, plugins by default) — each with its own popup submenu. Position/size configurable, never hardcoded.
- **Bottom bar** (`win`): one tab per currently-open entity, `[ ] N. entity_name`.
- Both windows share **one unified keyboard-navigation cursor** — right-click either bar to arm, arrow keys/digits move a single `[>]` across strip buttons *then* tabs (strip always gets priority on arm), Enter activates. See `nav_focus_apply()`/`nav_focus_step()`.
- All navigable things (strip buttons, tabs, open popup rows) claim numbers from one shared file, `#.desktop/livedesk_nav_claims.txt` — so a tab and a popup row can never show the same number at once. Popup rows *display and accept* local `1..n_menu` numbers while open (see Known Issues), but still claim real numbers from that shared pool for cross-window jump addressing.

## Configuration

All of this lives in `#.desktop/livedesk_taskbar.pdl` (`SECTION | key | value` rows) — no recompile needed:

| Key | Meaning |
|---|---|
| `strip_x_offset` | Strip's x position (screen-absolute) |
| `strip_y_offset` | Strip's y position — defaults to 40 (below GNOME Shell's own native top panel) |
| `strip_user_cmd` | Command run when the user/guest button is clicked |
| `strip_btn_N_label` / `_cmd` | Button N's label/command |
| `strip_btn_N_menu_M_label` / `_cmd` | Button N's submenu row M |
| `hq_label`, `hq_menu_N_*` | The HQ button's own menu |

Theme (`#.desktop/livedesk_theme.pdl`):

| Key | Meaning |
|---|---|
| `bg`, `fg` | Bar colors |
| `opacity` | 0.0–1.0 window opacity via `_NET_WM_WINDOW_OPACITY` (see Known Issues — currently only reliably applies to the bottom bar, not the strip) |

## Build

```
gcc -std=c11 -Wall -O2 ops/tp_taskbar.c -o ops/+x/tp_taskbar.+x -lX11
```

## ⚠️ Architecture status — this file is LEGACY

Per `&.widgits/tile-picker/ops/KHTPM-ARCH.txt`, `ops/tp_taskbar.c` is marked **LEGACY — do not add new design here**. The intended shared architecture (`khtpm_taskbar_core.c` + `khtpm_taskbar_plat_x11.c` + `khtpm_taskbar_main.c`, the same core/plat split the entity window uses via `khtpm_core.c`/`khtpm_plat_x11.c`) currently has **zero strip-button support** — `grep -n strip khtpm_taskbar_core.c` is empty. Running `&.widgits/tile-picker/ops/build_khtpm.sh` will silently overwrite the real running taskbar binary with one missing the entire top strip, since it builds from the khtpm-core files and copies the result over `ops/+x/tp_taskbar.+x`. **Do not run that script until the strip feature is ported into the khtpm core.** Full detail: `yz.muchiverse/a8-cc-++fix.md`.

## Known issues (as of 2026-08-09)

- **Strip window opacity doesn't match the bottom bar's**, despite provably identical `_NET_WM_WINDOW_OPACITY` property values, identical window-creation code (both built by one shared `create_bar_window()`), and identical offscreen-pixmap render pipeline. Not yet root-caused — full investigation, ruled-out theories, and the next diagnostic to try: `yz.muchiverse/opacity-bug-aug9.txt`.
- Popup keyboard focus under this house's Wayland/Mutter/Xwayland-rootless environment needed a specific fix (raise-then-focus, not bare `XSetInputFocus`) — see `!.HOUSE_STDS.md` §F-19 for the general rule and how it was diagnosed (a real XTest key/click injector + per-frame draw logs, not guesswork).
- No stale-PID cleanup in `livedesk_open.txt` if an entity is SIGKILLed rather than closed cleanly.

## Debug tooling

`#.desktop/tp_taskbar_debug/` (created on demand) holds real, inspectable render state — not guesses:

- `strip_frame.raw` / `.receipt.txt` — actual RGBA capture of the strip window (`XGetImage`), same raw+receipt contract `014.wsr-pal💸️📌️+2/ops/dump_rgb_png.c` reads (`frame_w`/`frame_h` keys) — decode with that tool or any RGBA8888 reader.
- `strip_frame_log.txt` / `popup_frame_log.txt` — one text block per real frame change: every cell/row's label, nav number, and focus state.
- `strip_frame_changed.txt` — append-only marker file; the strip only redraws + recaptures when this grows (mirrors `chtpm_parser_pal.c`'s own `frame_changed.txt` contract).
- `key_history.txt` — every armed-mode keypress logged before it's acted on.
- `key_inject.txt` (only present while a companion injector is running) — write `KEY_PRESSED: <keysym-name>` or `CLICK: <x> <y> <button>` lines here to drive the taskbar via real XTest-injected X11 events, for scripted/automated testing without a human at the keyboard.

## History

- `walk-off-au5.md` — original 2026-08-05 build session handoff (tab bar only, predates the strip).
- `yz.muchiverse/a8-cc-++fix.md` — 2026-08-09 nav-number bugs, unified focus cursor, LEGACY architecture warning.
- `yz.muchiverse/opacity-bug-aug9.txt` — 2026-08-09 opacity investigation (open).
