# 🗓️ aug3-walk.md — Progress Report & User Walkthrough (Aug 3 2026)

> Agent: opencode (big-pickle) · 🤖️ Target: `gemma3:270m` + `1b` only
> Goal: **7-10/10** on GCC-C coding + Soul Pen book editing
> Architecture: **Agent-45 runs via `button.sh`** — opencode builds tools & docs, agent-45 uses them

---

## 📍 Where We Are Right Now

**W0 = ✅ DONE · W1 = 🟡 IN PROGRESS (ops built, agent integration done, first fill pending)**

```
W0  canon store        ██████████████ 100% ✅
W1  cell pipeline      ████████░░░░░░  60% 🟡  (ops built + agent integration, fill pending)
W2  rolling memory     ░░░░░░░░░░░░░░   0%
W3  review tiers       ░░░░░░░░░░░░░░   0%
W4  canaries + PAL     ░░░░░░░░░░░░░░   0%
W5  hardening          ░░░░░░░░░░░░░░   0%
```

---

## 🏗️ What Got Built Today

### W0 — Canon Store (`canon/`)

The entire franchise knowledge base, built from LAN-side source dirs:

| Folder | Contents | Count |
|---|---|---|
| `canon/source/solpen/` | 🎬 40 screenplay scenes (`scene_01`..`scene_40`) + `_meta/` (character_sheet, world_building, outline) | 43 files |
| `canon/lt/solpen/` | 📖 21 gold Living Testament chapters (`ch01`..`ch21`, ch16 from `ooo/0.LT-SP-ch16=best.txt`) | 21 files |
| `canon/lexicon/` | 🏷️ `entities.pdl` (12), `places.pdl` (4), `items.pdl` (7) — seeded from source meta | 3 files |
| `canon/ledger/` | 📊 `entity_index.pdl` — 38 deterministic grep-based rows | 1 file |
| `canon/plan/` | 🗂️ `solpen/ch01.plan` — 7-cell chapter plan | 1 file |
| `canon/` root | `manifest.pdl`, `README.md` | 2 files |

**Total: 82 files** · All ingested by `scripts/canon_ingest.sh` (idempotent, K3-verifiable)  
**Verify: PASS 4/4** — 40 scenes, 21 contiguous chapters, index row count matches manifest  
**Gold ch01 stats:** 76 verses · 1097 words · "The City Beneath the Dust"

### W1 — Cell Pipeline (`ops/`)

Five brand-new C ops, all house-style, all compile warning-free `-Wall -Wextra -O2`:

| Op | Lines | What It Does | Model? |
|---|---|---|---|
| `plan_cells.c` | 119 | Reads `ch01.plan` → materializes `cell_01.pdl`..`cell_07.pdl` + `cells.manifest` + `chapter.pdl` | ❌ none |
| `fill_cell.c` | 246 | Calls `gemma3:270m` on LINUX (`10.0.0.187:11434`) via `curl -d @file` — prompt = LT rules + truncated scene excerpts + beat + last-verse continuity → `cell_NN.txt` | 🟢 270m |
| `verify_cell.c` | 130 | Calls `gemma3:1b` on MAC (`10.0.0.144:11434`) — describe-shaped only: "OBSERVED ISSUES" (never a judge) → `cell_NN.review` | 🔵 1b |
| `apply_cell.c` | 122 | Assembles all `cell_NN.txt` → `chapter.generated.txt`, renumbers verses 1..N, joins continuations, drops noise | ❌ none |
| `grade_chapter.c` | 206 | Deterministic 0-100 score vs gold: verse_ratio(25) + word_ratio(25) + entity_cov(20) + style_markers(20) + clean_register(10) → appends `proof/model-grades.csv` | ❌ none |

### 🔗 Agent-45 Integration (NEW)

The 5 W1 ops are now wired into agent-45's tool system:

| Change | File | What |
|---|---|---|
| `button.sh` check list | `button.sh` | 5 new ops added to verification list |
| Keyword routing | `ops/gemma_strategy.c` | "plan cells" → `plan_cells`, "fill cell" → `fill_cell`, etc. |
| Execution paths | `ops/strategy_execute_a.c` | 5 new execution blocks + progress indicator (`tool_progress.txt`) |
| Agent instructions | `agent-onbording.txt` | New §"LT Pipeline" with step-by-step workflow |
| Architecture docs | `45.agent-vs-haiku+sp❤️‍🔥️.md` | New §11: "Agent-45 Integration" |

**How it works:** User types → `gemma_strategy.c` keyword-detects → `strategy_execute_a.c` pre-executes the op → result stashed → gemma sees result and responds. The deterministic layer does ALL the work; gemma just describes. 🎯

**Progress indicator:** For blocking LAN calls (fill_cell, verify_cell), a `tool_progress.txt` file is written with status=running/done, so the renderer can show "⏳ fill_cell running..."

### 🐛 Bugs Found & Fixed (x2)

Both were the same class — **path doubling** (`cells/cells/...`):

