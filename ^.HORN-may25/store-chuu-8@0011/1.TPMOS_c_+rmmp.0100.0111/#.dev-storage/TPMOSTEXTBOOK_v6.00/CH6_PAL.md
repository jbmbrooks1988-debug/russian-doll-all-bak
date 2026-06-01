# 🔧 Chapter 6: PAL: The Assembly Language of TPMOS
How do you orchestrate complex operations across an entire project? You write a **PAL script**. PAL (Prisc Assembly Language) is the glue that holds the Standardized Ops architecture together. 🔗🧠

---

## 📜 What is PAL?
PAL is an assembly-style scripting language designed specifically for TPMOS. It lets you chain together **Ops** (Muscles) into complex workflows without writing new C code for every variation.

### The Instruction Set (v6.00)
PAL has a RISC-inspired instruction set, now fully aware of the **Standardized Registry**:

| Instruction | Purpose | Example |
|-------------|---------|---------|
| `OP` | Call a standardized op | `OP user::move_entity "player" "up"` |
| `call` | Call a local subroutine | `call game_loop` |
| `sleep` | Pause execution (ms) | `sleep 100` |
| `beq` | Branch if equal | `beq r0, r1, done` |
| `read_state` | Read piece state | `read_state r1, "player", "pos_x"` |
| `hit_frame` | Trigger render pulse | `hit_frame` |

---

## 🫕 PAL + Fondu Integration
In the Standardized Ops Edition, PAL scripts are considered **First-Class Citizens**. When you install a project via Fondu, its scripts are registered alongside its binaries.

1.  **Ops:** Compiled C binaries for core logic.
2.  **Scripts:** PAL `.asm` files for high-level orchestration.

Now any project can call a script as if it were a binary:
```bash
./fondu --install user
# Now call a script registered in the manifest
user::welcome_sequence.asm "new_player"
```

---

## 💻 Code Example: The PAL Interpreter Logic
The interpreter (`prisc+x.c`) handles the `OP` instruction by looking up the path in the `ops_catalog.txt`.

```c
case OP_CUSTOM: {
    // Standardized Op Lookup
    char* op_path = lookup_standardized_op(inst->custom_name);
    if (op_path) {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "%s %s %s", op_path, arg1, arg2);
        run_command(cmd);
    }
    break;
}
```

---

## 🖋️ The PAL Editor (GUI Scripting)
Within `op-ed`, developers can now use a visual palette to build PAL scripts. This bridges the gap between raw code and high-level logic.

### 🗺️ The Scratch/RPGMaker Influence
To make development accessible, we are implementing high-level abstractions:
*   **Forever Loops:** For background sensing logic.
*   **Broadcasts:** For cross-piece communication.
*   **Variable Blocks:** For score and health management.

---

## 📝 PAL Program Example: Treasure Hunt
```asm
; treasure_hunt.asm
start:
    OP user::create_profile "hunter"
    sleep 200

    ; Move 5 steps right
    OP user::move_entity "hunter" "right"
    sleep 100
    ; ... repeat 4 more times ...

    ; Check state
    read_state r1, "hunter", "happiness"
    addi r2, r0, 60
    beq r1, r2, treasure_found

    OP playrm::render_map
    halt

treasure_found:
    OP playrm::fuzzpet_action "hunter" "celebrate"
    hit_frame
    halt
```

---

## 🏛️ Scholar's Corner: The "Assembly That Assembled Itself"
In the early days of TPMOS, a developer wrote a PAL script to automate the process of creating new PAL scripts. One day, the developer accidentally pointed the script at itself. Within seconds, there were 10,000 copies of the script filling the disk. This became known as **"The Assembly That Assembled Itself."** It taught us the critical importance of bounded loops and the power (and danger) of self-referential systems in v6.00. 📜♾️

---

## 📝 Study Questions
1.  How does the `OP` instruction in PAL interact with the `ops_catalog.txt`?
2.  What is the benefit of using a PAL script to orchestrate multiple C-based Ops?
3.  Why is `hit_frame` essential at the end of a PAL script?
4.  **True or False:** PAL scripts share memory with the C module that calls them.

---
[Return to Index](INDEX.md)
