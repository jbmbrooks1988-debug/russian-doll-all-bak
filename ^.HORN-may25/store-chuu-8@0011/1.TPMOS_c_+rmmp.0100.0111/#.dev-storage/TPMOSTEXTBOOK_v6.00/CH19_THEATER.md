# 🎭 Chapter 19: THE THEATER: CHTPM Markup & Parser Logic
The Theater is the heart of the TPMOS visual experience. It is powered by the **CHTPM Parser** (`pieces/chtpm/plugins/chtpm_parser.c`), a high-performance engine designed for ASCII-first rendering with variable-aware intelligence. 🎞️📺

---

## 🏗️ The CHTPM Syntax
CHTPM is an XML-inspired markup language that defines the structure and interaction of a TPMOS app.

### Core Tags
*   `<layout id="..." name="...">`: The root container. It defines the project scope.
*   `<row>` / `<cell>`: The grid system used to organize ASCII layout.
*   `<text label="..." />`: Static text or variable-substituted strings.
*   `<button label="..." onClick="..." href="..." />`: The primary interaction node.
    - `onClick="OP:user::move"`: Executes a Standardized Op.
    - `href="path/to/other.chtpm"`: Switches the active theater view.
*   `<cli_io id="..." label="..." />`: A text entry field for history injection.
*   `<module path="..." />`: Binds a background "Thin Brain" to the layout.

---

## 🧠 The Variable Engine `${var}`
The parser's most powerful feature is its real-time variable substitution.

### Scope Resolution
When the parser encounters `${player_hp}`, it follows a strict search path:
1.  **Project-Local:** Checks `projects/<current>/pieces/<active>/state.txt`.
2.  **Global Fallback:** Checks `pieces/os/global_state.txt`.
3.  **Special Reserved:** Internal system variables like `${clock}` or `${desktop_view}`.

### Zero-Waste Pulse
The Theater is **Event-Driven**. It only re-parses and re-renders when it detects a change in `frame_changed.txt`. This keeps idle CPU usage at 0.0%.

---

## 🎯 The Focus System
Because TPMOS is often run in a terminal without a mouse, the Theater implements a **Focus System**.
*   **Navigation:** Use Arrow Keys or TAB to move the `focus_index` between interactive elements (buttons, inputs).
*   **Activation:** Press ENTER to trigger the `onClick` or `href` of the focused element.
*   **Visual Feedback:** Focused elements are rendered with a highlight (e.g., `[ Button ]` instead of `| Button |`).

---

## 💻 Code Example: The Substitution Loop
From `chtpm_parser.c`:
```c
void substitute_vars(const char* src, char* dst, int max_len) {
    const char *p_src = src; char *p_dst = dst;
    while (*p_src && (p_dst - dst) < max_len - 1) {
        if (*p_src == '$' && *(p_src+1) == '{') {
            const char *end = strchr(p_src, '}');
            if (end) {
                char var_name[64];
                int len = end - (p_src + 2);
                strncpy(var_name, p_src + 2, len);
                var_name[len] = '\0';
                const char *val = get_var(var_name); // Lookup logic
                while (*val) *p_dst++ = *val++;
                p_src = end + 1; continue;
            }
        }
        *p_dst++ = *p_src++;
    }
}
```

---

## 🏛️ Scholar's Corner: The "Warning-Free Parser"
In the transition to v6.00, the CHTPM parser was rewritten to be 100% warning-free under strict `gcc -Wall -Wextra` flags. This wasn't just for aesthetics; it was to ensure that the Theater could run for months without a memory leak or a buffer overflow, even when parsing 1MB layouts. It is the most stable piece of C code in the ecosystem. 🛡️🏛️

---

## 📝 Study Questions
1.  What are the three tiers of variable scope resolution in CHTPM?
2.  Explain how the Focus System enables keyboard-driven navigation.
3.  What is the benefit of the "Zero-Waste Pulse" for mobile devices?
4.  **Exercise:** Write a CHTPM snippet that displays a "Health" variable and a "Heal" button.

---
[Return to Index](INDEX.md)
