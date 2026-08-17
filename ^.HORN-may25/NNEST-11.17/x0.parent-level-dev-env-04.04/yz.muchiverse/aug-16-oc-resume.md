# aug-16-oc-resume.md — chat-hai Bug Investigation Report

**Date:** 2026-08-16
**Investigator:** opencode (big-pickle)
**Session:** post macro-update familiarization + two bug investigations

---

## Session Context

Macro updates have been applied to the house codebase including chat-hai layout and
logic model changes. The underlying file-mediated IPC architecture remains the same:
renderer (X11/Xft process) and loop (bash script) communicate exclusively through
`.ledger` files and state files (`typing.txt`, `paused.txt`, `active.txt`).

**Runtime state at investigation time:**
- Active session: `gemma-lab` (3 personas: echo, nova, quill on gemma3:270m)
- Ledger: 557 lines, 3 user messages total — all apparently ignored
- Loop: paused (`paused.txt = 1`)
- Window: 350x1509 at 2122,97 (render PID 1451944, loop PID 1451950)

---

## Issue 1: Text Forward Truncation in Chat Window

### Symptom

User reports text in the chat feed sometimes gets "forward truncated" — the end of
messages appears cut off, recolored, or bleeding past the element boundary.

### Root Cause Chain

Four compounding bugs create the visible truncation:

#### Bug 1A (Primary): Wrapping width overestimates drawing width by 4px

**Location:** `chat_hai_hq_render.c` line 742 vs line 1556-1557

```c
// inject_panel_feed() — wrapping uses this width:
int wrap_w = panel->w - scaled(8);           // line 742

// layout_pass() — element gets this width:
c->w = panel->w - scaled(8);                 // line 1338

// draw_elem() — label drawn at e->x + pad:
int pad = e->style.has_padding ? e->style.padding : 4;  // line 1556
int label_x = e->x + pad;                               // line 1557
```

Feed items have no CSS padding applied (the `.messages-feed .data-item` descendant
combinator is unsupported — see Bug 1C below), so `pad` defaults to **4**. The
wrapping thinks the available text width is `panel->w - scaled(8)`, but the actual
drawing area is `e->w - pad = panel->w - scaled(8) - 4`.

**Effect:** Any wrapped line that measures just under `wrap_w` overflows by ~4px
during drawing. At ~6px/char, that is approximately the last character of every
marginal wrapped line being clipped or bleeding past the element's right edge.

#### Bug 1B: No X11/Xft clip region on elements

**Location:** `chat_hai_hq_render.c` line 2624 (XftDrawCreate), no XftDrawSetClip anywhere

The `XftDraw` is created once for the entire pixmap buffer. There is no per-element
clipping (`XSetClipRectangles` or `XftDrawSetClip`). Overflow text draws unclipped
past the element boundary, across neighboring elements, until the window edge.

**Effect:** The 4px overflow from Bug 1A becomes visible — it bleeds onto the
sidebar's darker background (`#0d0e12` vs panel's `#1e2130`), creating the visual
impression that text is "recolored" or "truncated" at the element boundary.

#### Bug 1C: Dead CSS forces larger-than-intended font

**Location:** `chat-hai.css` lines 74-80, `khtpm_css_parser.c` lines 187-216

The CSS rule `.messages-feed .data-item { font-size: 10px; }` uses a descendant
combinator (space) that the parser does not support. The selector tokenization
fails silently, so the entire rule is dead code. Feed items fall back to the default
`"DejaVu Sans:pixelsize=12"` — 20% larger than the intended 10px.

**Effect:** Text occupies more horizontal space than planned, making the width
mismatch from Bug 1A worse — more lines hit the overflow boundary.

#### Bug 1D: `g_events` truncates messages at 255 chars on load

**Location:** `chat_hai_hq_render.c` line 332 (`static char g_events[MAX_EVENTS][256]`)
and line 609 (`snprintf(g_events[g_n_events], sizeof(g_events[0]), "%s", content)`)

Messages loaded from the ledger are truncated to 255 characters by `snprintf` into
the 256-byte buffer. This is a data-level truncation that happens **before**
wrapping — `wrap_lines()` never sees the full text of messages longer than 255 chars.

**Severity:** MEDIUM. Many AI replies in the gemma-lab ledger exceed 255 chars
(most are 500-2000+ chars). Their tails are silently lost.

### Recommended Fixes

| Bug | Fix | Effort |
|-----|-----|--------|
| 1A | Change line 742 to `int wrap_w = panel->w - scaled(8) - 4;` to account for draw_elem's default pad | Trivial |
| 1B | Add `XSetClipRectangles(dpy, gc, e->x, e->y, e->w, e->h, ...)` before label drawing in draw_elem | Small |
| 1C | Fix CSS selector to use class-only `.data-item` or add tag+class matching to the parser | Medium |
| 1D | Increase `g_events` buffer (e.g., `[MAX_EVENTS][512]` or dynamically) | Small |

---

## Issue 2: User Input Ignored by AI Personas

### Symptom

User typed "what movie are u all talking about? something secific?" into the composer.
The AI personas continued discussing "Hero Archetype emotional cues" and "narrative
tension" as if the question was never posted. All 3 user messages in the session's
history were ignored.

### Ledger Evidence

| Line | Time | Speaker | Content |
|------|------|---------|---------|
| 554 | 19:03:55 | nova | "Reflecting on this data – specifically echoing your assessment regarding character engagement..." |
| 555 | 19:05:36 | echo+quill+nova | (3-persona monologue about Hero Archetype tension) |
| **556** | **19:06:10** | **user** | **"what movie are u all talking about? something secific?"** |
| 557 | 19:06:33 | nova | "quill: alright! Let's consider the Hero archetype... specifically, those elements that fuel *controlled* resistance..." |

