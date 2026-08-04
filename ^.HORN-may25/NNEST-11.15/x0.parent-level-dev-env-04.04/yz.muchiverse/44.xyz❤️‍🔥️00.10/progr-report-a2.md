# 🗡️ TSC_ELO — PROGRESS REPORT a2 🌐
### 🎯 *True Swords Clash — PvP over the house P2P stack (palnet_peer)*

> 💌 a1 → a2: **PvP design signed off, implementation under way.**
> Goal: prove `TSC_ELO` PvP works over the existing `palnet_peer` mesh with
> a harness that boots 2 subharnesses, drives a real duel through the widget
> GL input chain, and asserts convergence on real network artifacts.
> **This file is the resume-after-disconnect checkpoint.** Read this first.
>
> 🚩 **MILESTONE MET (a2 → pvp):** the PvP harness now **PASSES end-to-end**
> (P1 presence → P2 CHALLENGE wire → P3 ACCEPT → P4 MOVE both ways →
> P5 ledger convergence), with per-run proof archives. See
> `@.apps/TSC_ELO/prog-report-pvp.md` (progress) and
> `@.apps/TSC_ELO/summary-walkthru.txt` (play + test + done/todo + CPU guards).

---

## 🏁 TL;DR — WHERE WE STAND

| 🧩 Piece | 📍 Status |
|---|---|
| 📐 PvP design doc `@.apps/TSC_ELO/TSC_P2P_PVP.md` | ✅ written + signed off |
| 📜 Standards doc `2.muchi-verse/PAL-NET-STANDARD.txt` restored | ✅ written FROM reference impl |
| 🔁 `palnet_peer.c` verbatim copy + build | ✅ `ops/palnet_peer.c` + `ops/+x/palnet_peer.+x` |
| 📨 `ops/tsc_net.c` (inbox drainer) | ✅ written + builds clean |
| ⚙️ `ops/tsc_setup.c` PvP commands (CHALLENGE/ACCEPT/MOVE/RESIGN) | ✅ written + builds clean |
| 🪟 widget ops (MOVE keys in playing mode) | ✅ `setup_enqueue_cmd.c` + `setup_menu_input.c` edited, build clean |
| 🎼 orchestrator (tsc_duel peer + tsc_net drainer loop) | ✅ edited, builds (pre-existing warnings OK) |
| 🚪 `button.sh` net wiring (net/ files, presence root) | ✅ edited |
| 🏗️ `scripts/build.sh` tsc_net build + `button.sh check` list | ✅ edited |
| 🧪 `test-harn-same/` (fsm-driver.sh + pvp_duel.sh) | 🔜 NEXT |
| ▶️ Run + capture proof + write `prog-report-pvp.md` | 🔜 NEXT |

**CURRENT STEP: wiring is done in code; the harness (test-harn-same/) is the next build.**

---

## ✅ WHAT'S DONE (this session, in order)

### 📐 DESIGN (signed off by user)
- `@.apps/TSC_ELO/TSC_P2P_PVP.md` — full PvP design, all 4 decisions resolved:
  - **D1**: reuse `ops/palnet_peer.+x` AS-IS (kind is a CLI arg) AND restore the
    missing standards doc at its cited path.
  - **D2**: harness = FSM driver with pluggable answer sources
    `REAL_KEYS` / `GEMMA_LAN` / `BOOK` / `GUESS` (hybrid for scaffolding, all
    states eventually REAL_KEYS for human-parity).
  - **D3**: v1 INCLUDES widget GL UI moves (drive the widget's OWN session input
    chain via `tk_inject_key`, real history.txt → parser → interact_relay → menu
    ops → player_action → tsc_deal).
  - **D4**: each host owns its own ledger copy; convergence via message replay +
    `(game_id,seq)` dedupe.
- `2.muchi-verse/PAL-NET-STANDARD.txt` — restored standards doc (sections 0–4:
  file contract, presence root, symmetric discovery, ports + HELLO/DATA wire
  lines + backlog replay, heartbeat/stale/cleanup + dedupe). Written FROM the
  reference implementation.

### 🔁 VERBATIM PEER (reused, not rewritten)
- `cp 044…/ops/palnet_peer.c` → `@.apps/TSC_ELO/ops/palnet_peer.c`
- `gcc -Wall -Wextra -O2` → `ops/+x/palnet_peer.+x` (built OK, zero warnings).

### 📨 ops/tsc_net.c — the game's inbox drainer (remote half of cmd-bus philosophy)
- Reads NEW lines from `net/inbox.txt` (offset-tracked via `net/inbox_offset.txt`).
- Parses `sender|MSG|<seq>|<game_id>|<user>|<ts>|<action>` (7 fields).
- Dedupes via `net/applied.txt` (sender|seq) — replays/double-delivery are no-ops.
- Appends to per-session game ledger `pieces/system/games/<gid>/ledger.txt` with
  `via=net` (the PITFALL-21-safe convergence proof).
