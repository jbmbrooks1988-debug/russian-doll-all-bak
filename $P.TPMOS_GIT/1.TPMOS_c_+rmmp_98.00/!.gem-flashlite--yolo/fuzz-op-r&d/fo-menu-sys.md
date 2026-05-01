# Fuzz-Op Dynamic Menu System (Trait Menus)

This document provides a technical guide for implementing dynamic, PDL-driven menus in TPMOS projects, modeled after the `fuzz-op` pet simulation.

## 1. Core Philosophy
The Fuzz-Op menu system follows the **Thin Brain** philosophy. The UI (CHTPM) is a hollow shell that reflects the state and capabilities (Methods) of the active Piece. Capabilities are defined in the Piece's DNA (`piece.pdl`) and are dynamically rendered as interactive buttons.

## 2. Architecture Overview

### A. The Layout (`.chtpm`)
The layout simply provides a slot for the methods using the `${piece_methods}` variable.
```xml
<text label="║  " />${piece_methods}<text label=" ║" /><br/>
```

### B. The Parser (`chtpm_parser.c`)
The parser is responsible for:
1.  **Resolution**: Finding the PDL for the `active_target_id`.
2.  **Extraction**: Parsing `METHOD` lines (skipping internal ones like `move`, `select`).
3.  **Generation**: Creating `<button>` elements with `onClick="KEY:n"` (indexing usually starts at 2).
4.  **Injection**: Mapping button clicks to ASCII key injections (e.g., `KEY:2` -> `50` in `history.txt`).

### C. The PDL (`piece.pdl`)
Methods are defined as `SECTION | KEY | VALUE` where `VALUE` is the command/Op to execute.
```pdl
METHOD | scan    | pieces/apps/playrm/ops/+x/scan_op.+x xlector
METHOD | feed    | projects/fuzz-op/ops/+x/feed_op.+x xlector
```

### D. The Manager (`_manager.c`)
The manager acts as the dispatcher. It reads the history file, identifies the method index, resolves the method name/handler via the PDL, and executes it.

## 3. The Lifecycle of an Action

1.  **Render**: Parser reads `active_target_id` (e.g., `fuzzball`). It loads `fuzzball/piece.pdl`, finds methods `feed`, `play`, `sleep`. It renders them as buttons `[2] feed`, `[3] play`, `[4] sleep`.
2.  **Input**: User presses `2` or clicks the `feed` button.
3.  **Injection**: Parser calls `send_command("KEY:2")` which writes ASCII `50` to `pieces/apps/player_app/history.txt`.
4.  **Dispatch**: Manager reads `50`, subtracts `'0'` to get index `2`. 
5.  **Resolution**: Manager calls `pdl_reader.+x fuzzball list_methods`. It takes the 2nd item (e.g., `feed`).
6.  **Execution**: Manager calls `pdl_reader.+x fuzzball get_method feed` to get the handler Op. It then calls `system(handler)`.
7.  **Sync**: Manager updates state, calls `trigger_render()`, and pulses `state_changed.txt`.

## 4. Implementation Guide for Agents

### Step 1: Define PDL Methods
Ensure your piece has a `piece.pdl` with `METHOD` entries.
- Use `void` for internal methods.
- Use full paths for Ops.

### Step 2: Set Active Target
In your manager, ensure `active_target_id` is set to the piece you want to control.
```c
fprintf(state_file, "active_target_id=%s\n", target_id);
```

### Step 3: Implement Dynamic Dispatcher
Copy the dispatch pattern from `fuzz-op_manager.c`:
1.  Detect keys in range (2-9).
2.  Use `pdl_reader.+x` to find the method name at that index.
3.  Execute the handler string returned by `pdl_reader.+x`.

## 5. Critical Pitfalls (2026 Edition)

| Pitfall ID | Name | Description | Fix |
|:---|:---|:---|:---|
| **#80** | **ASCII Injection** | Parser injecting raw `2` instead of `'2'` (50). | Ensure `inject_raw_key('0' + k)` is used in `send_command`. |
| **#93** | **Stale Methods** | Methods don't refresh when `active_target_id` changes. | Manager MUST pulse `state_changed.txt` after updating target. |
| **#31** | **Project ID Drift** | App layouts loaded via `href` may have stale `project_id`. | Explicitly set `project_id` when launching app or loading layout. |
| **#82** | **Triple Render** | Calling `compose_frame()` multiple times per keypress. | Write to `frame_changed.txt` marker instead of setting `dirty=1`. |
| **#26** | **Button vs Text** | Writing `[2] Feed` as text instead of `<button>`. | Use `<button label="Feed" onClick="KEY:2" />`. |
| **#N/A** | **Index Desync** | Manager and Parser starting at different indices. | Standardize: Methods start at index 2 (1 is reserved for Map/Main). |
| **#2d** | **Path Drift** | Hardcoding `pdl_reader` paths. | Always resolve `project_root` from `location_kvp`. |

## 6. Pro-Tip: Response Conjugation
For better UX, the manager should conjugate method names in the `last_response` field.
- `feed` -> `fed`
- `scan` -> `scanned`
- `inventory` -> `inventoried`

---
*Documented by Gemini CLI for TPMOS Future Agents - 2026-04-21*
