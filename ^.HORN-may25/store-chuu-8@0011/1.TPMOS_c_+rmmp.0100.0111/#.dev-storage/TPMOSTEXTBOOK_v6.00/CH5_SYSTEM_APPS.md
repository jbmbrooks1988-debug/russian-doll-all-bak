# 🖥️ Chapter 5: System Apps & The Centralized Core
TPMOS v6.00 is built on a foundation of **Standardized System Apps**. These are the core utilities that manage the OS itself. 🏛️

---

## 🏛️ The Core Quartet
Every TPMOS session relies on these four pillars.

### 1. The Process Manager (`pieces/os/process_manager`)
The invisible hand that ensures Modules are running.
*   **Standardized Behavior:** It reads `compiled_projects.txt` and ensures every installed app's manager is active.
*   **Safety:** It monitors CPU usage and kills runaway "Heavy Brains."

### 2. Fondu (`pieces/system/fondu`)
The Lifecycle Authority.
*   Handles `--install`, `--uninstall`, and `--archive`.
*   Maintains the **Global Ops Catalog**.

### 3. The Playroom (`projects/playrm`)
The primary game engine and world simulator.
*   Manages 2D ASCII maps and Entity positions.
*   Uses `move_entity` and `render_map` ops.

### 4. Op-Ed (`projects/op-ed`)
The Integrated Development Environment (IDE).
*   Allows for real-time manipulation of Piece state.
*   Includes the **PAL Editor** for visual scripting.

---

## 🫕 The Global Ops Catalog
Located at `pieces/os/ops_catalog.txt`, this file is the "Phonebook" of the OS. Any process can read it to find out what capabilities are available on the system.

**Example `ops_catalog.txt`:**
```text
system::shutdown
user::login
playrm::spawn_npc
ai::inference
```

---

## 💻 Code Example: Calling a System Op from a Module
```c
void shutdown_system() {
    char* op_path = lookup_op("system::shutdown");
    if (op_path) {
        system(op_path);
    }
}
```

---

## 🏛️ Scholar's Corner: The "App Store of 2026"
In early 2026, a developer used Fondu to create a "Project Pack" that contained 50 small games. Because they all followed Standardized Ops, they all worked together instantly. A player could use their character from Game 1 to enter a shop in Game 2 and buy an item from Game 3. This became known as the **"Sovereign App Store"** - a marketplace of interoperable pieces rather than siloed applications. 🛒📦

---

## 📝 Study Questions
1.  Name the "Core Quartet" of TPMOS system apps.
2.  What is the `ops_catalog.txt` and how does it benefit developers?
3.  How does the Process Manager ensure system stability?
4.  **Critical Thinking:** Why is the Playroom considered a "System App" even though it looks like a game engine?

---
[Return to Index](INDEX.md)
