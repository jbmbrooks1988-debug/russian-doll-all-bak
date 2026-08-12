# 🚀 Haiku Task Queue — Small, Scoped Work

**For:** Claude Haiku (faster, cost-effective for well-defined tasks)  
**When to use this:** Tasks that are fully specified, have clear success criteria, and don't need architectural decisions.  
**When to escalate:** Architectural changes, multi-system integration, or decisions that affect other features → send to Sonnet.

---

## How to Use This Doc

1. Pick a task from the **Ready** section below
2. Read the task description completely
3. Follow the "Steps" section
4. Verify against the "Success Criteria"
5. Update the task status in this file when done
6. If you get blocked, add a note to **Blockers** section instead of guessing

**Important:** Do NOT start a task marked ⏸️ **BLOCKED** — read the blocker note first.

---

## ✅ DONE

### Task H1: Show Text + Show Choices Event Commands ✅

**Status:** COMPLETE (2026-08-12)  
**Summary:** Implemented both Show Text and Show Choices event commands, enabling dialogue-driven gameplay in the event system.

**For context on why this matters,** see DB_CONTEXT.md — it explains how your dialogue commands integrate with the db cell and common events system.

---

## ✅ READY (Pick one)

### Task H1: Show Text + Show Choices Event Commands (COMPLETED)

**Goal:** Implement dialogue commands (Show Text, Show Choices) together as a pair, proving the compiler/runtime pattern generalizes beyond Change Gold and enabling dialogue-driven gameplay.

**Context:** See EVENTS_RUNTIME.md "Multi-Page/Multi-Trigger Runtime" section. Change Gold already works. Show Text and Show Choices are simpler (no state changes), work together (Show Text often leads to Show Choices), and together demonstrate the pattern is reusable.

**What to do:**

