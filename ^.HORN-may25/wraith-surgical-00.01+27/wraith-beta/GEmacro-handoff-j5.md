# GEmacro-handoff-j5.md
**Date:** 2026-06-05
**Status:** REQUIRED PRE-READING FOR WRAITH ARCHITECTURAL FIX

## Overview
Wraith is currently in a "broken but aligned" state. The architectural road network matches TPMOS, but the runtime behavior is blocked by tight-coupling race conditions and stale state synchronization.

**DO NOT START CODING** until you have internalized the following context.

---

## 1. Mandatory Context Set (Read in this order)
The next agent **MUST** review these documents to understand why the current codebase is fragile and how it must behave to be "honest."

| Document | Purpose |
| :--- | :--- |
| `gem-opinion-j5.txt` | **Primary Directive:** The architectural fix plan and actionable roadmap. |
| `last-session-j5/architectural-assumptions-j5.txt` | Defines the desired runtime behavior (what "fixed" looks like). |
| `tpmos-discipline-clues-j5.txt` | Defines the behavioral constraints required by TPMOS. |
| `mac/mac-handoff-j5.txt` | Diagnostic results; clarifies why previous attempts failed. |
| `mac/mac-state-j5.txt` | Audit of race conditions ("Two Pulses" and "Stale Clock" issues). |
| `1.TPMOS_c_+rmmp.0102.0018/!.gem-flashlite--yolo/!.evo&obs/!.TPMOS_ONBORD_BIBLE_10.md` | The Canonical TPMOS source of truth. |

---

## 2. Immediate Tactical Priority
Follow the four-phase roadmap defined in `gem-opinion-j5.txt`:

1.  **Phase A (Atomic Updates):** Implement `rename()`-based atomic state writing in `wraith-pm_manager.c`.
2.  **Phase B (Render Discipline):** Enforce Parser-only rendering keyed *exclusively* to `frame_changed.txt` growth (no eager `compose_frame()`).
3.  **Phase C (Robust Polling):** Refactor `history_thread` polling to use the `fuzz-op` truncation-safe pattern.
4.  **Phase D (Decommission Compositor):** Transition to KVP-variable-based taskbar rendering.

---

## 3. TPMOS Reference Patterns (Where to look)
If you are unsure how to implement something, do not invent. Copy these exactly:

- **For Atomic Input/State Update:** `projects/fuzz-op/manager/fuzz-op_manager.c`
- **For Parser-Owned Rendering:** `projects/op-ed/`
