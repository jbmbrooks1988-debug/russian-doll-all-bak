# 📂 Chapter 2: The File System as the Source of Truth
In TPMOS, the file system isn't just storage; it is the **active state of the universe**. We do not use databases; we use directories. 📁🌲

---

## 🏗️ The Standardized Directory Structure
Every project follows a strict anatomy to be compatible with **Fondu**.

```text
projects/my_app/
├── project.pdl          # Project metadata & entry point
├── layouts/             # CHTPM Markup files (The View)
├── manager/             # C/PAL Brain (The Controller)
├── ops/                 # C binaries (The Muscle)
│   ├── ops_manifest.txt # List of exposed capabilities
│   └── +x/              # Compiled binaries
├── pieces/              # The Souls (The Data)
│   └── player_01/
│       ├── piece.pdl
│       └── state.txt
└── assets/              # Icons, Artifacts, Audio
```

---

## 🫕 The Ops Registry (`pieces/os/ops_registry/`)
When a project is installed via `fondu`, its ops are registered in a central registry. This allows any project to call another project's "Muscles".

**Example `ops_registry/user.txt`:**
```text
user::create_profile=projects/user/ops/+x/create_profile.+x
user::move_entity=projects/user/ops/+x/move_entity.+x
```

### 🧠 The Thin Brain Pattern
A Standardized Ops Module (`manager.c`) follows this logic:
1.  **Poll**: Read `history.txt` or `gui_state.txt`.
2.  **Match**: See if the user clicked a button or typed a command.
3.  **Execute**: Look up the op in the registry and fork a process.
4.  **Reset**: Clear the input buffer.

---

## 💾 Data Sovereignty & The Mirror
Each Piece has a folder. Inside that folder, `state.txt` is the **Mirror**.
*   **Atomic Updates:** We use `rename()` to ensure state updates are atomic. Write to `state.txt.tmp`, then swap.
*   **Key-Value Pairs:** State is always `key=value`. No binary blobs.

### 📜 Master Ledger
Every state-changing Op MUST log to the global `master_ledger.txt`. This allows for "Time Travel Debugging" - you can literally replay the ledger to see how the system reached a specific state.

---

## 💻 Code Example: Reading State in C
The following utility is used across all "Thin Brain" managers to read state efficiently.

```c
char* get_state_value(const char* piece_path, const char* key) {
    char state_path[512];
    snprintf(state_path, sizeof(state_path), "%s/state.txt", piece_path);
    FILE* f = fopen(state_path, "r");
    if (!f) return NULL;

    static char value[256];
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(line, key) == 0) {
                char* val = eq + 1;
                val[strcspn(val, "\n\r")] = 0;
                strncpy(value, val, 255);
                fclose(f);
                return value;
            }
        }
    }
    fclose(f);
    return NULL;
}
```

---

## 🏛️ Scholar's Corner: The "Directory Ghost"
There's a famous bug report from v4.2 where a developer deleted a folder but the "Piece" kept appearing in the UI. It turned out the Renderer was caching directory listings for 5 seconds to save battery. This led to the concept of the **"Directory Ghost"** - a piece of data that exists in the "Theater" but not in "Reality." In v6.00, we've implemented **State-First Polling**, ensuring the Theater only renders what the File System confirms exists. 👻📁

---

## 📝 Study Questions
1.  What is the `ops_manifest.txt` and why is it important for Fondu?
2.  Explain the "Thin Brain" pattern.
3.  Why does TPMOS use `rename()` for state updates?
4.  How does a centralized `ops_registry` enable cross-project collaboration?

---
[Return to Index](INDEX.md)
