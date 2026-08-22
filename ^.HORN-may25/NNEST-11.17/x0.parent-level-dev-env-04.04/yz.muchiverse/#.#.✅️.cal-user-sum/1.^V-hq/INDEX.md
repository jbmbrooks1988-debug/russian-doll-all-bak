# 📍 START HERE — au11-hq Document Index

**If you're a new agent picking this up cold, read in this order until you have enough context to act.**

---

## Tier 1 — Always read (small context budget, ~2 min)
1. **This file** — orientation, what exists, read order
2. **HANDOFF.md → "⚡ TL;DR" + "🎯 Project State" sections only** — current status, what's next
3. **$.claude-hai-budget.md — HIGH PRIORITY, direct instruction (2026-08-13)** — before doing scoped/mechanical work yourself, check whether it should be delegated to a Harnecient model (h-ai) instead, to save Claude token budget. Log delegation opportunities there, not just plans.
4. **HARNESS-DELEGATION-PIPELINE.md — HIGH PRIORITY, direct instruction (2026-08-13)** — before writing any new one-off dispatch/test script, check whether it should be a reusable harness instead. Harnesses are sellable products, not just test infra.

Stop here if you just need to know "what's going on."

---

## Tier 2 — Read before touching code (medium context budget, ~10 min)
3. **HANDOFF.md (full)** — architecture (sessions/desks/events), file structure, known gaps, demo game list
4. **HANDOFF.md → "⚠️ STANDING RULE"** (near top) — check local chtpm, then tpmos
   (`1.TPMOS_c_+rmmp.0103.0001/`), before inventing any new UI/state pattern
5. **TESTING_STRATEGY.md** — relay-only testing rule (no direct CLI calls), harness template, frame-history reading
5b. **TASKBAR-MENU-ARCHITECTURE.md — READ BEFORE TOUCHING ANY TASKBAR MENU** — cell 14 (h-ai)'s
   submenu is C-hardcoded (`livedesk_build_ai_menu()`), NOT PDL-driven, despite matching PDL rows
   existing and looking live (they're dead — editing them wastes a full debug cycle, confirmed
   2026-08-15). Covers the real two-layer relay/dispatch architecture, the exact recipe to add a
   menu item without repeating a ~15-round-trip debug session, the `nav.sh` double-Enter trap, and
   the standing PDL-externalization refactor debt (why this drifted from config-driven back to
   hardcoded C, and what the real fix looks like).
5b2. **`taskbar-tpmos-parallel-refactor.md` + `taskbar-history-txt-migration-investigation.md` —
   READ BEFORE TOUCHING `khtpm_strip_parser.c`'s REAL X11 INPUT/DISPATCH CODE** (2026-08-18,
   cutover completed 2026-08-19): the taskbar now has a real terminal ASCII mirror (HQ menu's "cli"
   row) AND a single dispatch path — capture-only writers + one read-back dispatcher, matching
   mutaclysm's real `x11_mirror.c`/`game_dispatch.c` split. The old inline-dispatch path and its
   `KHTPM_NEW_DISPATCH_MODE` feature flag are DELETED, not just defaulted off — there is no
   fallback mode anymore. Live-verified with real X11 input (genuine user interaction AND real
   `XTest`-based injection tools - see `&.widgits/tile-picker/ops/
   tp_test_send_key.c`/`tp_test_send_click.c`, the real fix for "no xdotool on this machine"). A
   same-day frame-unification pass (`strip_frame.cells.pdl`) then introduced and fixed a real bug —
   see `taskbar-history-txt-migration-investigation.md`'s "Phase 3" section and
   `au11-hq/TASKBAR-FRAME-UNIFICATION-HANDOFF.md` (under `44.xyz❤️‍🔥️00.17/`) for the frame-sharing
   plan that pass was the first step of.
   `dispatch_key_code()` gained two real relay-forwarding fixes (`KSC_HQ_HEADER_BASE`,
   `KSC_FOCUS_LEFT`/`RIGHT`) this pass - know about them before assuming the relay's own
   capabilities match its older, more limited header comment.
