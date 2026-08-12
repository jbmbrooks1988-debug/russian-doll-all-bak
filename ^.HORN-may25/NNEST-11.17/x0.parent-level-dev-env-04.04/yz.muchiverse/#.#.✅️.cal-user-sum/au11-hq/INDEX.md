# 📍 START HERE — au11-hq Document Index

**If you're a new agent picking this up cold, read in this order until you have enough context to act.**

---

## Tier 1 — Always read (small context budget, ~2 min)
1. **This file** — orientation, what exists, read order
2. **HANDOFF.md → "⚡ TL;DR" + "🎯 Project State" sections only** — current status, what's next

Stop here if you just need to know "what's going on."

---

## Tier 2 — Read before touching code (medium context budget, ~10 min)
3. **HANDOFF.md (full)** — architecture (sessions/desks/events), file structure, known gaps, demo game list
4. **HANDOFF.md → "⚠️ STANDING RULE"** (near top) — check local chtpm, then tpmos
   (`1.TPMOS_c_+rmmp.0103.0001/`), before inventing any new UI/state pattern
5. **TESTING_STRATEGY.md** — relay-only testing rule (no direct CLI calls), harness template, frame-history reading

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
10. **DB-HQ-HANDOFF.md** — Implementation handoff for db cell (cell 9). Status: broken/incomplete.
    Explains what's wrong, step-by-step debug/fix plan for db-ez, placeholder for db-hq. Read this
    if you're fixing the db cell or implementing db-ez/db-hq.

---

## Document Roles (so you don't duplicate one)

| File | Purpose | Update when |
|---|---|---|
| `INDEX.md` | This file — pure routing, no content | New doc added/removed |
| `HANDOFF.md` | Living architecture + status snapshot, "hand this to a fresh agent" doc | Architecture changes, status changes |
| `TESTING_STRATEGY.md` | How to test (relay-only rule, harness patterns, verified recipes) | Testing approach changes |
| `USER_CREATION.md` | Deep-dive research + plan + test log for one specific feature | That feature's design/status changes |
| `EVENTS_RUNTIME.md` | Event runtime mechanics, real bugs fixed, ops migration, ops-vs-events architecture | Runtime/architecture changes |
| `EVENT_AI_VISION.md` | Long-range intent: trigger types, message/input UI reuse, entity AI, network events | New capability designed/started |
| `a12.opencode-prompt.md` | Self-contained handoff prompt for the parallel palette-picker agent | If that task's scope changes before it's dispatched |
| `2do-au11.txt` | Task tracker / progress log | Every work session (append to Progress Log) |
| `a11.focus-troubleshooting.md` | Closed-bug record | Rarely (historical) |
| `maintenance-fixes.md` | Small non-blocking polish items | Whenever one is noticed |
| `_.0.aigent-testing-k9.txt` (house root, not au11-hq) | House-wide testing guide across ALL program families, with a 2026-08-11 khtpm-specific addendum at the bottom | When a testing mechanism is discovered/corrected for a NEW family |

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

**Last updated:** 2026-08-12 by claude-0001