1. **plan_cells.c**: `spec_path` was `"%s/cells/cell_%02d.pdl"` where `cells_dir` already ends in `/cells` → doubled. Fixed + also fixed chapter-line 4-pipe parse (p4 nulled correctly).
2. **fill_cell.c**: `req_path`, `out_path`, `txt_path` all had the same doubling. Fixed all three.

Both compile clean after fix. Offline tests pass.

### 🗃️ Archive Created

```
archive/045.muchi-pal-agent.2026-08-03-20260803-201803.tar.gz  (646 KB)
```

Full pre-W0/W1 backup. Restore verified via `tar xzf`.

---

## ✅ Offline Verification Results

All ops tested against mock data before any LAN call:

| Op | Test | Result |
|---|---|---|
| `plan_cells` | Ran on `ch01.plan` → 7 cells, gold_verses=76 | ✅ PASS |
| `apply_cell` | Fake 4-verse cells → `chapter.generated.txt` | ✅ PASS (4 verses, continuations joined, noise dropped) |
| `grade_chapter` | Fake gen vs gold ch01 → 40.2/100 (expected low: 4 vs 76 verses) | ✅ PASS (all 5 metrics computed, CSV row written) |

`fill_cell` + `verify_cell` not testable offline (need LAN model). Build-verified only.

---

## 🌐 LAN Status

```
LINUX  10.0.0.187:11434  ✅ REACHABLE  — gemma3:270m (Q8_0, 268M params)
MAC    10.0.0.144:11434  (not tested this session — gemma3:1b lives here)
THIS BOX                orchestration only, no local inference ⛔️
```

Test generation against `10.0.0.187` returned valid JSON with response. Endpoint confirmed live.

---

## 🔜 What Happens Next

Agent-45 can now run the pipeline via its CLI. To test:

```
# In agent-45's CLI:
plan cells for solpen ch01
fill cell 01 for solpen ch01
verify cell 01 for solpen ch01
... (repeat for cells 02-07)
apply cells for solpen ch01
grade chapter solpen ch01
```

**Expected runtime:** ~2-5 min total (7 fills × ~2-30s each + 7 reviews + assembly + grade)

**Then inspect:**
1. `canon/work/solpen/ch01/chapter.generated.txt` — the regenerated chapter
2. `proof/model-grades.csv` — the deterministic grade (target: **7-10/10**)
3. Per-cell `.txt` and `.review` files — did 270m produce numbered verses? Did 1b find real issues?

---

## 🧭 Key Constraints Still Active

| Rule | Status |
|---|---|
| ⛔️ No local model compute | ✅ all inference on LAN nodes |
| 🚫 No llama / opencode / gemini | ✅ gemma family only |
| 🐍→🔧 GCC C everywhere | ✅ all new ops are C, warning-free |
| 🎓 Pass = 7-10/10 | pending first real grade |
| 🗜️ Archive before changes | ✅ done |
| 🔑 PITFALL 69: 1b is describe-shaped only | ✅ verify_cell never judges |
| 🤖️ Agent-45 runs the pipeline | ✅ not opencode — agent-45 uses the tools |

---

## 📁 Key Files (quick ref)

```
045.muchi-pal-agent🤖️+1++/
├── 45.agent-vs-haiku+sp❤️‍🔥️.md     ← full roadmap (§0-§11, §11 = agent integration)
├── agent-onbording.txt              ← agent instructions (LT pipeline section)
├── button.sh                        ← agent launcher (check list updated)
├── ops/gemma_strategy.c             ← keyword routing (W1 ops added)
├── ops/strategy_execute_a.c         ← execution paths + progress indicator
├── ops/plan_cells.c                 ← W1: plan → cell specs
├── ops/fill_cell.c                  ← W1: 270m generates verses
├── ops/verify_cell.c                ← W1: 1b describe-shaped review
├── ops/apply_cell.c                 ← W1: assemble + normalize
├── ops/grade_chapter.c              ← W1: deterministic scoring
├── scripts/lt_pipeline.sh           ← W1 driver (standalone)
├── scripts/canon_ingest.sh          ← W0 ingest (idempotent)
├── scripts/canon_verify.sh          ← W0 assertions
├── canon/                           ← W0 store (82 files)
│   ├── source/solpen/               ← 40 screenplay scenes
│   ├── lt/solpen/                   ← 21 gold LT chapters
│   ├── lexicon/                     ← entities, places, items
│   ├── ledger/entity_index.pdl      ← 38-row franchise index
│   ├── plan/solpen/ch01.plan        ← 7-cell plan
│   └── work/solpen/ch01/            ← W1 output (pending)
├── lan-paths.txt                    ← LAN endpoint map
└── ⛔️.compute-constraint.READ-FIRST.txt
```

---

*Report written 2026-08-03 by opencode (big-pickle) · Updated: agent-45 integration complete*
*Architecture: opencode builds tools & docs → agent-45 uses them via button.sh + gemma_strategy + strategy_execute_a*