5c. **`!.HOUSE_STDS.md` §J — READ BEFORE ASSUMING A `.chtpm`/CSS FEATURE EXISTS** — there are TWO
   separate, unmerged parser implementations sharing the `.chtpm` name: legacy `chtpm_parser_pal.c`
   (PAL-VM/text-grid, what §A of that doc covers) and the newer `khtpm_*` family (raw Xlib/Xft, no
   PAL VM, its own `khtpm_css_parser.c`) that the taskbar and every tb-launched sub-app (db-hq,
   open-hai/"open-hai", events-hq, chat-hai) actually use. Confirmed real feature drift this session:
   `khtpm_css_parser.c` has no flexbox AND no descendant-selector (`.a .b`) support — both silently
   no-op rather than error, and both caused real, live-caught chat-hai bugs. If you're building on
   any tb sub-app, you're in the `khtpm_*` family — §A of `!.HOUSE_STDS.md` does not apply to you.

---

## Tier 2.5 — Read before major architectural decisions

**HQML-DESIGN+PLANS.md** — Vision for a web-like markup language (HTML/CSS-like syntax) that would enable prettier UIs while keeping .pal scripting. Covers events-hqml (prettier event editor), db-hq (modern database UI), AI applications, network applications (forum/IRC). Design phase, technical requirements, implementation roadmap. Read this if designing new UIs or considering how to modernize existing ones.

---

## Tier 3 — Read only if working the specific task (large context budget)
6. **USER_CREATION.md** — full research + plan for wiring account creation into livedesk's USER
   cell (tpmos multi-field `<cli_io>` reference, khtpm gap analysis, step-by-step build plan)
6b. **EVENTS_RUNTIME.md** — how the event-ez runtime actually fires (right-click "Play" /
   `RUN_METHOD:Play` relay, not automatic on-click), real bugs found+fixed (path resolution, stale
   in-memory cache after migration), the completed ops migration (`xyzfs/bin/<game>/ops/+x/`
   pattern), and the ops-vs-events sharing model (shared ops, private-until-published events via the
   `store` cell). Read before touching event/db/common-events work.