- Handles `CHALLENGE:<name>` (writes opponent, sets human types),
  `ACCEPT` (mode=PvP, game_state=playing, resets duel stats),
  `MOVE:` (snapshots to `net/state.txt`), `RESIGN` (game_state=victory).
- Fixed a parse bug this session: was splitting into 6 tokens (merging ts into
  action); now 7 tokens, action is the tail.

### ⚙️ ops/tsc_setup.c — host cmd-bus drainer, PvP commands added
- New helpers: `ensure_dir`, `read_kv_str`, `read_kv_int`, `broadcast_action`
  (writes `MSG|<seq>|<gid>|<user>|<ts>|<action>` to `net/outbox.txt`, seq from
  `net/seq.txt` counter), `game_ledger_append` (via=local), `write_opponent`,
  `read_role`/`write_role` (`pieces/system/pvp_role.txt`), `pvp_apply_move`
  (duplicates tsc_deal's Mana-Challenge math by design — no shared headers).
- New commands:
  - `PVP:CHALLENGE` — broadcasts `CHALLENGE:<pending_player>` (challenger IS
    player_1), writes role=player_1, ledger via=local.
  - `PVP:ACCEPT` — broadcasts `ACCEPT` (acceptor IS player_2, own name from
    per-session pending.txt), sets mode=PvP + playing + reset stats, role=player_2.
  - `MOVE:<strike|heavy|heal|block>` — turn-gated by `current_turn % num_players
    + 1` vs `pvp_role`; applies via `pvp_apply_move`, advances turn, broadcasts
    `MOVE:<action>`, ledger via=local. NOT YOUR TURN otherwise.
- Fixed a `read_kv_int` missing-helper (added it), fixed format-truncation
  warnings with `%.63s` precision.

### 🪟 WIDGET OPS (the GL UI move path, D3)
- `widgets/setup/ops/setup_enqueue_cmd.c` — added `CHALLENGE`→`PVP:CHALLENGE`,
  `ACCEPT`→`PVP:ACCEPT`, `RESIGN`, `MOVE <action>`→`MOVE:<action>`.
- `widgets/setup/ops/setup_menu_input.c` — added `pvp_playing()` (reads the
  HOST config via focus.txt session_root: mode=PvP && game_state=playing) and
  playing-mode key map: '1'→strike '2'→heavy '3'→heal '4'→block '5'→resign,
  each enqueued into the HOST cmd bus. Widget keeps its own setup_state
  last_message + screen_changed bump.
- Both build clean (`-Wall -Wextra -O2`).

### 🎼 SYSTEM ORCHESTRATOR (the launch wiring)
- Peer args changed from `chain_node pal-chain - net/outbox.txt net/inbox.txt
  chain_node` → **`tsc_duel tsc_elo - net/outbox.txt net/inbox.txt tsc_duel`**
  (the 6-arg form, PITFALL 20: execl does NOT shell-split).
- Added a `tsc_net` drainer loop (fork/exec one-shot `tsc_net 8` every 0.2s,
  skipped if `NO_NET`), PID logged so `kill_all_tracked` reaps it.
- Builds with the same pre-existing `-Wno-unused-result` warnings the
  build.sh already tolerates.

### 🚪 button.sh (host launcher)
- Session mkdir adds `net/`; seeds `net/outbox.txt` + `net/inbox.txt`.
- `export PRISC_NET_ROOT="$SCRIPT_DIR/net/presence"` (shared flat discovery dir,
  PAL-NET-STANDARD sec. 1) — two sessions find each other by kind.
- Removed `export NO_NET=1` (net now on by default; harness can set NO_NET to skip).
- `kill` action now also `pkill`s `ops/+x/palnet_peer` and `ops/+x/tsc_net`.
- `check` list now includes `ops/+x/tsc_net.+x` and `ops/+x/palnet_peer.+x`.

### 🏗️ scripts/build.sh
- Added `ops/+x/tsc_net.+x` to the ops build section.

---

## 🔜 NEXT STEPS (resume here)

1. **Build all host ops** to confirm the whole project still compiles clean:
   `bash @.apps/TSC_ELO/scripts/build.sh` (or at least `tsc_setup`, `tsc_net`,
   orchestrator, widget ops).
2. **Build `test-harn-same/`** under TSC_ELO (mirror `044.pal-chat-irc👥️+2/test-harn-same/`):
   - `button.sh` (compile/kill/help) compiling: `tk_inject_key`, `tk_type_text`,
     `tk_focus_item`, `tk_assert_contains`, and NEW ops `tsc_cmd.c` (enqueue a
     host cmd directly for scaffold states) + `tsc_answer.c` (book/guess/gemma
     answer sources).
   - `book.txt` — the "book" corpus file (keyword/topic → move).
   - `fsm-driver.sh` — THE FSM driver (name/location still open: proposed
     TSC_ELO-local, promote house-wide once proven).
   - `scenarios/pvp_duel.sh` — the scenario: BOOT → PRESENCE → CHALLENGE →
     ACCEPT → A_MOVE → B_VERIFY → B_MOVE → A_VERIFY → CONVERGENCE → DONE.
3. **Run it**, capture `proof/pvp-<timestamp>/` (mode table, presence, both
   inbox, both ledgers, both config, both current_frame).
4. **Assert all 5 proof criteria** P1–P5 (§5 of the design doc).
5. **Write `@.apps/TSC_ELO/prog-report-pvp.md`** in the same voice as
   `prog-report-au2.md`.

---

## ⚠️ KEY DECISIONS / CONTEXT (re-read before continuing)

- **Two-session model** (mirrors `demo_2user_chat.sh`): each subharness runs
  `NO_GL=1 setsid bash button.sh run --pal` with its OWN session dir, net files,
  and widget; **`config.txt` is SHARED** (symlinked from the real project dir,
  seeded once). Both hosts write the same config; pvp_role.txt is per-session
  and decides "my side" for turn-gating.
- **Data flow**: local action → `net/outbox.txt` → peer relays
  `MSG|<seq>|<gid>|<user>|<ts>|<action>` → remote `net/inbox.txt` → `tsc_net`
  drainer → per-session `games/<gid>/ledger.txt` via=net. Acting host writes its
  own ledger line via=local. **Convergence = both ledgers identical ordered
  sequence** (PITFALL 21: assert on real net artifacts, not shared-fs).
- **Live remote effect**: acting host mutates shared `config.txt`; remote
  `tsc_net` records the move per-session (proves net delivery); config change is
  visible to both via shared filesystem. Ledger is the network proof.
- **Roles**: challenger = player_1, acceptor = player_2 (global identity in the
  shared config). Each host's OWN name comes from its per-session pending.txt.
- **Wire**: 7 fields — `sender|MSG|seq|game_id|user|ts|action`.
- **Peer args** (orchestrator convention): `<own_kind> <project_id> <piece|-> 
  <outbox> <inbox> <seek_kind>` = `tsc_duel tsc_elo - net/outbox.txt
  net/inbox.txt tsc_duel` (6 args, PITFALL 20).
- **Turn math**: `current_turn % num_players + 1`; MOVE gated by
  `pvp_role` (my_cp) in `tsc_setup`.
- **PITFALLS in force**: 20 (execl no shell-split), 21 (real net artifacts),
  54 (wait for non-empty current_frame.txt before GL render).
- **House conventions**: zero-warning builds (`-Wall -Wextra -O2`), self-contained
  ops (no shared headers — the family deliberately duplicates small helpers like
  write_kv/read_kv), one-writer append-only cmd buses, REAL code over docs.

## 📁 RELEVANT FILES
- `…/44.xyz❤️‍🔥️00.10/progr-report-a2.md` — THIS file (checkpoint).
- `…/44.xyz❤️‍🔥️00.10/@.apps/TSC_ELO/TSC_P2P_PVP.md` — design (signed off).
- `…/44.xyz❤️‍🔥️00.10/2.muchi-verse/PAL-NET-STANDARD.txt` — restored standard.
- `…/@.apps/TSC_ELO/ops/tsc_net.c` + `ops/+x/tsc_net.+x` — inbox drainer.
- `…/@.apps/TSC_ELO/ops/tsc_setup.c` + `ops/+x/tsc_setup.+x` — host PvP commands.
- `…/@.apps/TSC_ELO/ops/palnet_peer.c` + `ops/+x/palnet_peer.+x` — verbatim peer.
- `…/@.apps/TSC_ELO/system/orchestrator.c` — tsc_duel peer + tsc_net loop.
- `…/@.apps/TSC_ELO/button.sh`, `scripts/build.sh` — net wiring.
- `…/@.apps/TSC_ELO/widgets/setup/ops/setup_enqueue_cmd.c`,
  `widgets/setup/ops/setup_menu_input.c` — widget MOVE path.
- `…/044.pal-chat-irc👥️+2/test-harn-same/` — source of `tk_*` ops + scenario
  template to mirror.
- `…/@.apps/TSC_ELO/prog-report-au2.md` — prior report (voice reference).

---

*Last updated: session at step "harness next" — all wiring code written, none of
the harness built yet.*
