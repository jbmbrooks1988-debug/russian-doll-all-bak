# 🚀 Chapter 3: The Standardized Pipeline
In TPMOS v6.00, the pipeline from user input to visual feedback is more structured than ever. This is the **Standardized Ops Pipeline**. ⚙️

---

## 🔄 The Feedback Loop
The loop consists of four distinct phases, all mediated by the file system.

1.  **INPUT (The Sensation) ⌨️**
    *   User types in the CLI or clicks a CHTPM button.
    *   Input is written to `pieces/keyboard/history.txt` or `pieces/apps/gl_os/session/gui_state.txt`.

2.  **BRAIN (The Module) 🧠**
    *   A background manager (e.g., `playrm_manager.+x`) polls the input files.
    *   It identifies a command (e.g., `MOVE:up`).
    *   It looks up the corresponding **Op** in the Standardized Registry.

3.  **MUSCLE (The Op) 💪**
    *   The manager forks a process to run the Op binary (e.g., `move_entity.+x`).
    *   The Op modifies the **Mirror** (`state.txt`) of the target Piece.
    *   The Op updates the `frame_changed.txt` pulse.

4.  **THEATER (The View) 🎭**
    *   The `chtpm_parser` detects the pulse.
    *   It re-reads the layouts and substitutes variables (e.g., `${player_x}`).
    *   It composes the final ASCII or GL frame.

---

## 🫕 Fondu and the Registry
The **Standardized Pipeline** relies on every project being correctly registered.

### The Lifecycle of a Pulse
When an Op finishes its work, it doesn't just exit. it must signal the OS that "Reality has shifted."
```c
// pulse.c - Standard Op Signal
void trigger_pulse() {
    FILE* f = fopen("pieces/display/frame_changed.txt", "w");
    if (f) {
        fprintf(f, "%ld", (long)time(NULL));
        fclose(f);
    }
}
```

---

## 🧠 Thin Brain vs. Heavy Brain
*   **Legacy (Heavy Brain):** The manager did everything. It read state, calculated physics, updated files, and rendered. This led to "Sync Lag."
*   **Standardized (Thin Brain):** The manager only routes.
    *   *Input:* "I want to jump."
    *   *Manager:* "Okay, I will tell the `jump` muscle to handle that."
    *   *Manager:* (Forks `jump.+x` and goes back to listening).

---

## 💻 Code Example: The Thin Brain Loop
```c
while (keep_running) {
    if (check_history_changed()) {
        char* last_cmd = get_last_history_line();
        if (strstr(last_cmd, "MOVE:")) {
            char* direction = last_cmd + 5;
            execute_op("user::move_entity", direction);
        }
        clear_history();
    }
    usleep(10000); // 100Hz polling = CPU Friendly
}
```

---

## 🏛️ Scholar's Corner: The "Pulse That Broke the World"
In the early days of the Standardized Pipeline, a developer accidentally wrote a "Infinite Pulse." An Op would trigger a pulse, which triggered a Module, which triggered the same Op... creating a feedback loop that froze the entire OS and heated up the CPU to 90°C. This incident led to the **Pulse Cooldown Rule**: "No piece may trigger more than 60 pulses per second." 🌡️💥

---

## 📝 Study Questions
1.  Describe the four phases of the Standardized Ops Pipeline.
2.  How does `frame_changed.txt` facilitate communication between the Muscle and the Theater?
3.  Why is the "Thin Brain" model more stable than the "Heavy Brain" model?
4.  **Scenario:** You click a button, but nothing happens. Which phase of the pipeline is likely failing?

---
[Return to Index](INDEX.md)