6c. **EVENT_AI_VISION.md** — the long-range design intent for events: RPG-Maker-aligned trigger
   types, message/choice events that reuse chtpm's own layout/navigation (self-bootstrapping, not a
   new UI), entity AI (movement/interaction/decision-trees via FSM/BT, referencing agent-45's
   tool-loop pattern and SCM's deterministic-first philosophy), and future network/MMO-participation
   events. Read before designing any new event TYPE (not just wiring existing ones).
6d. **HAIKU_TASKS.md** — small, well-scoped tasks suitable for Haiku (Claude's faster variant).
   Self-contained work with clear success criteria, no architectural decisions. Start here if you
   want to pick up a concrete, bounded task without needing Sonnet's reasoning depth.
6e. **DB_CONTEXT.md** — explanation of what the "db" (database) cell is and how event commands
   like Show Text + Show Choices integrate with it. Read after completing an event-related task
   to understand the bigger picture.
7. **2do-au11.txt** — the working task log: what's done, what's pending, timestamped progress notes.
   This is the most volatile file — always check its bottom (Progress Log) for the latest state.
8. **a11.focus-troubleshooting.md** — historical record of two entity-window bugs already fixed
   (focus-steal, arrow-nav). Reference only; not active work.
9. **maintenance-fixes.md** — running list of small, non-blocking UI polish items (index numbers,
   window sizing, etc.). Check before starting cosmetic work; add to it, don't fix inline elsewhere.
10. **DB-HQ-HANDOFF.md** — Implementation handoff for db cell (cell 9). Status: broken/incomplete
    AS OF BEFORE 2026-08-12 — db-hq itself got real, working this session, see #11 below for the
    current state before trusting this file's own "still-placeholder" framing.
11. **EVENTS-HQ-RGB-HANDOFF.md** — 2026-08-12 session handoff (context ran full, new agent picking
    up). db-hq's real focus fix (managed window + `_MOTIF_WM_HINTS`, NOT override_redirect — see
    `!.HOUSE_STDS.md` #21-24 house-root), events-hq built real and wired to two entities (nav
    confirmed working), RGB Phase 0 proven (compose→buffer→present is pixel-identical), and the
    concrete next-step plan for both (events-hq's remaining event-ez-parity gaps, how to actually
    refactor db-hq/taskbar to RGB). **Read this before starting ANY db-hq/events-hq/khtpm-window/
    RGB work** — it points at exactly what's real vs. still-needed, don't re-derive.
12. **OPENROUTER-INTEGRATION-HANDOFF.md** — 2026-08-16, real router API key work for open-hai.
    Status: **DONE for OpenRouter, TokenRouter marked a real confirmed paywalled non-starter**
    ($0 account credit blocks tool-calling even on free-labeled models — plain chat works fine).
    OpenRouter is fully live-verified through the REAL UI (not just the manager in isolation):
    cycled the model via a real relay-injected UI action, sent a message that ALSO matches the
    local tool-detection harness's own keywords, confirmed it now bypasses that harness for this
    backend ("un-harnessed"), and watched the model's real native `tool_calls` response actually
    EXECUTE (not just get detected) and show a real result in the transcript/GUI. Also caught and
    fixed a real bug mid-session: the shell (`khtpm_open_hai_render.c`) has its OWN separate model
    list from the manager's — adding models to only one silently broke the model-changer UI.
    **Read this before touching open-hai's backend/model logic** — real next steps left: no shared
    source of truth for the model list (real footgun), conversation history, cost tracking,
    model-name/error validation, more tools in the OpenRouter `tools` array.

---

## Document Roles (so you don't duplicate one)

| File | Purpose | Update when |
|---|---|---|
| `INDEX.md` | This file — pure routing, no content | New doc added/removed |
| `HANDOFF.md` | Living architecture + status snapshot, "hand this to a fresh agent" doc | Architecture changes, status changes |
| `livedesk-dir-map.md` | Real, current directory map of everything meaningful to the livedesk toolbar (taskbar, the 5 merged window apps, `#.desktop/` runtime state, toys-cell targets, tile-picker test tooling, `xyzfs/`'s real current scope) - written 2026-08-17 to inform a real, still-undecided xyzfs migration question (only `muchi-pet`/`livedesk-clock` have moved into `xyzfs/bin/` so far; everything else, including mutaclysm/the taskbar, is still outside it) | When the directory structure meaningfully changes, or before deciding/acting on any xyzfs migration |
| `legacy-shared-fix.md` | Separate leg of work from `khtpm-merge-how2.md`: consolidating all 16 legacy-GL projects' `system/`+`ops/` engine binaries. **AS OF 2026-08-17: `chtpm_parser_pal.c`/`prisc+x.c` consolidation is DONE - all 12 real participants (of 16 total; 4 have neither file) now on ONE shared baseline (`&.widgits/_shared-lib/system/`), see §3.10.** `chtpm_rgb_render.c` also consolidated (9 projects, §5c.7 in the other doc). `gl_mirror.c`→`x11_mirror.c` display-shim migration: 3 of 16 projects done (mutaclysm/piececraft-xyz/my-chara-txt), 13 remain - real, open work. Also covers a real mutaclysm interact-mode regression found+fixed post-consolidation (§3.11) and mutaclysm's own separate, deferred camera/3D work (§2.6, handed off to `opencode-mutafix-pie.md`). | Every time the remaining 13-project GL migration or mutaclysm's own deferred camera work advances |
| `HARNECIENT-HACK.md` | **THE COMPANY'S BREAD AND BUTTER** - tool-like use out of NON-tooled models (gemma 270M/1b, stable-code 3b) by never telling the API we want tools: plain /api/chat, persona files forbidding structure, simple plain-text prompts, tolerant parser (`json_parser.+x`), deterministic app-side tool dispatch, real-file folding, fallback-everywhere, DESCRIBE-don't-CLASSIFY. Live reference: `@.apps/my-lawyer` (gemma reads+writes real case docs). Use this pattern for any feature that needs tool-like behavior on non-tooled models. | Before building any agentic/tool feature on a non-tooled LAN model |
| `TESTING_STRATEGY.md` | How to test (relay-only rule, harness patterns, verified recipes) - PLUS (2026-08-18) how to test REAL X11 input specifically (not the relay): real `XTest`-based `tp_test_send_key.+x`/`tp_test_send_click.+x` (confirmed `xdotool` is NOT installed here), and the `WM_CLASS`-vs-title window-matching gotcha | Testing approach changes |
| `TASKBAR-MENU-ARCHITECTURE.md` | Taskbar menu dispatch mechanics: which cells are C-hardcoded vs PDL-driven, the two-layer relay system, the exact recipe to add a menu item, nav.sh gotchas, standing PDL-externalization refactor debt, PLUS the full lifecycle/pitfalls of building a brand-new khtpm_*-family sub-app from scratch (elem-pool exhaustion, apply_css() clobbering, tail-vs-head ledger reads, PDL-driven geometry) | When a new taskbar cell/menu is added, a new sub-app is built, or the PDL-externalization refactor is finally done |
| `taskbar-keyboard-relay-and-terminal-render.md` | ORIGINAL finding (2026-08-18): taskbar's real X11 input+render was bundled into one process (`khtpm_strip_parser.c`), unlike mutaclysm/TPMOS's real split renderer/input binaries. Superseded in practice by the two docs below, which built the actual fix - kept as the original architectural discovery, still cross-linked as prerequisite reading from both. | Historical/prerequisite only - read the two docs below for current status |
| `taskbar-tpmos-parallel-refactor.md` | **DONE, live-verified**: taskbar's real terminal ASCII mirror (HQ menu's "cli" row → `khtpm_strip_render_ascii.+x` + `khtpm_strip_keyboard_ascii.+x`, matching TPMOS's real renderer.c/keyboard_input.c split exactly - fixed a real `\r\n`/staircase bug from an earlier combined-binary attempt). Strip + HQ popup + bottom tabs render with the real `[cursor] N. [Label]` format (ported from `chtpm_parser.c`'s own `render_element()`). Real `[>]` cursor + arrow-key nav on the strip, driven by two real relay-forwarding gaps found+fixed in `khtpm_strip_parser.c`'s `dispatch_key_code()`. Still open: bottom-tab-bar activation (renders, doesn't yet activate via relay - bigger shared nav-claims system) and `cli_io` typing. | Before touching the taskbar's ASCII mirror, or `dispatch_key_code()`'s relay-forwarding logic |
| `taskbar-history-txt-migration-investigation.md` | **Phase 1 + Phase 2 DONE + live-verified, Phase 3 (full cutover) DONE 2026-08-19**: real X11 `KeyPress`/`ButtonPress` mirror into `#.desktop/strip_input_history.txt` (real `KEY_PRESSED:`/`MOUSE_EVENT:` format, same as `pieces/keyboard/history.txt`); the old inline-dispatch path and its `KHTPM_NEW_DISPATCH_MODE` flag are DELETED — capture-only writers + one read-back dispatcher (matching mutaclysm's real `x11_mirror.c`/`game_dispatch.c` shape) is the only path now, no fallback. Real XTest-based input-injection tools used for live testing (`&.widgits/tile-picker/ops/tp_test_send_key.c`/`tp_test_send_click.c` - real fix for "no xdotool on this machine"). A same-day frame-unification pass introduced+fixed a real arrow-key-submenu-nav bug (see the doc's own "Phase 3" section). | Before touching real X11 capture/dispatch in `khtpm_strip_parser.c`, or before the next frame-sharing step in `au11-hq/TASKBAR-FRAME-UNIFICATION-HANDOFF.md` |
| `chat-hack.md` | Theory/exploration doc (not yet implemented): maps chat-hai's current bot-loop against the Harnecient Hack's 6 components, finds the real gap (no memory/relationships/document artifacts — only a flat ledger), proposes concrete DESCRIBE-not-CLASSIFY-safe designs for persona memory, relationship scoring, shared documents, and activating the already-stubbed moderator hook | Before implementing chat-hai memory/relationships, or exploring similar "evolving multi-agent conversation" designs elsewhere |
| `khtpm-merge-how2.md` | Step-by-step guide to merge the `khtpm_*_render.c` copies. **AS OF 2026-08-16 END OF SESSION (see its own "CURRENT REAL STATUS" right after the title): Stage 5 (literal single-binary merge) is DONE for all 5 window apps** — entity-menu, taskbar-settings, db-hq, events-hq, chat-hai all now live in ONE binary (`khtpm_entity_menu_render.c`), mode-selected via `class=`. Old standalone renderers archived to `_.ARCHIVED-pre-merge-legacy.zip` (db-hq's own kept live — `stats-hq` still uses it). 2 real chat-hai bugs (undecoded escapes, `fgets`-truncated ledger lines) fixed post-merge; a WM-managed-window drag-clamp fix also landed. See §5d.6–§5d.13 for the full trail (§5d.13: a real db-hq/chat-hai `[X]`-close bug that closed ALL desktop entities, found+fixed 2026-08-17). **Legacy GL migration (§5c.1): 3 of 16 projects converted (mutaclysm/piececraft-xyz/my-chara-txt) to a shared `x11_mirror.c` binary, 13 remain - see `legacy-shared-fix.md` for the real, current status of this thread.** | Before touching ANY khtpm app's rendering/model/identity logic, or before assuming any stage is further along than its own real STATUS section says |
| `USER_CREATION.md` | Deep-dive research + plan + test log for one specific feature | That feature's design/status changes |
| `EVENTS_RUNTIME.md` | Event runtime mechanics, real bugs fixed, ops migration, ops-vs-events architecture | Runtime/architecture changes |
| `EVENT_AI_VISION.md` | Long-range intent: trigger types, message/input UI reuse, entity AI, network events | New capability designed/started |
| `a12.opencode-prompt.md` | Self-contained handoff prompt for the parallel palette-picker agent | If that task's scope changes before it's dispatched |
| `opencode-mutafix-pie.md` | Self-contained handoff prompt for a separate opencode agent: port mutaclysm's own 3D camera/render engine to piececraft/board-viewer's own real architecture. Real motivation: 4 separate real camera bugs were found+fixed live this session (camera-struct rewrite, duplicate-process flicker, missing offscreen buffer, a state.txt read-cap bug), each correct in isolation, but a new bug kept surfacing every time - direct instruction: stop patching, port the underlying architecture instead. Written 2026-08-17, full real bug history in `legacy-shared-fix.md` §2.6 | When the porting agent reports its real plan/findings, or scope changes before dispatch |
| `2do-au11.txt` | Task tracker / progress log | Every work session (append to Progress Log) |
| `a11.focus-troubleshooting.md` | Closed-bug record | Rarely (historical) |
| `maintenance-fixes.md` | Small non-blocking polish items | Whenever one is noticed |
| `_.0.aigent-testing-k9.txt` (house root, not au11-hq) | House-wide testing guide across ALL program families, with a 2026-08-11 khtpm-specific addendum at the bottom | When a testing mechanism is discovered/corrected for a NEW family |
| `EVENTS-HQ-RGB-HANDOFF.md` | 2026-08-12 session handoff: db-hq focus fix, events-hq built+wired, RGB Phase 0 result, next-step plans | When events-hq/db-hq/RGB work resumes and this doc's own "next steps" get done |
| `!.chtpm-render-dedup-guidance.md` (house root) | Deferred: `chtpm_rgb_render.c`/`chtpm_parser_pal.c` duplication across 22/28 dirs, NOT byte-identical (real per-app divergence) - investigation plan for whenever this becomes relevant, not urgent | Only when someone actually starts that dedup pass |
| `HARNECIENT-H-AI-RELAY.md` | **HIGH PRIORITY + LOAD-BEARING** - approved design to wire the Harnecient mode (`HARNECIENT-HACK.md`) into h-ai as a CHOOSABLE model, then demo + bake in a lasting reproducible harness for the full loop: relay injection into the real h-ai window → non-tooled model (270m/1b/3B) → deterministic read/write/run → **real control of the livedesk taskbar state files** (`strip_var_tabs.txt`, `strip_state.txt`). 4 phases (model switcher → `BACKEND_HARNECIENT` backend path → relay demo → `relay-harness/` N/N proof), success criteria + risks + milestones all in the doc. | Before starting any h-ai model-switcher / Harnecient-mode / relay-harness work |
| `chat-hai-design.md` | **HIGH PRIORITY** — design plan for a new side-bar multi-model conversation engine: 4 smol models (gemma270/qwen-ladder) constantly chatting with persistent memory, moderated by slower bigger models (qwen2.5:7b/haiku) that curate, reprompt, and delegate. Proof-of-concept ladder (Phase 0-5), memory/priority/FSM-recall architecture, relationship graphs, moderator loop, and roadmap. | Before starting any multi-agent / ambient-chat / side-bar conversation work |
| `13.AUG.13-HAI-2do.txt` | **DETAILED OPERATIONAL ROADMAP** — phases 1-5 with KPIs, sub-tasks, milestones for Harnecient integration into h-ai. Phases: model switcher (1-2h), Harnecient wiring (4-6h), relay demo (1-2h), reproducible harness (3-4h), autonomous generation (post-phase-4). Includes progression: Claude manages → Claude reviews → h-ai autonomous, with success criteria and risk mitigation. | Every phase completion or when strategy/timelines change |
| `1-1.HARNECIENT.AUBIO/` (this dir) | **HARNECIENT VOL 1 — the living textbook** (2026-08-12). 16 daily lessons in chapter format (README = cover + lesson map), audio-friendly, house cast (tomo/rahweh/maxine/iqa). Covers: the Harnecient Hack + 6 components, DESCRIBE-not-CLASSIFY, the 9/9 proof + war stories (fopen crash, 3B hallucination, 8B lying), the house + relay + nav.sh, h-ai & the relay plan, controlling tb, harness philosophy, and **the prompting masterclass (verbatim session-opener templates + token-saving playbook)**. Part VI = the telescope: create events, range-limited entity movement, **fake time starting 0 A.D. + endturn + time ticker + toolbar options**. Each day file stands alone; new lessons append as work grows. | Before starting a new Harnecient feature or onboarding an agent — read INDEX first, then the relevant day |
| `OPEN-HAI-GUI-DESIGN.md` | REAL AND BUILT (as of 2026-08-12) - cell 14 "h-ai" window: real nav, scroll, disk-persisted deletable history, raw Ollama backend, PNG+receipt verification. Current primary work: Harnecient integration (model switcher → protocol → relay demo → harness). See `13.AUG.13-HAI-2do.txt) for detailed roadmap and phase breakdown. See `ONBOARDING.md` for current status and next steps.` FIRST for the actual current blocker |
| `&.widgits/open-hai/code-tools-harness/api-test-results.md` | 2026-08-12 Ollama tool-use probe. HEADLINE: HARNECIENT (my-lawyer strategy) proven 9/9 on non-tooled `stable-code` 3B + `gemma3:1b` + `gemma3:270m` (deterministic read/edit/run, zero `tools` fields). Contrast: 3B has NO native tools (server rejects); naive text-JSON calls are flaky/hallucinate; 8B native works but read-then-edit FAILS (parallel calls, guessed search). Harness + raw JSONs in same dir. | When tool-use capability/verdicts change |
| `&.widgits/open-hai/code-tools-harness/LEARNINGS.md` | 2026-08-12 "don't waste time again" doc: §0 = THE HARNECIENT HACK (see `HARNECIENT-HACK.md`) - proven 9/9 on 3B/1b/270m; 3B cannot do native tools (don't retest); pure-C-only rule; 8B emoji-path corruption; native args arrive as dict-or-string with `\uXXXX` escapes; 8B won't wait for read before editing; harness C-port parity bugs (missing `n++`, wrong verdict msg); fopen-on-directory `ftell=LONG_MAX` malloc crash. Read BEFORE re-probing Ollama tool use or touching the harness. | Before any new Ollama tool-use probing or harness edits |
| `#.Z.HUMAN_LLM/.MAC-ACCESS.txt` (house root, NOT au11-hq) | LAN Mac SSH+Ollama access (10.0.0.144, real model list, the "Ollama only binds localhost after a restart" fix) - single source of truth, read before any LAN-model work | Before any SSH/Ollama LAN access; if Ollama seems unreachable, check the restart-fix here first |
| `$.claude-hai-budget.md` | **HIGH PRIORITY** - Claude↔h-ai token budget strategy: when to delegate scoped/mechanical work to a Harnecient model instead of doing it in-session, standing offload opportunities, BT/FSM/RL test-runner ops worth building, delegation log | Every time a real delegation opportunity is identified or acted on - log it, don't just plan |
| `EVENTS-PAL-BUILDOUT-PLAN.md` | Delegation plan for building out the RPG-Maker event command vocabulary (~90 commands, `#.ref/menu/`) via `run_plan.sh`/`run_queue.sh` - real architecture confirmed (Change Gold IS `.pal`-driven, not hardcoded), staged rollout (Party/Actor commands first - structurally identical to Change Gold, highest delegation leverage; Flow Control needs real VM opcodes, keep in Claude; Movement/Screen/Audio/Battle blocked on unbuilt infra) | Before starting ANY new event command implementation - check which stage it belongs to first |
| `HARNESS-DELEGATION-PIPELINE.md` | **HIGH PRIORITY, BUSINESS-CRITICAL** - survey of every existing harness (Harnecient Hack, code-tools-harness, detect_tool(), relay harnesses, Phase-4 design) + the real gap (no adaptive multi-step tool chaining exists yet) + a staged design (deterministic FSM, pluggable tool registry, declarative scoring, checkpoint/resume) for a reusable, sellable delegation pipeline. Direct instruction: "always favor making a harness to do work that we can reuse over recoding something by hand." | Before writing ANY new one-off dispatch/test script - check if it should be a reusable harness piece instead; update when a new harness is built or the pipeline design advances |
| `44.xyz❤️‍🔥️00.17/completed-sym-list.md` + `sim-smell-fix.md` (house root) | **SESSION NOTE 2026-08-21 — symlink-migration Step 2 session (opencode agent)**: wired `persist_session_state()` into **19 projects'** `button.sh` (all `0.user-pal👤️`, `@.apps/*`, `&.widgits/*`, TSOTS families) after the earlier house-wide `ln -s`→`cp -r` swap. STRUCTURAL CHANGES to know about if complications arise later: (1) every patched button.sh now copies mutable session state back to the real project root at session exit — data written mid-session only exists under `pieces/sessions/<id>/` (or `/tmp/.<app>-*`) until exit, so anything reading real-root state mid-run must read the SESSION copy instead; (2) persist blocks auto-`mkdir -p` real-root dest dirs on fresh checkouts; (3) volatile files deliberately NOT copied back (`avatar_window_pids.txt`, quit_flag, history, relays, gui_state); (4) identity ops unchanged — they write real roots via `USERPAL_LOGIN_ROOT`/seeded `house_root.txt`; (5) 5 harness scenarios updated (stale real-root assertions → session/house-level paths, post-exit persistence checks added, sessions launched with `setsid` so cleanup sweeps can't kill the scenario); (6) `&.widgits/{context-menu,event-editor,event-ez}` verified STATELESS — no persist wiring, on purpose; (7) known pre-existing quirk left as-is: external TERM on `button.sh run` defers the EXIT trap while keyboard_input is foreground (Ctrl+C in-UI unaffected). Full per-project reports + test recipes in `completed-sym-list.md`; classification/status tables in `sim-smell-fix.md`. Awaiting human per-project signoff before `.pre-symlink-swap` backup deletion. **Also this session: house-wide compile sweep added — all 44 build scripts now PASS** (fixed 5 pre-existing breakages: undefined `$_SS` vars in zoo-INK/muchi-pals/mutaclsym+18.0G build.sh, wsr-pal missing freetype flags, tile-picker missing vendored stb_image.h). **NEW STANDING HOUSE RULE recorded in `!.HOUSE_STDS.md` §A.2 + pitfall 7b: NEVER use symlinks anywhere in this tree (they break on Windows) — copy or compile-from-canonical-source instead; §A.2 rewritten for the copy-in/persist-out model (live binary rebuilds no longer apply to running sessions).** | If any project's save/config data looks stale, missing, or duplicated after a session — read this note + completed-sym-list.md first; if a build fails, re-run its scripts/build.sh and compare against the 44/44 baseline |

**Rule of thumb:** architecture/status → HANDOFF.md. How-to-test → TESTING_STRATEGY.md. Deep
single-feature research → its own `<FEATURE>.md`. Task-by-task progress → 2do-au11.txt. Never duplicate
the same fact across files — link instead (`see HANDOFF.md §X`).

---

## Standing Rules (apply everywhere, not just one doc)

1. **Testing:** All testing goes through relay/inject (`nav.sh`), never direct CLI binary calls.
   See TESTING_STRATEGY.md.
2. **New UI/state patterns:** Check local chtpm usage first, then tpmos
   (`1.TPMOS_c_+rmmp.0103.0001/`) before inventing new shape. See HANDOFF.md's standing-rule section.
3. **Storage:** New work (games, events, common events) goes under `sessions/<user>/<session>/`, not
   `@.apps/`. Games = sessions. Maps = desks.
4. **Ownership:** Store your own harnesses/test output under your own
   `xyzfs/users/<your_account>/harnesses/` so agents don't clobber each other.
5. **Blocked/uncertain?** Document the question + dead end in au11-hq/ rather than guessing silently.
6. **Demo games must use palettes**, not hardcoded content — direct instruction: "when u build the
   game, i want u 2 use pallets, to make sure users can use it and that we have autonomous harnesses
   ready to sell." Palette population UI is on the 2do (Task 4) but not yet built — building it may
   be a prerequisite for the first demo game, not an afterthought.

---

**Last updated:** 2026-08-21 (added session note for the 2026-08-21 symlink-migration Step 2
session — `persist_session_state()` wired into 19 projects' button.sh across the house, 5 harness
scenarios fixed, full trail in `44.xyz❤️‍🔥️00.17/completed-sym-list.md` + `sim-smell-fix.md`;
see the new Document Roles row for what may bite later) by opencode (ox-alpha)

**Previously:** 2026-08-17 (`legacy-shared-fix.md`: full `chtpm_parser_pal.c`/`prisc+x.c` consolidation done, 12/12 real participants on one shared baseline; `chtpm_rgb_render.c` consolidated for 9 projects; GL→X11 display-shim migration 3/16 done (mutaclysm/piececraft-xyz/my-chara-txt), 13 remain; a real mutaclysm interact-mode regression and a real db-hq/chat-hai `[X]`-close-all-entities bug both found+fixed; mutaclysm's own camera/3D work deferred to `opencode-mutafix-pie.md`. `!.HOUSE_STDS.md` also updated to point its own khtpm-window-reference mentions at the now-merged shared binary instead of the archived standalone files) by claude
