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
5c. **`!.HOUSE_STDS.md` §J — READ BEFORE ASSUMING A `.chtpm`/CSS FEATURE EXISTS** — there are TWO
   separate, unmerged parser implementations sharing the `.chtpm` name: legacy `chtpm_parser_pal.c`
   (PAL-VM/text-grid, what §A of that doc covers) and the newer `khtpm_*` family (raw Xlib/Xft, no
   PAL VM, its own `khtpm_css_parser.c`) that the taskbar and every tb-launched sub-app (db-hq,
   ai-cell/"open-hai", events-hq, chat-hai) actually use. Confirmed real feature drift this session:
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

---

## Document Roles (so you don't duplicate one)

| File | Purpose | Update when |
|---|---|---|
| `INDEX.md` | This file — pure routing, no content | New doc added/removed |
| `HANDOFF.md` | Living architecture + status snapshot, "hand this to a fresh agent" doc | Architecture changes, status changes |
| `HARNECIENT-HACK.md` | **THE COMPANY'S BREAD AND BUTTER** - tool-like use out of NON-tooled models (gemma 270M/1b, stable-code 3b) by never telling the API we want tools: plain /api/chat, persona files forbidding structure, simple plain-text prompts, tolerant parser (`json_parser.+x`), deterministic app-side tool dispatch, real-file folding, fallback-everywhere, DESCRIBE-don't-CLASSIFY. Live reference: `@.apps/my-lawyer` (gemma reads+writes real case docs). Use this pattern for any feature that needs tool-like behavior on non-tooled models. | Before building any agentic/tool feature on a non-tooled LAN model |
| `TESTING_STRATEGY.md` | How to test (relay-only rule, harness patterns, verified recipes) | Testing approach changes |
| `TASKBAR-MENU-ARCHITECTURE.md` | Taskbar menu dispatch mechanics: which cells are C-hardcoded vs PDL-driven, the two-layer relay system, the exact recipe to add a menu item, nav.sh gotchas, standing PDL-externalization refactor debt, PLUS the full lifecycle/pitfalls of building a brand-new khtpm_*-family sub-app from scratch (elem-pool exhaustion, apply_css() clobbering, tail-vs-head ledger reads, PDL-driven geometry) | When a new taskbar cell/menu is added, a new sub-app is built, or the PDL-externalization refactor is finally done |
| `chat-hack.md` | Theory/exploration doc (not yet implemented): maps chat-hai's current bot-loop against the Harnecient Hack's 6 components, finds the real gap (no memory/relationships/document artifacts — only a flat ledger), proposes concrete DESCRIBE-not-CLASSIFY-safe designs for persona memory, relationship scoring, shared documents, and activating the already-stubbed moderator hook | Before implementing chat-hai memory/relationships, or exploring similar "evolving multi-agent conversation" designs elsewhere |
| `USER_CREATION.md` | Deep-dive research + plan + test log for one specific feature | That feature's design/status changes |
| `EVENTS_RUNTIME.md` | Event runtime mechanics, real bugs fixed, ops migration, ops-vs-events architecture | Runtime/architecture changes |
| `EVENT_AI_VISION.md` | Long-range intent: trigger types, message/input UI reuse, entity AI, network events | New capability designed/started |
| `a12.opencode-prompt.md` | Self-contained handoff prompt for the parallel palette-picker agent | If that task's scope changes before it's dispatched |
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
| `AI-CELL-GUI-DESIGN.md` | REAL AND BUILT (as of 2026-08-12) - cell 14 "h-ai" window: real nav, scroll, disk-persisted deletable history, raw Ollama backend, PNG+receipt verification. Current primary work: Harnecient integration (model switcher → protocol → relay demo → harness). See `13.AUG.13-HAI-2do.txt) for detailed roadmap and phase breakdown. See `ONBOARDING.md` for current status and next steps.` FIRST for the actual current blocker |
| `&.widgits/ai-cell/code-tools-harness/api-test-results.md` | 2026-08-12 Ollama tool-use probe. HEADLINE: HARNECIENT (my-lawyer strategy) proven 9/9 on non-tooled `stable-code` 3B + `gemma3:1b` + `gemma3:270m` (deterministic read/edit/run, zero `tools` fields). Contrast: 3B has NO native tools (server rejects); naive text-JSON calls are flaky/hallucinate; 8B native works but read-then-edit FAILS (parallel calls, guessed search). Harness + raw JSONs in same dir. | When tool-use capability/verdicts change |
| `&.widgits/ai-cell/code-tools-harness/LEARNINGS.md` | 2026-08-12 "don't waste time again" doc: §0 = THE HARNECIENT HACK (see `HARNECIENT-HACK.md`) - proven 9/9 on 3B/1b/270m; 3B cannot do native tools (don't retest); pure-C-only rule; 8B emoji-path corruption; native args arrive as dict-or-string with `\uXXXX` escapes; 8B won't wait for read before editing; harness C-port parity bugs (missing `n++`, wrong verdict msg); fopen-on-directory `ftell=LONG_MAX` malloc crash. Read BEFORE re-probing Ollama tool use or touching the harness. | Before any new Ollama tool-use probing or harness edits |
| `#.Z.HUMAN_LLM/.MAC-ACCESS.txt` (house root, NOT au11-hq) | LAN Mac SSH+Ollama access (10.0.0.144, real model list, the "Ollama only binds localhost after a restart" fix) - single source of truth, read before any LAN-model work | Before any SSH/Ollama LAN access; if Ollama seems unreachable, check the restart-fix here first |
| `$.claude-hai-budget.md` | **HIGH PRIORITY** - Claude↔h-ai token budget strategy: when to delegate scoped/mechanical work to a Harnecient model instead of doing it in-session, standing offload opportunities, BT/FSM/RL test-runner ops worth building, delegation log | Every time a real delegation opportunity is identified or acted on - log it, don't just plan |
| `EVENTS-PAL-BUILDOUT-PLAN.md` | Delegation plan for building out the RPG-Maker event command vocabulary (~90 commands, `#.ref/menu/`) via `run_plan.sh`/`run_queue.sh` - real architecture confirmed (Change Gold IS `.pal`-driven, not hardcoded), staged rollout (Party/Actor commands first - structurally identical to Change Gold, highest delegation leverage; Flow Control needs real VM opcodes, keep in Claude; Movement/Screen/Audio/Battle blocked on unbuilt infra) | Before starting ANY new event command implementation - check which stage it belongs to first |
| `HARNESS-DELEGATION-PIPELINE.md` | **HIGH PRIORITY, BUSINESS-CRITICAL** - survey of every existing harness (Harnecient Hack, code-tools-harness, detect_tool(), relay harnesses, Phase-4 design) + the real gap (no adaptive multi-step tool chaining exists yet) + a staged design (deterministic FSM, pluggable tool registry, declarative scoring, checkpoint/resume) for a reusable, sellable delegation pipeline. Direct instruction: "always favor making a harness to do work that we can reuse over recoding something by hand." | Before writing ANY new one-off dispatch/test script - check if it should be a reusable harness piece instead; update when a new harness is built or the pipeline design advances |

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

**Last updated:** 2026-08-15 (§J added to `!.HOUSE_STDS.md` — the khtpm_* vs chtpm_parser_pal.c parser split is real and unmerged, confirmed feature drift documented; chat-hai now fully working: sessions add/delete, reliable LAN-call-gated pause, typing indicator, PDL-driven window geometry, real text wrapping) by claude