Nova's response at line 557 was committed 23 seconds AFTER the user's message, but
was **generated without seeing it** — the qwen API call was already in progress when
the user posted.

### Root Cause Chain

Five compounding problems ensure user input is effectively invisible to the AI:

#### Bug 2A (Critical): Race condition buries user messages

**Location:** `chat_hai_loop.sh` line 420 (`prev_name` extraction) + the async gap
between qwen call start (line 501) and ledger write (line 545)

The loop's pipeline:
1. `tail -n 1` of ledger → determines `prev_name` for prompt phrasing (line 420)
2. Reads `recent_context` (last 12 lines) → builds prompt (line 418)
3. Starts qwen API call → **blocks 20-40 seconds** (line 501)
4. Commits response to ledger → **AFTER** user may have posted (line 545)

When the user posts during step 3, the response generated in step 2 has no knowledge
of the user's message. The response commits at step 4, burying the user's message
between two persona lines. The next persona's `tail -n 1` returns the just-committed
persona, not the user.

**Timeline for the most recent ignored message:**
```
19:05:36  echo/quill/nova responses committed (lines 555)
19:05:53  Loop asks nova to speak; prev=echo; qwen call starts
19:06:10  USER POSTS: "what movie are u all talking about?"
19:06:33  Nova's response committed (line 557) — generated without user message
19:06:58  Loop asks quill; prev=nova (NOT prev=user)
19:07:18  Quill's response DROPPED — 67% word-overlap with own last message
```

#### Bug 2B (Critical): Prompt directs persona to reply to other personas

**Location:** `chat_hai_loop.sh` lines 460-466

```bash
if [ -n "$prev_name" ] && [ "$prev_name" != "$name" ]; then
    question="... Reply to ${prev_name} (and the group) now."
```

When `prev_name` is "nova" (due to the race condition), the prompt says "Reply to
nova." The gemma3:270m model is instructed to continue a conversation with another
persona, not address the user. Even when `prev_name` IS "user" (which is rare due
to the race), the prompt correctly says "Reply to user" — but by then the context
window problem (Bug 2C) makes the user's question invisible.

#### Bug 2C (High): Context window drowns user messages

**Location:** `chat_hai_loop.sh` line 151 (`CONTEXT_LINES=12`), line 418

Only the last 12 ledger lines are fed as context. Each persona message is 300-2000+
chars of dense monologue. The user's short question ("what movie are u all talking
about? something secific?" = ~56 chars) is **0.3%** of the 18KB context string.

The gemma3:270m model lacks the attention capacity to pick a 56-byte needle out of
an 18KB haystack of persona-generated text.

#### Bug 2D (Medium): Word-overlap gate silently drops remaining responses

**Location:** `chat_hai_loop.sh` lines 536-544

The 55% word-overlap anti-repeat gate compares a new response against the persona's
own last message. After dozens of turns of repetitive "Hero Archetype" / "narrative
tension" / "breath patterns" vocabulary, nearly every response exceeds 55% overlap
and is silently dropped.

From the live log:
```
[19:07:18] (dropped - 67% word-overlap with quill's own last message, harness anti-repeat gate)
```

This is the mechanism that dropped the few responses that might have acknowledged
the user's question.

#### Bug 2E (Medium): No recovery mechanism for unanswered user questions

**Location:** Entire `chat_hai_loop.sh`

No code tracks whether the most recent user message has received a response. No
mechanism re-injects or highlights unanswered questions. The loop cycles through
personas infinitely with no awareness of whether user input was addressed.

### Evidence Across All 3 User Messages

| User Message | Line | What Happened |
|---|---|---|
| "u guys seem to be saying the same things? do u agree?" | 58 | Echo responded at line 60 with generic "engagement and precision" — did not acknowledge the meta-question about repetition |
| "hi" | 358 | Echo responded at line 359 with "Here is a response from Echo, based on the provided information" — continued the blue-hue topic, ignored the greeting |
| "what movie are u all talking about? something secific?" | 556 | Nova committed a response at line 557 starting with "quill: alright! Let's consider the Hero archetype" — generated before user message was posted, completely oblivious |

### Recommended Fixes

| Bug | Fix | Effort |
|-----|-----|--------|
| 2A | After committing a persona response, check if a new user message appeared since `prev_name` was read; if so, re-read `tail -n 1` before next persona | Small |
| 2B | When `prev_name = "user"`, add explicit instruction: "The user just asked a question. Address it directly." | Trivial |
| 2C | When a `user:` line appears in context, prepend it prominently: "THE USER ASKED: [message]" at the top of the prompt, not buried in raw context | Small |
| 2D | Raise overlap threshold to 70%, or exempt responses that contain user-addressing keywords | Trivial |
| 2E | Track "last_unanswered_user_msg" in a state file; inject it into every persona prompt until answered | Medium |

---

## Summary

| Issue | Root Cause | Severity | Confidence |
|-------|-----------|----------|------------|
| Text forward truncation | 4px width mismatch between wrap_w and draw_elem's pad; no clipping | HIGH | HIGH (code-verified) |
| User input ignored | Race condition + context drowning + prompt wording + overlap gate | CRITICAL | HIGH (ledger-timeline-verified) |

Both issues are systemic — not one-off glitches. The truncation affects every wrapped
message line. The user-input problem affects every user message (3/3 ignored).
