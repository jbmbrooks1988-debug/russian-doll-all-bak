# 🌐 Chapter 15: Cross-Platform: Mac, Linux, and Windows
TPMOS v6.00 is designed to be **Sovereign**—it does not care what hardware it runs on. This is achieved through the **Unified Shell Layer**. 💻🍎🐧

---

## 🐚 The Unified Shell (.sh & .ps1)
Every project includes two launch scripts:
1.  `button.sh`: For Mac and Linux (Unix-like systems).
2.  `button.ps1`: For Windows (PowerShell).

These scripts perform the same three tasks:
*   **Verify Environment:** Check if `gcc` and `make` are installed.
*   **Resolve Paths:** Use `location_kvp` to find the project root.
*   **Trigger Pulse:** Ensure the Theater knows a new session has started.

---

## 📂 Path Normalization
Windows uses `\`, Mac/Linux use `/`.
TPMOS solves this by:
*   **Internal Standard:** All internal paths in `.pdl` and `.chtpm` use `/`.
*   **Path Wrapper:** A C-utility that converts `/` to `\` on Windows at runtime.

---

## 🏛️ The Portability Mandate
A project is only "Fondu-Compatible" if it can be compiled and run on all three platforms with a single command: `./button.sh` or `./button.ps1`.

### Platform Detection in C
```c
#ifdef _WIN32
    #include <windows.h>
    #define CLEAR_CMD "cls"
#else
    #include <unistd.h>
    #define CLEAR_CMD "clear"
#endif

void clear_screen() {
    system(CLEAR_CMD);
}
```

---

## 🏛️ Scholar's Corner: The "Windows Wall"
In the early days of v5.01, the Windows version of TPMOS was 20% slower because of how Windows handles file locking. This was called the **"Windows Wall."** The solution was the **Atomic Swap Pattern** (Chapter 2)—by writing to a `.tmp` file and using `rename()`, we bypassed the locking issues and made TPMOS equally fast on all platforms. 🧱🚀

---

## 📝 Study Questions
1.  How does the Unified Shell Layer ensure cross-platform compatibility?
2.  Explain the "Path Wrapper" concept.
3.  What is the "Windows Wall" and how was it solved?
4.  **True or False:** A project must have separate C code for every OS.

---
[Return to Index](INDEX.md)
