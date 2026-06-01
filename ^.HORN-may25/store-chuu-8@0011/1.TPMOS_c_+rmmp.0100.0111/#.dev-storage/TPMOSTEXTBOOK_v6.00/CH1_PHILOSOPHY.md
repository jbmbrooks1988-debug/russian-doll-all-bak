# 🧬 Chapter 1: The Soul of a Piece (Standardized Ops Edition)
**The True Piece Method (TPM)** is the governing philosophy of this entire OS. In TPMOS v6.00, we have moved beyond simple object management into **Standardized Ops Architecture**. 🧱

---

## 🏗️ The PMO Hierarchy
The system follows a strict thought priority. If you violate this, the "Mirror" will crack! 🪞

1.  **PIECE (The Atomic Unit / Soul) 🧱**
    *   Everything is a Piece. A button, a player, a map, or a galaxy.
    *   **Rule:** A Piece owns its state *exclusively*.
    *   **File:** `piece.pdl` (DNA) + `state.txt` (Mirror).

2.  **MODULE (The Logic Agent / Brain) 🧠**
    *   The Module is the "Manager". In v6.00, we mandate **Thin Brain** design.
    *   **Mandate:** The Module polls input and delegates logic to **Ops**. It does not perform heavy computation itself.

3.  **OS / CHTPM (The Theater / View) 🎭**
    *   The OS is the stage where Pieces perform. It handles the UI, the layouts, and the "Magic" (variable substitution).
    *   **Standardized Ops:** Every interaction is now routed through a centralized ops registry.

---

## 🧬 Anatomy of Piece DNA (`.pdl`)
Every Piece has a `.pdl` file. This is its blueprint.

```pdl
<piece_id>my_hero</piece_id>
<traits>
    <trait>movable</trait>
    <trait>auditable</trait>
</traits>
<methods>
    <method id="move_north" cmd="user::move_entity north" />
    <method id="say_hello" cmd="echo 'Hello' > last_response.txt" />
</methods>
```

### 🪞 The Mirror (`state.txt`)
While DNA defines the *potential*, the **Mirror** defines the *now*. It is a flat text file for high-speed reading.
*   `x=10`
*   `y=5`
*   `health=100`
*   `status=idle`

> 💡 **Pro Tip:** "If it's not in a file, it's a lie." We never trust memory. In the Standardized Ops Edition, even transient animation states are mirrored to ensure total auditability. 💾

---

## 🫕 Fondu: The Project Lifecycle
Version 6.00 introduces **Fondu**, the project lifecycle manager. Projects are no longer loosely scattered; they are installed, registered, and managed through a centralized system.

*   `fondu --install <project>`: Compiles the source, deploys to `pieces/apps/installed/`, and registers available ops.
*   `fondu --list-ops`: Shows every capability available to the system, from movement to AI inference.

---

## 🧘‍♂️ The Zen of TPM
*   **Data Sovereignty:** No piece touches another's files without permission.
*   **Auditability:** Every major change is logged to the `master_ledger.txt`.
*   **Recursive Reality:** A Piece can contain other Pieces. This is the **Scale-Free Container Model**. 

---

## 🏛️ Scholar's Corner: The "Mirror of Tomokazu"
There is a legendary anecdote among TPMOS engineers known as the **"Mirror of Tomokazu."** Early in development, a rogue developer tried to optimize the system by keeping a player's HP in a global variable instead of writing it to `state.txt`. During a stress test, the process crashed. When the system rebooted, the player had 0 HP and was "permanently dead," despite having been full health seconds before. This disaster led to the founding mandate: **"If it's not in a file, it's a lie."** From that day on, every piece of reality had to be mirrored in a file, ensuring immortality through persistence. 🕯️

---

## 📝 Study Questions
1.  What are the three layers of the PMO hierarchy in v6.00?
2.  What is the role of **Fondu** in the Standardized Ops architecture?
3.  Why is a "Thin Brain" preferred over a "Heavy Brain" in module design?
4.  **True or False:** A galaxy and a button can both be considered "Pieces" in TPMOS.

---
[Return to Index](INDEX.md)
