# 🎭 Chapter 12: The Simulation Theater: CHTPM & View.txt
The Theater is where the "Reality" of the file system is translated into "Experience" for the user. In v6.00, this is handled by the **CHTPM Parser**. 🎞️📺

---

## 📽️ The Rendering Chain
The Theater does not "calculate" graphics; it **Composes** them.

1.  **Observation:** The Parser watches `frame_changed.txt` (The Pulse).
2.  **Substitution:** It reads the layout (`.chtpm`) and replaces `${variables}` with values from the state files.
3.  **Composition:** It calculates the layout (Rows/Cells) to ensure the ASCII box-drawing characters align perfectly.
4.  **Display:** It writes the final frame to the terminal or the `view.txt` buffer.

---

## 🔣 Variable Engine: The `${var}` Pattern
Variables are the bridge between the Brain and the Theater.
*   **Static Vars:** `${project_id}`, `${version}`.
*   **Dynamic Vars:** `${player_hp}`, `${current_location}`.
*   **Global Vars:** `${clock}`, `${cpu_load}`.

### State-First Polling
To keep CPU usage at 0%, the Theater only renders when a file changes. If the user does nothing, and the AI does nothing, the screen remains static and the process sleeps.

---

## 💻 Code Example: Substitutor Snippet
```c
void substitute(char* src, char* dst) {
    char* p = strstr(src, "${");
    if (p) {
        char var_name[64];
        extract_name(p, var_name);
        char* value = get_state_value("pieces/player", var_name);
        replace_text(src, p, value, dst);
    }
}
```

---

## 🏛️ Scholar's Corner: The "Blinking Cursor" Bug
In v5.01, a bug caused the Theater to re-render every time the system clock updated (once per second). While this seemed fine, it meant the OS was never truly "at rest." In v6.00, the clock is now a **Reactive Variable**. It only triggers a render if a Piece is specifically "Listening" to the time. This saved 15% battery life on mobile devices! 🔋🕒

---

## 📝 Study Questions
1.  Explain the four steps of the Rendering Chain.
2.  What is "State-First Polling" and why is it efficient?
3.  Difference between a Static and Dynamic variable in CHTPM?
4.  **Scenario:** You update `state.txt` but the screen doesn't change. What did you forget to do?

---
[Return to Index](INDEX.md)