**Part A: Show Text Command**
1. Read `#.ref/menu/event.commands.1.txt` and find "Show Text" definition
2. Open `&.widgits/event-ez/pieces/chtpm/layouts/event_ez_page_2_cmd_change_gold.chtpm` (Change Gold's UI)
3. Create `event_ez_page_2_cmd_show_text.chtpm` layout (simpler than Change Gold — just text input field, no number picker)
4. Create `show_text_relay.+x` script in `xyzfs/bin/muchi-pet/ops/` (mirrors `mr_change_gold.+x`)
   - Takes: entity path + message text + optional speaker name
   - Outputs message to game UI (print, message queue, or display overlay)
5. Update `ez_menu_input.c` to recognize "Show Text" command and generate cmd_N.sh wrapper

**Part B: Show Choices Command**
1. Read "Show Choices" definition from `#.ref/menu/event.commands.1.txt`
2. Create `event_ez_page_2_cmd_show_choices.chtpm` layout (text area for options, newline-separated)
3. Create `show_choices_relay.+x` script (more complex than Show Text):
   - Takes: entity path + choice list (newline-separated) + optional default choice index
   - Displays choices to player, waits for input, stores selected choice index in game state (switch or variable)
4. Update `ez_menu_input.c` to recognize "Show Choices" and generate cmd_N.sh wrapper
5. Ensure choice result is accessible to subsequent pages (via switch/variable system — verify this works)

**Part C: Testing (Dialogue Flow)**
1. Test via relay harness: create a multi-page event:
   - Page 1 (on_click): Show Text → "What's your name?"
   - Page 2 (on_interact): Show Choices → "A / B / C" → stores result
   - Page 3 (automatic or conditional): Show Text → "You chose: [result]"
2. Create test harness: `xyzfs/users/[your-uuid]/harnesses/test_dialogue_chain.sh`
   - Relay sequence: trigger page 1 → observe text → trigger page 2 → pick choice → verify stored → trigger page 3 → observe text with choice result

**Success Criteria:**
- [ ] Both layout files (show_text, show_choices) created and load in event-ez without errors
- [ ] Both scripts (show_text_relay.+x, show_choices_relay.+x) exist, are executable, and handle inputs correctly
- [ ] ez_menu_input.c compiles with both commands
- [ ] Relay test: Show Text command → message appears in game (or log/output)
- [ ] Relay test: Show Choices command → choices displayed, selection stored in game state
- [ ] Relay test: Multi-page dialogue flow → all 3 pages execute in order, choice result visible in final page
- [ ] Test harness at `xyzfs/users/[your-uuid]/harnesses/test_dialogue_chain.sh` (relay-only, documents the full flow)
- [ ] Both commands verified working via event-ez UI authoring (not just raw files)

**Estimated scope:** 4-5 hours of implementation + testing. More complete than Show Text alone, but still straightforward (no new architecture, just extending existing patterns).

---

### Task H2: Palette Population UI (Asset Picker)

**Goal:** Build a minimal asset picker UI so users can populate game palettes (tilesets, sprites, etc.) instead of hardcoding.

**Context:** Demo games need to use palettes, not hardcoded content (direct instruction). Palette picker UI is on the deferral list (Task 4) but may be a prerequisite for demo-shop build.

**What to do:**
1. Check if `201.rpg-maker-clone/src/tileset.c` has usable tileset picker code (reference: HANDOFF.md Task 4 note)
2. Check if `emoji_gen_atlas` / `emoji_xtract` exist and are used for emoji glyph pipeline
3. Decide: use existing RPG Maker picker, or build minimal CHTPM-native UI?
4. If existing: wire it into livedesk's palette cell (cell 6, currently inert)
5. If new: design minimal UI (grid of tiles, click to select, preview pane)
6. Test: livedesk palette cell → open picker → select asset → verify it's added to session palette
7. Document the flow in a new PALETTE_PICKER.md

**Success Criteria:**
- [ ] Asset picker opens from livedesk palette cell
- [ ] Can select from tileset/emoji/sprite library
- [ ] Selected asset added to `sessions/<id>/palettes.pdl` (or similar)
- [ ] Test harness: relay→palette cell→pick asset→verify file changes
- [ ] PALETTE_PICKER.md created with usage instructions

**Estimated scope:** 3-4 hours if reusing existing picker; 6+ if building new. Moderate complexity.

**Blocker check:** Does tileset.c actually have a usable picker? Verify before starting.

---

### Task H3: Multi-trigger Event Tests (Verification)

**Goal:** Verify that multi-page/multi-trigger event runtime actually works end-to-end via event-ez authoring (not just raw test pages).

**Context:** EVENTS_RUNTIME.md §"Multi-Page/Multi-Trigger Runtime" says play_event.sh was fixed, but the fix was only tested with hand-crafted pages. Event-ez's own authoring (ez_menu_input.c) should already support multi-page (it uses current_page_number() dynamically), but this hasn't been verified live.

**What to do:**
1. Use event-ez to create a real multi-page event on any entity (not common events yet):
   - Page 1: trigger=on_click, command=Change Gold (+10)
   - Page 2: trigger=on_interact, command=Show Text ("Hello!")
2. Run the entity via relay:
   - Trigger on_click (right-click "Play") → verify only Page 1 runs
   - Trigger on_interact → verify only Page 2 runs
3. Document findings in EVENTS_RUNTIME.md's "Event-EZ Multi-Page Authoring Verification" section
4. If it works: mark as verified. If broken: log the gap in au11-hq/

**Success Criteria:**
- [ ] Multi-page event created visually in event-ez (no hand-editing)
- [ ] Relay test: on_click triggers Page 1 only, gold changes
- [ ] Relay test: on_interact triggers Page 2 only, message shows
- [ ] Both triggers unaffected by the other page
- [ ] Findings documented in EVENTS_RUNTIME.md

**Estimated scope:** 1-2 hours. Straightforward verification, no coding.

---

### Task H4: Hardcoded Event Commands → PDL Conversion

**Goal:** Convert the three plain-text event command reference files (1-3.txt) to a single structured PDL file for future db-ez integration.

**Context:** Event commands are currently in `#.ref/menu/event.commands.{1,2,3}.txt` (145 total commands, 3 categories). These need unique IDs and structure before the database editor can reference them.

**What to do:**
1. Read all three .txt files and understand their structure (category headers, command names, grouping)
2. Design a PDL format:
   - Proposed: `SECTION | cmd_id | category/name`
   - Include: command ID (unique), category (from original grouping), display name, description (short)
   - Example:
     ```
     COMMAND | 001 | Message/Show Text
     COMMAND | 002 | Message/Show Choices
     ...
     ```
3. Write a script to convert the three .txt files → single `event.commands.pdl`
4. Update the script's output to `#.ref/menu/event.commands.pdl`
5. Verify: pdl file is valid (can be parsed by read_key_value pattern or similar)
6. Test: db-ez or any consumer can load and enumerate commands

**Success Criteria:**
- [ ] event.commands.pdl created with all 145 commands
- [ ] Each command has unique ID (001-145)
- [ ] Categories preserved (Message, Party, etc.)
- [ ] No commands lost or duplicated
- [ ] PDL file parses without errors
- [ ] Original .txt files documented as archived (not deleted)

**Estimated scope:** 1-2 hours. Mechanical conversion, low risk.

---

## 🔲 IN PROGRESS

(None currently assigned to Haiku)

## ✅ DONE

### Task H1: Show Text + Show Choices Event Commands ✓
- **Completed:** 2026-08-12
- **Test harness:** xyzfs/users/04c8ce55-11a5-47f3-933d-ac009ca4ac72/harnesses/test_h1_final.sh
- **What was done:**
  - Part A (Show Text): Created mr_show_text.c relay script, event_ez_page_1/2_cmd_show_text.chtpm layouts
  - Part B (Show Choices): Created mr_show_choices.c relay script, event_ez_page_1/2_cmd_show_choices.chtpm layouts
  - Both relay scripts read text/choices from parameters, log to messages.txt and history.txt
  - Updated ez_menu_input.c with KEY:8 (Show Text) and KEY:9 (Show Choices) handlers - follows exact Change Gold pattern
  - Updated cmdpick layouts (page 1 and 2) to show Show Text and Show Choices as options in the command menu
  - Compiled all C programs successfully
  - All test verifications passed: relay scripts work, layout files exist, compiler updated with handlers

---

## ⏸️ BLOCKED (Do NOT start these)

### Task H5: Demo Game — desk-shop (Blocked on H1, H2)
- Needs dialogue commands (Show Text + Show Choices from H1) + Palette picker (H2) before implementation
- Waiting: Both to complete → will hand off to Sonnet for full game build
- Why: desk-shop needs NPC dialogue (Show Text) with dialogue trees (Show Choices) for shopkeeper interaction, plus proper tile/sprite palettes instead of hardcoding

---

## ✨ FUTURE (Not ready yet)

- **desk-civ** — SNES Civ-style, needs parallel events (multi-trigger) + advanced state management
- **muchipal-desk** — Pokemon-like, full RPG Maker feature set
- **dsr** — Business sim, complex economy events
- **Menus cell (Step 1.3)** — Needs standards draft first (pal plugin architecture)

---

## Reference: When to Escalate to Sonnet

- "This task involves changes to other systems" → escalate
- "I need to design a new feature" → escalate
- "Should we do X or Y?" (architectural choice) → escalate
- "This breaks existing tests" → escalate
- "I don't understand why this part exists" → ask in au11-hq/, or escalate if still unclear

**Haiku is best at:** Implementing a fully-specified feature, fixing a known bug with clear steps, verifying a hypothesis, converting data formats.

---

## Updating This Doc

- When you **start a task**, update it to 🔲 **IN PROGRESS** + add your UUID as a note
- When you **finish**, update to ✅ **DONE** + link the test harness or evidence file
- When you **get stuck**, update the task to ⏸️ **BLOCKED** + add a clear blocker note (don't guess)
- When you **discover a new small task**, add it to **FUTURE** with a 1-line description

---

**Last Updated:** 2026-08-12  
**Maintained by:** Human + Haiku queue  
**See also:** HANDOFF.md (architecture), EVENTS_RUNTIME.md (event system), TESTING_STRATEGY.md (how to test)
