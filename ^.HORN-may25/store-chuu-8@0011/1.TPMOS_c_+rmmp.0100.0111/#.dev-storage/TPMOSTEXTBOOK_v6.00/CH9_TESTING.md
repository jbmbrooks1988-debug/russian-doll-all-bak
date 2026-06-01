# 🧪 Chapter 9: Automated Testing of the Ops Pipeline
In a system where every action is a standalone binary (an Op), testing becomes a modular exercise. We don't test "the app"; we test the **Pipeline**. 🧪⚙️

---

## 🚦 The 3-Tier Testing Suite
TPMOS v6.00 enforces a three-tier testing strategy.

### Tier 1: Unit Tests (Op-Specific)
Testing the C code of a single Op.
*   *Tool:* Standard `gcc` and custom assertion headers.
*   *Goal:* Ensure `move_entity.+x` correctly updates `x=1` to `x=2`.

### Tier 2: Integration Tests (Pipeline-Specific)
Testing the flow from Input to State.
*   *Tool:* PAL scripts and `history.txt` injection.
*   *Goal:* Ensure that writing `MOVE:up` to history results in the Op being called and the Mirror being updated.

### Tier 3: Visual Regression (Theater-Specific)
Testing the render output.
*   *Tool:* `frame_buffer` comparison.
*   *Goal:* Ensure the ASCII/GL output matches the expected "Visual Truth."

---

## 💻 Code Example: A PAL Test Script
```asm
; test_move.asm
start:
    ; Set initial state
    OP user::set_state "player" "x=0"
    
    ; Execute move
    OP user::move_entity "player" "right"
    sleep 100
    
    ; Validate state
    read_state r1, "player", "x"
    addi r2, r0, 1
    beq r1, r2, pass
    
fail:
    echo "TEST FAILED: Player X is not 1" > test_results.txt
    halt

pass:
    echo "TEST PASSED" > test_results.txt
    halt
```

---

## 🛡️ Sandbox Execution
Testing is always performed in a **Clone Sandbox**. The system creates a temporary copy of the project folders, runs the tests, and then deletes the clone. This ensures that a failing test never corrupts "Mainframe Reality."

---

## 🏛️ Scholar's Corner: The "Zombie Test Suite"
In v5.01, a bug in the test runner caused it to stop deleting the Clones. Overnight, the system created 50,000 hidden project clones, filling the disk and causing the OS to "hallucinate" old data. These were called **"Zombie Projects."** In v6.00, we now have a **Garbage Collector** that runs every 60 seconds to purge any folder that isn't registered in the active Process Manager. 🧟‍♂️🧹

---

## 📝 Study Questions
1.  Describe the three tiers of the TPMOS testing suite.
2.  Why is "Sandbox Execution" critical for system stability?
3.  How can PAL be used for automated integration testing?
4.  **Exercise:** Write a simple test plan for a "Login" Op.

---
[Return to Index](INDEX.md)
