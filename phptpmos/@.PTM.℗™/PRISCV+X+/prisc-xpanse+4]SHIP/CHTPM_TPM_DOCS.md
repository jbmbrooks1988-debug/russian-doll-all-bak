# CHTPM + TPM System Documentation (v2.0)
**Date:** 2026-03-05
**Status:** Authoritative Standard for Decoupled Piece-Module Architecture

---

## 1. The Three Stages of the Pipeline

The system is organized into three distinct layers of responsibility, moving from the user interface down to the atomic logic.

### Stage 1: CHTPM (The Theater / View Layer)
*   **Responsibility:** UI Rendering, Layout Management, and Input Routing.
*   **Components:** 
    *   `.chtpm` Layouts: XML-like files defining the UI structure using `<button>`, `<text>`, and `<menu>` tags.
    *   `chtpm_parser.c`: The engine that substitutes `${variables}` and composes the final frame.
*   **Mantra:** "The Parser is the mirror's eye, not the mirror's mind." It should never have app-specific logic.

### Stage 2: PMO (The OS / Orchestration Layer)
*   **Responsibility:** Process lifecycle, Application Launching, and Location Resolution.
*   **Components:**
    *   `orchestrator.c`: Manages concurrent execution of the Parser, Manager, and Input Dispatchers.
    *   `location_kvp`: A registry file used to resolve paths dynamically, ensuring the system is "location-agnostic."
    *   `proc_manager.c`: Handles signals (STOP/CONT) and tracks active PIDs.

### Stage 3: TPM (The Stage / Logic Layer)
*   **Responsibility:** Atomic Data (Pieces) and Sovereign Logic (Modules).
*   **Components:**
    *   **Pieces:** Directories containing `.pdl` (DNA/Truth) and `state.txt` (Mirror/Speed).
    *   **Modules (Managers):** The "Stage Managers" (e.g., `fuzzpet_manager.c`) that interpret input and update Piece states.
    *   **Traits (Plugins):** Generic logic blocks (e.g., `move_entity.c`) shared across multiple pieces.

### Stage 4: System Daemons (Background Services)
*   **Responsibility:** Autonomous background processes that update state without user input.
*   **Components:** 
    *   `clock_daemon.c`: Ticks time, updates `state.txt` via `piece_manager`, hits `frame_changed.txt`.
*   **Pattern:** Daemons run in parallel, update state continuously, and signal the global frame marker to trigger re-render.
*   **Mantra:** "The clock ticks whether you watch it or not."

---

## 2. The I/O Pipeline Flow

To ensure an auditable and synchronized system, data flows through a strict file-based pipeline:

1.  **INPUT:** `keyboard_input` writes raw ASCII codes to `pieces/keyboard/history.txt`.
2.  **ROUTING:** `chtpm_parser` reads the history.
    *   **Nav Mode:** Parser uses keys to move focus `[>]`.
    *   **Interact Mode:** Parser "injects" keys into the active Module's `history.txt`.
3.  **LOGIC:** The Module (Manager) consumes keys.
    *   Maps `KEY:n` to the n-th method in the Piece's `.pdl`.
    *   Updates state via `piece_manager`.
4.  **RESPONSE:** The Module generates a sovereign `view.txt` (the "Stage") and logs a response.
5.  **RENDER:** The Parser detects the change, substitutes `view.txt` into `${game_map}`, and writes the final composite frame to `display/current_frame.txt`.
6.  **DISPLAY:** The `renderer` prints the frame to the terminal.

---

## 3. Core Standards & Patterns

### The Marker-File Pulse (KISS Protocol)
Filesystem `mtime` (modification time) has a 1-second resolution limit, which is too slow for 60 FPS. 
*   **Solution:** Every writer appends 1 byte to `pieces/display/frame_changed.txt`. 
*   **Result:** Readers monitor the **file size** for sub-millisecond synchronization.

### DNA vs. Mirror Sync
*   **DNA (.pdl):** The auditable source of truth. Structured and slow to parse.
*   **Mirror (state.txt):** A flat `key=value` file optimized for high-speed reads by the Parser.
*   **Mandate:** Every logic update must sync both.

### Generic Method Mapping (Decoupling)
Layouts use `onClick="KEY:2"`. The Manager looks up the 2nd method in the `.pdl` (skipping `move` and `select`). This allows the same UI to work for a Pet (KEY:2 = Feed) or a Selector (KEY:2 = Scan) without recompiling the Parser.

---

## 4. Pitfalls & Lessons Learned

### Irregular Verb Responses
When mapping methods to responses (e.g., "play" -> "played"), watch for irregular verbs.
*   **Fail:** "feed" -> "feeded" (Missing from .pdl).
*   **Fix:** Explicitly map "feed" -> "fed" and "sleep" -> "slept" in the Manager's logic.

### Buffer Truncation (-Wformat-truncation)
In C, using `snprintf` with a buffer size equal to the max potential string length triggers warnings.
*   **Standard:** Always double your buffer sizes for paths (e.g., `4096` for a `2048` root) and use `sizeof(buffer)` to ensure safety.

### The "Always Pulse" Trap
In a multi-process system, a Module must poll its history continuously. 
*   **Anti-Pattern:** Only pulsing when a frame change is detected. This causes missed inputs if the timing isn't perfect.

---

## 5. FAQ

**Q: How do I add a new button to a Pet?**
A: Add a `METHOD | new_action | void` line to the `.pdl`. The Parser will automatically generate the button. Update the Manager's `route_input` to handle the new action if it requires special logic.

**Q: Why is the screen flickering?**
A: Flickering usually means two processes are writing to `current_frame.txt`. Only the `chtpm_parser` should be the final writer.

**Q: How does joystick input work?**
A: The `joystick_input` plugin writes codes (2000+) to history. The Parser maps these to `KEY:n` commands or movement triggers just like keyboard keys.

---
**Mantra:** "If it's not in a file, it's a lie."
